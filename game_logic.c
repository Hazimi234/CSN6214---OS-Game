#include "game_logic.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// --- WORD BANK ---
const char *WORD_BANK[] = {
    "apple", "beach", "brain", "bread", "brush", "chair", "chest", "chord", 
    "click", "clock", "cloud", "dance", "diary", "drink", "drive", "earth", 
    "feast", "field", "fruit", "glass", "grape", "green", "ghost", "guide", 
    "heart", "horse", "house", "juice", "light", "lemon", "melon", "money", 
    "music", "night", "party", "piano", "pilot", "phone", "plane", "plate", 
    "radio", "river", "robot", "shirt", "shoes", "smile", "space", "spoon", 
    "store", "storm", "sugar", "sweet", "table", "tiger", "toast", "tower", 
    "track", "trade", "train", "truck", "uncle", "video", "voice", "waste", 
    "watch", "water", "whale", "white", "woman", "world", "write", "youth"
};
#define BANK_SIZE 67

const char* get_random_word() {
    int r = rand() % BANK_SIZE;
    return WORD_BANK[r];
}

static int normalize_letter(const char *input)
{
    if (!input || strlen(input) < 1) return -1;
    unsigned char c = (unsigned char)input[0];
    if (!isalpha(c)) return -1;
    return tolower(c) - 'a';
}

void game_setup_round(shared_state_t *st, const char *word)
{
    // Reset the board, but NOT scores or health
    memset(st->guessed, 0, sizeof(st->guessed));
    
    strncpy(st->secret_word, word, MAX_WORD_LEN - 1);
    st->secret_word[MAX_WORD_LEN - 1] = '\0';

    int len = strlen(st->secret_word);
    for (int i = 0; i < len; i++) {
        st->revealed[i] = '_';
    }
    st->revealed[len] = '\0';
}

guess_result_t game_apply_guess(shared_state_t *st, int player_id, const char *input)
{
    int idx = normalize_letter(input);
    if (idx < 0 || idx > 25) return GUESS_INVALID;

    char guess_char = (char)('a' + idx);
    guess_result_t res;

    // Check SHARED history
    if (st->guessed[idx]) {
        res= GUESS_DUPLICATE;
        return res;
    }
    // Mark as guessed for EVERYONE
    st->guessed[idx] = 1;
    // char guess_char = (char)('a' + idx);
    int hit = 0;
    int len = strlen(st->secret_word);

    // Check match
    for (int i = 0; i < len; i++) {
        if (st->secret_word[i] == guess_char) {
            st->revealed[i] = guess_char;
            hit = 1;
        }
    }

    // guess_result_t res;
    // int score_change = 0;

    if (!hit) {
        st->remaining_attempts[player_id]--;
        if (st->remaining_attempts[player_id] <= 0) {
            st->player_eliminated[player_id] = 1;
            res = GUESS_ELIMINATED;
        } else {
            res = GUESS_MISS;
        }
    } else{
         if (strcmp(st->revealed, st->secret_word) == 0) {
        st->scores[player_id] += 2;
        // score_change = 2;
        res= GUESS_WORD_COMPLETED;
    } else{
        st->scores[player_id] += 1;
        // score_change = 1;
        res= GUESS_HIT;
        }
    }
    
   

    // Add to game log
    if (st->log_count < MAX_LOG_ENTRIES){
        st->logs[st->log_count].player_id = player_id;
        st->logs[st->log_count].guessed_char = guess_char;
        st->logs[st->log_count].result = res;
        strncpy(st->logs[st->log_count].word, st->secret_word, MAX_WORD_LEN - 1);
        st->log_count++;
    }
    return res;
}
    // --- SCORING LOGIC UPDATE ---
    
    // Check if word is fully revealed
    // +2 Points for finishing the word
    