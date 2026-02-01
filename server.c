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
// Runs in Parent. Waits for turn_complete, then advances to next active player.
void *scheduler_thread(void *arg)
{
    shared_state_t *st = (shared_state_t *)arg;
    printf("[Scheduler] Thread started. Waiting for game to begin...\n");

    // Wait for the game to actually start
    while (st->phase == GAME_WAITING) {
        sleep(1);
    }

    while (1)
    {
        // 1. Lock turn mutex to wait for signal
        pthread_mutex_lock(&st->turn_mutex);

        // 2. Wait until the current player says "I'm done"
        //    (We also wake up if the game ends)
        while (st->turn_complete == 0 && st->phase != GAME_ENDED)
        {
            pthread_cond_wait(&st->sched_cond, &st->turn_mutex);
        }

        if (st->phase == GAME_ENDED)
        {
            pthread_mutex_unlock(&st->turn_mutex);
            printf("[Scheduler] Game ended. Scheduler stopping.\n");
            // Broadcast one last time to wake up any stuck clients so they can exit
            pthread_cond_broadcast(&st->turn_cond); 
            break;
        }

        // 3. Round Robin Logic: Find next active player
        int found_next = 0;
        int attempts = 0;
        int next_id = st->current_turn;

        // Cycle through IDs until we find an active player
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
        } else {
            printf("[Scheduler] WARNING: No active players found!\n");
        }

        // 4. Reset the flag and notify all clients
        st->turn_complete = 0;
        pthread_cond_broadcast(&st->turn_cond); // Wake up clients to check "Is it my turn?"

        // 5. Unlock
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

    // 2. Initialize Synchronization Primitives (PART 2)
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED); // Required for fork()

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED); // Required for fork()

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

    // 3. Start the Scheduler Thread (PART 2)
    pthread_t sched_tid;
    if (pthread_create(&sched_tid, NULL, scheduler_thread, (void *)state) != 0) {
        perror("Failed to create scheduler thread");
        exit(1);
    }

    printf("[Server] Running. Waiting for connections...\n");

    // SIMULATION: In a real server, you would accept() loop here.
    // We will simulate connecting players by forking.
    int my_player_id = 0; 
    
    // Example: Fork 3 players
    while (state->player_count < 3) 
    {
        // In real code, you assign ID based on the slot you found
        // For this demo, we assume sequential connection
        pthread_mutex_lock(&state->game_mutex);
        int new_id = state->player_count;
        state->player_count++;
        state->active[new_id] = 1;
        pthread_mutex_unlock(&state->game_mutex);

        pid_t pid = fork();

        if (pid == 0)
        {
            // --- PART 3: CHILD PROCESS LOGIC ---
            // This represents the Player Handler
            printf("[Child %d] Connected.\n", new_id);
            
            // Wait for game start
            while(state->phase == GAME_WAITING) sleep(1);

            while (state->phase == GAME_RUNNING)
            {
                // A. Wait for Turn
                pthread_mutex_lock(&state->turn_mutex);
                while (state->current_turn != new_id && state->phase == GAME_RUNNING)
                {
                    pthread_cond_wait(&state->turn_cond, &state->turn_mutex);
                }

                if (state->phase != GAME_RUNNING) {
                    pthread_mutex_unlock(&state->turn_mutex);
                    break;
                }
                
                printf("[Child %d] It is my turn! Processing move...\n", new_id);
                pthread_mutex_unlock(&state->turn_mutex);

                // B. Perform Game Logic (Critical Section if touching board)
                pthread_mutex_lock(&state->game_mutex);
                // ... Call game_apply_guess() here ...
                // ... For simulation, we sleep ...
                sleep(1); 
                printf("[Child %d] Move complete.\n", new_id);
                pthread_mutex_unlock(&state->game_mutex);

                // C. Signal Completion to Scheduler
                pthread_mutex_lock(&state->turn_mutex);
                state->turn_complete = 1;
                pthread_cond_signal(&state->sched_cond); // Wake up scheduler
                pthread_mutex_unlock(&state->turn_mutex);
            }
            
            printf("[Child %d] Exiting.\n", new_id);
            exit(0);
        }
        
        sleep(1); // Small delay between player joins for demo
    }

    // Start the game once we have players
    printf("[Server] Starting game...\n");
    game_start(state, "system"); // Using "system" as secret word for test
    state->phase = GAME_RUNNING;
    
    // Wait for the scheduler to finish (which happens when game ends)
    pthread_join(sched_tid, NULL);
    
    cleanup_and_exit(0);
    return 0;
}