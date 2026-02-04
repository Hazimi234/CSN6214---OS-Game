// Purpose: Prototypes for game rule logic.

#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "shared.h"

// Returns a random word from the internal hardcoded library
const char* get_random_word();

// Initializes a new round: sets secret word, resets guessed letters
void game_setup_round(shared_state_t *st, const char *word);

// Processes a player's guess:
// 1. Checks validity (A-Z)
// 2. Checks if duplicate
// 3. Updates board (Hit/Miss)
// 4. Updates Score and Lives
// 5. Logs the action
guess_result_t game_apply_guess(shared_state_t *st, int player_id, const char *input);

#endif