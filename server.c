#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "shared.h"
#include "game_logic.h"

shared_state_t *state_ptr;

// --- LOG FILE ---
void write_log_to_file(shared_state_t *st) {
    if (st == NULL || st->log_count == 0) {
        printf("[SERVER] No scores to write.\n");
        return;
    }   

    char filename[32];
    int file_num = 1;
    FILE *fp = NULL;

    while (1) {
        sprintf(filename, "scores_%d.txt", file_num);
        if (access(filename, F_OK) != -1) {
            file_num++;
        } else {
            fp = fopen(filename, "w");
            break;
        }
    }

    if (fp == NULL) return;

    fprintf(fp, "=== GAME SESSION LOG ===\n");
    
    char last_word[MAX_WORD_LEN] = ""; // Keep track of word changes

    for (int i = 0; i < st->log_count; i++) {
        // If this move is for a different word than the last move, print a header
        if (strcmp(last_word, st->logs[i].word) != 0) {
            fprintf(fp, "\n[ CURRENT WORD: %s ]\n", st->logs[i].word);
            fprintf(fp, "----------------------\n");
            strncpy(last_word, st->logs[i].word, MAX_WORD_LEN);
        }

        switch (st->logs[i].result) {
            case GUESS_HIT: 
                fprintf(fp, "P%d guessed '%c' -> HIT (+1 Point)\n", st->logs[i].player_id + 1, st->logs[i].guessed_char); break;
            case GUESS_MISS: 
                fprintf(fp, "P%d guessed '%c' -> MISS (-1 Life)\n", st->logs[i].player_id + 1, st->logs[i].guessed_char); break;
            case GUESS_WORD_COMPLETED: 
                fprintf(fp, "P%d guessed '%c' -> WORD COMPLETED (+2 Points)\n", st->logs[i].player_id + 1, st->logs[i].guessed_char); break;
            case GUESS_ELIMINATED: 
                fprintf(fp, "P%d guessed '%c' -> ELIMINATED\n", st->logs[i].player_id + 1, st->logs[i].guessed_char); break;
            case GUESS_DUPLICATE: 
                fprintf(fp, "P%d guessed '%c' -> DUPLICATE GUESS\n", st->logs[i].player_id + 1, st->logs[i].guessed_char); break;
        }
    }

    fprintf(fp, "\n----------------\nFinal Scores:\n");
    for (int i = 0; i < st->player_count; i++) {
        fprintf(fp, "Player %d: %d points\n", i + 1, st->scores[i]);
    }
    
    fclose(fp);
    printf("[SERVER] Game log written to %s\n", filename); // Fixed to show actual filename
}

void cleanup_and_exit(int sig){
    printf("\n[SERVER] Shutting down...\n");
    if (state_ptr) {
        write_log_to_file(state_ptr);
        munmap(state_ptr, sizeof(shared_state_t));
    }
    shm_unlink("/hangman_shm");
    exit(0);
}


// void cleanup_and_exit(int sig) {
//     printf("\n[Server] Shutting down...\n");
//     if (state_ptr) munmap(state_ptr, sizeof(shared_state_t));
//     shm_unlink("/hangman_shm");
//     exit(0);
// }

