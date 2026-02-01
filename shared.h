#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

#define MAX_PLAYERS 5
#define MAX_WORD_LEN 32
#define MAX_ATTEMPTS 6

typedef enum
{
    GAME_WAITING = 0,
    GAME_RUNNING = 1,
    GAME_ENDED = 2
} game_phase_t;

typedef struct
{
    // --- Synchronization Primitives (REQUIRED for Part 2 & 3) ---
    pthread_mutex_t game_mutex;  // Protects game board/state
    pthread_mutex_t turn_mutex;  // Protects turn variables
    
    pthread_cond_t turn_cond;    // Signal sent TO Clients ("It is your turn")
    pthread_cond_t sched_cond;   // Signal sent TO Scheduler ("Move finished")

    int turn_complete;           // Flag: 1 = Current player finished move

    // --- Server Core Fields ---
    int player_count;
    int current_turn;            // ID of the player whose turn it is
    int active[MAX_PLAYERS];     // 1 = Active, 0 = Inactive/Disconnected

    // --- Hangman Game State ---
    game_phase_t phase;
    char secret_word[MAX_WORD_LEN];
    int word_len;
    char revealed[MAX_WORD_LEN];
    int remaining_attempts;
    int guessed[26];
    int winner_id;
    int last_player_id;
    char last_guess;

} shared_state_t;

#endif