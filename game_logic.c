#include "game_logic.h"
#include <ctype.h>
#include <string.h>

// Convert input to letter index 0–25, or -1 if invalid
static int normalize_letter(const char *input)
{
    if (!input)
        return -1;
    if (strlen(input) != 1)
        return -1;

    unsigned char c = (unsigned char)input[0];
    if (!isalpha(c))
        return -1;

    return tolower(c) - 'a';
}

// Start a new game
void game_start(shared_state_t *st, const char *word)
{
    memset(st->guessed, 0, sizeof(st->guessed));

    st->phase = GAME_RUNNING;
    st->remaining_attempts = MAX_ATTEMPTS;
    st->winner_id = -1;
    st->last_player_id = -1;
    st->last_guess = '\0';

    strncpy(st->secret_word, word, MAX_WORD_LEN - 1);
    st->secret_word[MAX_WORD_LEN - 1] = '\0';

    st->word_len = (int)strlen(st->secret_word);

    for (int i = 0; i < st->word_len; i++)
    {
        st->revealed[i] = '_';
    }
    st->revealed[st->word_len] = '\0';
}

int game_is_won(const shared_state_t *st)
{
    return strcmp(st->revealed, st->secret_word) == 0;
}

int game_is_lost(const shared_state_t *st)
{
    return st->remaining_attempts <= 0;
}

// OPTION A: only real guesses consume the turn
int result_consumes_turn(guess_result_t r)
{
    return (r == GUESS_HIT ||
            r == GUESS_MISS ||
            r == GUESS_WIN ||
            r == GUESS_LOSE);
}

// Apply a guess
guess_result_t game_apply_guess(shared_state_t *st, int player_id, const char *input)
{
    if (st->phase != GAME_RUNNING)
        return GUESS_GAME_ENDED;

    if (player_id != st->current_turn)
        return GUESS_NOT_YOUR_TURN;

    int idx = normalize_letter(input);
    if (idx < 0 || idx > 25)
        return GUESS_INVALID;

    // Duplicate guess
    if (st->guessed[idx])
        return GUESS_DUPLICATE;

    st->guessed[idx] = 1;
    st->last_player_id = player_id;
    st->last_guess = (char)('a' + idx);

    int hit = 0;
    for (int i = 0; i < st->word_len; i++)
    {
        if (st->secret_word[i] == st->last_guess)
        {
            st->revealed[i] = st->last_guess;
            hit = 1;
        }
    }

    // Wrong guess
    if (!hit)
    {
        st->remaining_attempts--;
        if (game_is_lost(st))
        {
            st->phase = GAME_ENDED;
            st->winner_id = -1;
            return GUESS_LOSE;
        }
        return GUESS_MISS;
    }

    // Correct guess
    if (game_is_won(st))
    {
        st->phase = GAME_ENDED;
        st->winner_id = player_id;
        return GUESS_WIN;
    }

    return GUESS_HIT;
}
