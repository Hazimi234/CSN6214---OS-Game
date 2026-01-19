#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include "shared.h"

// Global pointer so the handler can reach it
shared_state_t *state_ptr;

void cleanup_and_exit(int sig) {
    printf("\nCleaning up shared memory...\n");
    
    // 1. Unmap the memory
    munmap(state_ptr, sizeof(shared_state_t));
    
    // 2. Remove the shared memory object from the OS
    shm_unlink("/hangman_shm");
    
    exit(0);
}

// --- 1. THE HANDLER ---
void handle_sigchld(int sig) {
    // WNOHANG makes sure we don't block the parent if no child is ready to be reaped
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    int shm_fd = shm_open("/hangman_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shared_state_t));

    shared_state_t *state = mmap(NULL, sizeof(shared_state_t), 
                                 PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Mutex Setup
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&state->game_mutex, &attr);
    pthread_mutex_init(&state->turn_mutex, &attr);

    state->player_count = 0;
    state->current_turn = 0;
    
    state_ptr = state; // Assign to global for the handler

    // Register the CTRL+C handler
    signal(SIGINT, cleanup_and_exit);

    pid_t pid = fork();

    if (pid == 0) {
        // Child logic
        sleep(5);
        exit(0);
    } else {
        // Parent logic
        printf("Server running. Press CTRL+C to stop and clean up.\n");
        
        // Instead of while(1) sleep(1), you might check the game state
        while (state->player_count < 2) { 
            printf("Waiting for players... current: %d\n", state->player_count);
            sleep(2);
        }
    }
    return 0;
}