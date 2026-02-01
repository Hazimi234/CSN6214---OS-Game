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

typedef enum {
    GUESS_INVALID,
    GUESS_DUPLICATE,
    GUESS_HIT,
    GUESS_MISS,
    GUESS_WORD_COMPLETED,
    GUESS_ELIMINATED,
    GUESS_GAME_OVER
} guess_result_t;

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

    game_phase_t phase;

} shared_state_t;

#endif