// --- SCHEDULER THREAD ---
void *scheduler_thread(void *arg)
{
    shared_state_t *st = (shared_state_t *)arg;
    
    printf("[Scheduler] Thread active. Waiting for GAME_RUNNING phase...\n");
    while (st->phase == GAME_WAITING) sleep(1);

    printf("[Scheduler] Game started! Managing turns...\n");

    while (st->phase != GAME_ENDED)
    {
        pthread_mutex_lock(&st->turn_mutex);

        while (st->turn_complete == 0 && st->phase != GAME_ENDED) {
            pthread_cond_wait(&st->sched_cond, &st->turn_mutex);
        }

        int alive_count = 0;
        for(int i=0; i<st->player_count; i++) {
            if(st->active[i] && !st->player_eliminated[i]) alive_count++;
        }

        if (alive_count == 0) {
            printf("\n\n=== GAME OVER ===\n");
            printf("Final Word was: %s\n", st->secret_word);
            
            int max_score = -1;
            for (int i = 0; i < st->player_count; i++) {
                if (st->scores[i] > max_score) max_score = st->scores[i];
            }

            printf("\n--- FINAL SCOREBOARD ---\n");
            int winner_count = 0;
            int winners[MAX_PLAYERS];
            
            for (int i = 0; i < st->player_count; i++) {
                printf("Player %d: %d points\n", i + 1, st->scores[i]);
                if (st->scores[i] == max_score) {
                    winners[winner_count++] = i;
                }
            }
            printf("------------------------\n");

            if (winner_count > 1) {
                printf(">>> TIE GAME! Winners: ");
                for(int i=0; i<winner_count; i++) printf("P%d ", winners[i] + 1);
                printf("<<<\n");
            } else {
                printf(">>> PLAYER %d WINS! <<<\n", winners[0] + 1);
            }

            st->phase = GAME_ENDED;
            pthread_cond_broadcast(&st->turn_cond);
            pthread_mutex_unlock(&st->turn_mutex);
            break;
        }

        int found_next = 0;
        int attempts = 0;
        int next_id = st->current_turn;

        do {
            next_id = (next_id + 1) % MAX_PLAYERS;
            attempts++;
            if (st->active[next_id] && !st->player_eliminated[next_id]) {
                found_next = 1;
            }
        } while (!found_next && attempts <= MAX_PLAYERS);

        if (found_next) {
            st->current_turn = next_id;
            printf("[Scheduler] Next turn: Player %d\n", next_id + 1);
        }

        st->turn_complete = 0;
        pthread_cond_broadcast(&st->turn_cond);
        pthread_mutex_unlock(&st->turn_mutex);
    }
    return NULL;
}



int main()
{
    srand(time(NULL));
    shm_unlink("/hangman_shm"); // Force clear old memory

    int shm_fd = shm_open("/hangman_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shared_state_t));
    shared_state_t *state = mmap(NULL, sizeof(shared_state_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Sync Init
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&state->game_mutex, &mattr);
    pthread_mutex_init(&state->turn_mutex, &mattr);
    
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&state->turn_cond, &cattr);
    pthread_cond_init(&state->sched_cond, &cattr);

    // Config Init
    state->player_count = 0;
    state->target_players = 0; 
    state->current_turn = 0;
    state->turn_complete = 0;
    state->log_count = 0; /// logging ///
    state->phase = GAME_WAITING;
    memset(state->active, 0, sizeof(state->active));
    memset(state->player_eliminated, 0, sizeof(state->player_eliminated));
    memset(state->scores, 0, sizeof(state->scores));

    state_ptr = state;
    signal(SIGINT, cleanup_and_exit);

    pthread_t sched_tid;
    pthread_create(&sched_tid, NULL, scheduler_thread, (void *)state);

    printf("[Server] Lobby Open. Waiting for Host (Player 1) to join...\n");
    game_setup_round(state, get_random_word());

    // 1. Wait until Host sets the target
    // We use (volatile int*) to force the compiler to check memory every loop
    while(*(volatile int*)&state->target_players == 0) {
        sleep(1);
    }
    printf("[Server] Host set lobby size to %d. Waiting for players...\n", state->target_players);

    // 2. Wait until player count matches target
    while(*(volatile int*)&state->player_count < *(volatile int*)&state->target_players) {
        printf("[Server] Players connected: %d/%d\n", state->player_count, state->target_players);
        sleep(1);
    }
    
    printf("[Server] Lobby Full (%d/%d)! Starting game in 3 seconds...\n", state->player_count, state->target_players);
    fflush(stdout); // Force print
    sleep(3); 
    
    printf("[Server] GO! Setting phase to GAME_RUNNING.\n");
    state->phase = GAME_RUNNING;
    
    // Broadcast twice just to be safe
    pthread_cond_broadcast(&state->turn_cond);
    usleep(100000);
    pthread_cond_broadcast(&state->turn_cond);

    pthread_join(sched_tid, NULL);
    cleanup_and_exit(0);
    return 0;
}