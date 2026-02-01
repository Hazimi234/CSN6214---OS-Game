#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "shared.h"

// Returns a random word from the internal library
const char* get_random_word();

// Setup a new word on the board (resets guessed letters)
void game_setup_round(shared_state_t *st, const char *word);

// Apply a guess (updates board, scores, lives)
guess_result_t game_apply_guess(shared_state_t *st, int player_id, const char *input);

#endif