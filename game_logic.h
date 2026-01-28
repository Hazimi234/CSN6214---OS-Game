#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "shared.h"

typedef enum
{
    GUESS_INVALID = 0,
    GUESS_NOT_YOUR_TURN,
    GUESS_GAME_ENDED,
    GUESS_DUPLICATE,
    GUESS_HIT,
    GUESS_MISS,
    GUESS_WIN,
    GUESS_LOSE
} guess_result_t;

// Start a new game
void game_start(shared_state_t *st, const char *word);

// Apply a guess (Option A: invalid/duplicate do NOT consume turn)
guess_result_t game_apply_guess(shared_state_t *st, int player_id, const char *input);

// Helpers
int game_is_won(const shared_state_t *st);
int game_is_lost(const shared_state_t *st);
int result_consumes_turn(guess_result_t r);

#endif
