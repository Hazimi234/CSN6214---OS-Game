#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include "shared.h"
#include "game_logic.h"

// Global pointer so the handler can reach it
shared_state_t *state_ptr;

void cleanup_and_exit(int sig)
{
    printf("\n[Server] Cleaning up shared memory...\n");
    if (state_ptr) {
        munmap(state_ptr, sizeof(shared_state_t));
    }
    shm_unlink("/hangman_shm");
    exit(0);
}

void handle_sigchld(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// --- PART 2: SCHEDULER THREAD ---
void *scheduler_thread(void *arg)
{
    shared_state_t *st = (shared_state_t *)arg;
    printf("[Scheduler] Thread started. Waiting for game to begin...\n");

    while (st->phase == GAME_WAITING) {
        sleep(1);
    }

    while (1)
    {
        pthread_mutex_lock(&st->turn_mutex);

        // Wait until the current player says "I'm done"
        while (st->turn_complete == 0 && st->phase != GAME_ENDED)
        {
            pthread_cond_wait(&st->sched_cond, &st->turn_mutex);
        }

        if (st->phase == GAME_ENDED)
        {
            pthread_mutex_unlock(&st->turn_mutex);
            printf("[Scheduler] Game ended. Scheduler stopping.\n");
            pthread_cond_broadcast(&st->turn_cond); 
            break;
        }

        // Round Robin Logic
        int found_next = 0;
        int attempts = 0;
        int next_id = st->current_turn;

        do {
            next_id = (next_id + 1) % MAX_PLAYERS;
            attempts++;
            if (st->active[next_id] == 1) {
                found_next = 1;
            }
        } while (!found_next && attempts <= MAX_PLAYERS);

        if (found_next) {
            st->current_turn = next_id;
            printf("[Scheduler] Turn advanced to Player %d\n", st->current_turn);
        }

        // Reset flag and notify clients
        st->turn_complete = 0;
        pthread_cond_broadcast(&st->turn_cond); 

        pthread_mutex_unlock(&st->turn_mutex);
    }
    return NULL;
}

int main()
{
    // 1. Setup Shared Memory
    int shm_fd = shm_open("/hangman_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shared_state_t));
    shared_state_t *state = mmap(NULL, sizeof(shared_state_t),
                                 PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 2. Initialize Synchronization
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&state->game_mutex, &mattr);
    pthread_mutex_init(&state->turn_mutex, &mattr);
    pthread_cond_init(&state->turn_cond, &cattr);
    pthread_cond_init(&state->sched_cond, &cattr);

    // Initial State
    state->player_count = 0;
    state->current_turn = 0;
    state->turn_complete = 0;
    state->phase = GAME_WAITING;
    memset(state->active, 0, sizeof(state->active));

    state_ptr = state;
    signal(SIGINT, cleanup_and_exit);
    signal(SIGCHLD, handle_sigchld);

    // 3. Start Scheduler
    pthread_t sched_tid;
    pthread_create(&sched_tid, NULL, scheduler_thread, (void *)state);

    printf("[Server] Running.\n");
    
    // --- ASK FOR NUMBER OF PLAYERS ---
    int target_players = 0;
    while (target_players < 3 || target_players > 5) {
        printf("Enter number of players (3-5): ");
        scanf("%d", &target_players);
    }
    printf("Waiting for %d players to join...\n", target_players);

    // Fork players
    while (state->player_count < target_players) 
    {
        pthread_mutex_lock(&state->game_mutex);
        int new_id = state->player_count;
        state->player_count++;
        state->active[new_id] = 1;
        pthread_mutex_unlock(&state->game_mutex);

        pid_t pid = fork();

        if (pid == 0)
        {
            // --- CHILD PROCESS (PLAYER) ---
            printf("[Child %d] Connected.\n", new_id);
            
            while(state->phase == GAME_WAITING) sleep(1);

            while (state->phase == GAME_RUNNING)
            {
                // A. Wait for Turn
                pthread_mutex_lock(&state->turn_mutex);
                
                // FIXED: Also wait if turn_complete is 1 (Scheduler hasn't updated yet)
                while ((state->current_turn != new_id || state->turn_complete == 1) 
                       && state->phase == GAME_RUNNING)
                {
                    pthread_cond_wait(&state->turn_cond, &state->turn_mutex);
                }

                if (state->phase != GAME_RUNNING) {
                    pthread_mutex_unlock(&state->turn_mutex);
                    break;
                }
                
                printf("\n--- [Player %d] YOUR TURN ---\n", new_id);
                pthread_mutex_unlock(&state->turn_mutex);

                // B. Perform Game Logic (Playable)
                pthread_mutex_lock(&state->game_mutex);
                
                printf("Word: %s  (Attempts: %d)\n", state->revealed, state->remaining_attempts);
                printf("Enter a letter guess: ");
                
                char guess;
                // ' ' before %c eats newlines from previous inputs
                scanf(" %c", &guess); 
                
                game_apply_guess(state, new_id, &guess);
                printf("You guessed: %c\n", guess);
                
                pthread_mutex_unlock(&state->game_mutex);

                // C. Signal Completion
                pthread_mutex_lock(&state->turn_mutex);
                state->turn_complete = 1;
                pthread_cond_signal(&state->sched_cond);
                pthread_mutex_unlock(&state->turn_mutex);
            }
            
            printf("[Child %d] Game Over. Exiting.\n", new_id);
            exit(0);
        }
    }

    printf("[Server] All players connected. Starting game...\n");
    game_start(state, "operating"); // Secret word is "operating"
    state->phase = GAME_RUNNING;
    pthread_cond_broadcast(&state->turn_cond); // Wake up waiting children
    
    pthread_join(sched_tid, NULL);
    cleanup_and_exit(0);
    return 0;
}