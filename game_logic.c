//Purpose: Implementation of Hangman rules and Word Bank.

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

// Pick a random word
const char* get_random_word() {
    int r = rand() % BANK_SIZE;
    return WORD_BANK[r];
}

// Helper: Convert 'A' or 'a' to index 0 (0-25)
static int normalize_letter(const char *input)
{
    if (!input || strlen(input) < 1) return -1;
    unsigned char c = (unsigned char)input[0];
    if (!isalpha(c)) return -1;
    return tolower(c) - 'a';
}

// Setup Round: Clears board, sets new secret word
void game_setup_round(shared_state_t *st, const char *word)
{
    memset(st->guessed, 0, sizeof(st->guessed)); // Clear used letters
    
    strncpy(st->secret_word, word, MAX_WORD_LEN - 1);
    st->secret_word[MAX_WORD_LEN - 1] = '\0';

    int len = strlen(st->secret_word);
    for (int i = 0; i < len; i++) {
        st->revealed[i] = '_'; // Mask the word with underscores
    }
    st->revealed[len] = '\0';
}

// Core Game Logic
guess_result_t game_apply_guess(shared_state_t *st, int player_id, const char *input)
{
    int idx = normalize_letter(input);
    if (idx < 0 || idx > 25) return GUESS_INVALID;

    // 1. Check if letter was already guessed (Shared History)
    if (st->guessed[idx]) {
        return GUESS_DUPLICATE;
    }
    
    // 2. Mark letter as used
    st->guessed[idx] = 1;
    char guess_char = (char)('a' + idx);
    
    // 3. Check for match in secret word
    int hit = 0;
    int len = strlen(st->secret_word);
    for (int i = 0; i < len; i++) {
        if (st->secret_word[i] == guess_char) {
            st->revealed[i] = guess_char; // Reveal character
            hit = 1;
        }
    }

    guess_result_t res;

    // 4. Update Status (Miss/Hit/Win)
    if (!hit) {
        st->remaining_attempts[player_id]--; // Deduct Life
        if (st->remaining_attempts[player_id] <= 0) {
            st->player_eliminated[player_id] = 1;
            res = GUESS_ELIMINATED;
        } else {
            res = GUESS_MISS;
        }
    } else {
        // Check if the WHOLE word is done
        if (strcmp(st->revealed, st->secret_word) == 0) {
            st->scores[player_id] += 2; // +2 Points for Winning
            res = GUESS_WORD_COMPLETED;
        } else {
            st->scores[player_id] += 1; // +1 Point for Hit
            res = GUESS_HIT;
        }
    }
    
    // 5. Log the Event to Shared Memory
    if (st->log_count < MAX_LOG_ENTRIES){
        st->logs[st->log_count].player_id = player_id;
        st->logs[st->log_count].guessed_char = guess_char;
        st->logs[st->log_count].result = res;
        strncpy(st->logs[st->log_count].word, st->secret_word, MAX_WORD_LEN - 1);
        st->log_count++;
    }

    return res;
}