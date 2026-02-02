#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

#define MAX_PLAYERS 5
#define MAX_WORD_LEN 32
#define MAX_ATTEMPTS 1 //need 6 btw
#define MAX_LOG_ENTRIES 100
#define LOG_MSG_LEN 64

typedef enum
{
    GAME_WAITING = 0,
    GAME_RUNNING = 1,
    GAME_ENDED = 2
} game_phase_t;

typedef enum {
    GUESS_INVALID,
    GUESS_DUPLICATE,
    GUESS_HIT,
    GUESS_MISS,
    GUESS_WORD_COMPLETED,
    GUESS_ELIMINATED,
    GUESS_GAME_OVER
} guess_result_t;

typedef struct {
    int player_id;
    char guessed_char;
    guess_result_t result;
    int score_change;
} game_log_entry_t;

typedef struct
{
    // --- Synchronization ---
    pthread_mutex_t game_mutex;
    pthread_mutex_t turn_mutex;
    pthread_cond_t turn_cond;
    pthread_cond_t sched_cond;
    int turn_complete;

    // --- Game Config ---
    int player_count;
    int target_players; // NEW: Set by Host (Player 1)
    int current_turn;
    int active[MAX_PLAYERS];

    // --- SHARED BOARD STATE ---
    char secret_word[MAX_WORD_LEN];
    char revealed[MAX_WORD_LEN];
    int guessed[26];

    // --- INDIVIDUAL STATS ---
    int remaining_attempts[MAX_PLAYERS];
    int scores[MAX_PLAYERS];
    int player_eliminated[MAX_PLAYERS];

    // --- GAME LOG ---
    int log_count;
    game_log_entry_t logs[MAX_LOG_ENTRIES];
    

    game_phase_t phase;

} shared_state_t;

#endif