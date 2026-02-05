/*
 * * Purpose: Client Process (Player).
 * Responsibilities:
 * 1. Connect to existing Shared Memory.
 * 2. Handle Lobby Logic (Host sets player count).
 * 3. Game Loop: Wait for Turn -> Input Guess -> Signal Server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include "shared.h"
#include "game_logic.h"

shared_state_t *state;

// Helper: Visual Hangman
void print_hangman(int lives) {
    printf("\n");
    printf("  +---+\n");
    printf("  |   |\n");
    if (lives <= 5) printf("  O   |\n"); else printf("      |\n");
    if (lives <= 2) printf(" /|\\  |\n"); 
    else if (lives == 3) printf(" /|   |\n"); 
    else if (lives == 4) printf("  |   |\n"); else printf("      |\n");
    if (lives <= 0) printf(" / \\  |\n"); 
    else if (lives == 1) printf(" /    |\n"); else printf("      |\n");
    printf("=========\n");
}

void cleanup_handler(int sig) {
    printf("\nExiting...\n");
    exit(0);
}

int main() {
    srand(time(NULL) ^ getpid());

    // 1. Connect to Shared Memory
    int shm_fd = shm_open("/hangman_shm", O_RDWR, 0666);
    if (shm_fd == -1) {
        printf("Error: Could not connect to server. Run './server' first.\n");
        exit(1);
    }
    state = mmap(NULL, sizeof(shared_state_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // 2. Join Lobby
    pthread_mutex_lock(&state->game_mutex);
    
    // Logic: Check if Game Full
    if (state->target_players > 0 && state->player_count >= state->target_players) {
        printf("Game is full (%d/%d)!\n", state->player_count, state->target_players);
        pthread_mutex_unlock(&state->game_mutex);
        exit(0);
    }

    // Assign ID
    int my_id = state->player_count;
    state->player_count++;
    state->active[my_id] = 1;
    state->remaining_attempts[my_id] = MAX_ATTEMPTS;
    state->scores[my_id] = 0;
    state->player_eliminated[my_id] = 0;
    
    // --- HOST LOGIC (First Player) ---
    if (my_id == 0) {
        printf(">>> YOU ARE THE HOST (PLAYER 1) <<<\n");
        int target = 0;
        while (target < 3 || target > 5) {
            printf("Enter total number of players (3-5): ");
            scanf("%d", &target);
        }
        state->target_players = target;
        printf("Lobby set to %d players. Waiting for others...\n", target);
    } 
    else {
        printf("Connected as PLAYER %d\n", my_id + 1);
        
        // Wait for Host configuration
        if (state->target_players == 0) {
            printf("Waiting for Host to configure the game...\n");
            pthread_mutex_unlock(&state->game_mutex);
            while(state->target_players == 0) sleep(1);
            pthread_mutex_lock(&state->game_mutex);
        }
    }
    
    pthread_mutex_unlock(&state->game_mutex);
    signal(SIGINT, cleanup_handler);

    // 3. Lobby Wait Loop
    printf("Waiting for players... (%d/%d connected)\n", state->player_count, state->target_players);
    while (state->player_count < state->target_players) {
        sleep(1);
    }

    // Countdown
    printf("\nLobby Full! Starting in...\n");
    for(int i=3; i>0; i--) { printf("%d...\n", i); sleep(1); }
    printf("GO!\n");

    // Wait for Server sync
    while (state->phase == GAME_WAITING) usleep(100000);

    // 4. MAIN GAME LOOP
    while (state->phase == GAME_RUNNING) {
        
        // A. WAIT FOR TURN (Synchronization Point)
        pthread_mutex_lock(&state->turn_mutex);
        while ((state->current_turn != my_id || state->turn_complete) && state->phase == GAME_RUNNING) {
            pthread_cond_wait(&state->turn_cond, &state->turn_mutex);
        }

        // Check if game ended or player died
        if (state->phase != GAME_RUNNING || state->player_eliminated[my_id]) {
            pthread_mutex_unlock(&state->turn_mutex);
            break;
        }
        
        printf("\n\n================ YOUR TURN (PLAYER %d) ================\n", my_id + 1);
        pthread_mutex_unlock(&state->turn_mutex);

        // B. PLAY MOVE
        int turn_over = 0;
        while (!turn_over && state->phase == GAME_RUNNING) {
            pthread_mutex_lock(&state->game_mutex);
            
            // Display Board
            printf("\nWORD: %s\n", state->revealed);
            printf("USED: [ ");
            for(int i = 0; i < 26; i++) if(state->guessed[i]) printf("%c ", 'A' + i);
            printf("]\n");
            printf("STATS: Score = %d  |  Lives = %d\n", state->scores[my_id], state->remaining_attempts[my_id]);
            
            // Get Input
            char input_buffer[100];
            printf("Guess a letter: ");
            if (scanf("%s", input_buffer) != 1) {
                pthread_mutex_unlock(&state->game_mutex);
                continue;
            }

            // Validation
            if (strlen(input_buffer) > 1 || !isalpha(input_buffer[0])) {
                printf("\n>>> INVALID INPUT! Single letters only. <<<\n");
                pthread_mutex_unlock(&state->game_mutex);
                continue;
            }

            // Apply Guess
            char g_str[2] = {input_buffer[0], '\0'};
            guess_result_t res = game_apply_guess(state, my_id, g_str);

            if (res == GUESS_DUPLICATE) {
                printf("\n>>> ALREADY USED! Try again. <<<\n");
                pthread_mutex_unlock(&state->game_mutex);
                continue; // Loop again, don't end turn
            }

            // Handle Result
            if (res == GUESS_WORD_COMPLETED) {
                printf("\n>>> CORRECT! +2 Points! New Word Coming... <<<\n");
                game_setup_round(state, get_random_word());
            } 
            else if (res == GUESS_ELIMINATED) {
                print_hangman(state->remaining_attempts[my_id]);
                printf("\n>>> YOU DIED! Game Over for you. <<<\n");
            } 
            else if (res == GUESS_HIT) {
                printf("\nResult: HIT! (+1 Point)\n");
            } 
            else if (res == GUESS_MISS) {
                printf("\nResult: MISS! (-1 Life)\n");
                print_hangman(state->remaining_attempts[my_id]);
            }

            turn_over = 1; // Valid move made, end loop
            pthread_mutex_unlock(&state->game_mutex);
        }

        // C. SIGNAL COMPLETION
        pthread_mutex_lock(&state->turn_mutex);
        state->turn_complete = 1;
        pthread_cond_signal(&state->sched_cond); // Tell Scheduler "I'm done"
        pthread_mutex_unlock(&state->turn_mutex);
    }

    printf("Game ended. Thanks for playing!\n");
    return 0;
}