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
    pthread_mutex_t game_mutex;
    pthread_mutex_t turn_mutex;

    // Existing / server core fields
    int player_count;
    int current_turn;
    int active[MAX_PLAYERS];

    // ===== Hangman fields (your logic needs these) =====
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
