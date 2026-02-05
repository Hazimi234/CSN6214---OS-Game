/*
 * * Purpose: Main Server Process.
 * Responsibilities:
 * 1. Create and Initialize Shared Memory.
 * 2. Run the Scheduler Thread (Round Robin Turn Management).
 * 3. Wait for Lobby to fill.
 * 4. Write final scores to log file on exit.
 */

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

// FILE I/O: LOGGING
void write_log_to_file(shared_state_t *st) {
    if (st == NULL || st->log_count == 0) {
        printf("[SERVER] No scores to write.\n"); //nothing to log
        return;
    }   

    // Generate unique filename (scores_1.txt, scores_2.txt...)
    char filename[32];
    int file_num = 1;
    FILE *fp = NULL;

    while (1) {
        sprintf(filename, "scores_%d.txt", file_num);
        if (access(filename, F_OK) != -1) {
            file_num++; // File exists, try next number
        } else {
            fp = fopen(filename, "w"); // Create new file
            break;
        }
    }

    if (fp == NULL) return;

    fprintf(fp, "=== GAME SESSION LOG ===\n");
    
    char last_word[MAX_WORD_LEN] = ""; 

    // Write Log History
    for (int i = 0; i < st->log_count; i++) {
        // Header for new words
        if (strcmp(last_word, st->logs[i].word) != 0) {
            fprintf(fp, "\n[ CURRENT WORD: %s ]\n", st->logs[i].word);
            fprintf(fp, "----------------------\n");
            strncpy(last_word, st->logs[i].word, MAX_WORD_LEN);
        }
        
        // Detailed Event Log
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
            default: break;
        }
    }

    // Write Final Scoreboard
    fprintf(fp, "\n----------------\nFinal Scores:\n");
    
    // Find Max Score
    int max_score = -1;
    for (int i = 0; i < st->player_count; i++) {
        if (st->scores[i] > max_score) max_score = st->scores[i];
    }

    // Print Individual Scores
    for (int i = 0; i < st->player_count; i++) {
        fprintf(fp, "Player %d: %d points\n", i + 1, st->scores[i]);
    }
    fprintf(fp, "----------------\n");

    // Determine Winner(s)
    int winner_count = 0;
    int winners[MAX_PLAYERS];
    for (int i = 0; i < st->player_count; i++) {
        if (st->scores[i] == max_score) winners[winner_count++] = i;
    }

    if (winner_count > 1) {
        fprintf(fp, "RESULT: TIE GAME between Players: ");
        for(int i = 0; i < winner_count; i++) fprintf(fp, "P%d ", winners[i] + 1);
        fprintf(fp, "with %d points\n", max_score);
    } else if (winner_count == 1) {
        fprintf(fp, "RESULT: Winner is Player %d with %d points\n", winners[0] + 1, max_score);
    }
    
    fclose(fp); // Close the file
    printf("[SERVER] Game log written to %s\n", filename); // Notify log written
}

// Cleanup Handler (CTRL+C)
void cleanup_and_exit(int sig){
    printf("\n[SERVER] Shutting down...\n");
    if (state_ptr) {
        write_log_to_file(state_ptr);
        munmap(state_ptr, sizeof(shared_state_t));
    }
    shm_unlink("/hangman_shm");
    exit(0);
}

// THREAD: SCHEDULER
void *scheduler_thread(void *arg)
{
    shared_state_t *st = (shared_state_t *)arg;
    
    printf("[Scheduler] Thread active. Waiting for GAME_RUNNING phase...\n");
    while (st->phase == GAME_WAITING) sleep(1);

    printf("[Scheduler] Game started! Managing turns...\n");

    while (st->phase != GAME_ENDED)
    {
        // 1. Wait for signal from current player
        pthread_mutex_lock(&st->turn_mutex);
        while (st->turn_complete == 0 && st->phase != GAME_ENDED) {
            pthread_cond_wait(&st->sched_cond, &st->turn_mutex);
        }

        // 2. Check if Game Over (All players dead)
        int alive_count = 0;
        for(int i=0; i<st->player_count; i++) {
            if(st->active[i] && !st->player_eliminated[i]) alive_count++;
        }

        if (alive_count == 0) {
            // Game Over Sequence
            printf("\n\n=== GAME OVER ===\n");
            
            // Determine Winner logic for Console Output
            int max_score = -1;
            for (int i = 0; i < st->player_count; i++) if (st->scores[i] > max_score) max_score = st->scores[i];

            printf("\n--- FINAL SCOREBOARD ---\n");
            int winner_count = 0;
            int winners[MAX_PLAYERS];
            for (int i = 0; i < st->player_count; i++) {
                printf("Player %d: %d points\n", i + 1, st->scores[i]);
                if (st->scores[i] == max_score) winners[winner_count++] = i;
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
            pthread_cond_broadcast(&st->turn_cond); // Wake up clients so they exit
            pthread_mutex_unlock(&st->turn_mutex);
            break;
        }

        // 3. Round Robin Logic: Find next active, living player
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

        // 4. Notify Clients
        st->turn_complete = 0;
        pthread_cond_broadcast(&st->turn_cond); // Wake up all clients to check turn
        pthread_mutex_unlock(&st->turn_mutex);
    }
    return NULL;
}

// MAIN SERVER PROCESS
int main()
{
    srand(time(NULL));
    shm_unlink("/hangman_shm"); // Force clear old memory

    // 1. Create Shared Memory Object
    int shm_fd = shm_open("/hangman_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shared_state_t));
    shared_state_t *state = mmap(NULL, sizeof(shared_state_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 2. Initialize Synchronization (Process Shared)
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

    // 3. Initialize Game State
    state->player_count = 0;
    state->target_players = 0; 
    state->current_turn = 0;
    state->turn_complete = 0;
    state->log_count = 0; 
    state->phase = GAME_WAITING;
    memset(state->active, 0, sizeof(state->active));
    memset(state->player_eliminated, 0, sizeof(state->player_eliminated));
    memset(state->scores, 0, sizeof(state->scores));

    state_ptr = state;
    signal(SIGINT, cleanup_and_exit);

    // 4. Start Scheduler Thread
    pthread_t sched_tid;
    pthread_create(&sched_tid, NULL, scheduler_thread, (void *)state);

    printf("[Server] Lobby Open. Waiting for Host (Player 1) to join...\n");
    game_setup_round(state, get_random_word());

    // 5. Wait for Host Config
    // Use (volatile int*) to force compiler to read memory fresh every loop
    while(*(volatile int*)&state->target_players == 0) {
        sleep(1);
    }
    printf("[Server] Host set lobby size to %d. Waiting for players...\n", state->target_players);

    // 6. Wait for Players
    while(*(volatile int*)&state->player_count < *(volatile int*)&state->target_players) {
        printf("[Server] Players connected: %d/%d\n", state->player_count, state->target_players);
        sleep(1);
    }
    
    // 7. Start Game
    printf("[Server] Lobby Full (%d/%d)! Starting game in 3 seconds...\n", state->player_count, state->target_players);
    fflush(stdout); 
    sleep(3); 
    
    printf("[Server] GO! Setting phase to GAME_RUNNING.\n");
    state->phase = GAME_RUNNING;
    
    // Broadcast signal multiple times to ensure clients wake up
    pthread_cond_broadcast(&state->turn_cond);
    usleep(100000);
    pthread_cond_broadcast(&state->turn_cond);

    pthread_join(sched_tid, NULL);
    cleanup_and_exit(0);
    return 0;
}