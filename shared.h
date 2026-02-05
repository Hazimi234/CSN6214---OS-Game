// Purpose: Defines the Shared Memory structure and constants used by both Server and Client.


#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

// --- GAME CONSTANTS ---
#define MAX_PLAYERS 5
#define MAX_WORD_LEN 32
#define MAX_ATTEMPTS 6       // Maximum lives per player
#define MAX_LOG_ENTRIES 100  // Limit for game history log
#define LOG_MSG_LEN 64

// --- ENUMS ---

// Current state of the game session
typedef enum {
    GAME_WAITING = 0, // Lobby phase
    GAME_RUNNING = 1, // Active gameplay
    GAME_ENDED = 2    // Game over, showing scores
} game_phase_t;

// Result of a player's move
typedef enum {
    GUESS_INVALID,
    GUESS_DUPLICATE,      // Letter already used
    GUESS_HIT,            // Correct letter (+1 Score)
    GUESS_MISS,           // Wrong letter (-1 Life)
    GUESS_WORD_COMPLETED, // Finished word (+2 Score)
    GUESS_ELIMINATED,     // Ran out of lives
    GUESS_GAME_OVER
} guess_result_t;

// Structure for a single log entry (History)
typedef struct {
    int player_id;
    char guessed_char;
    guess_result_t result;
    int score_change;
    char word[MAX_WORD_LEN]; // Snapshot of the word at that time
} game_log_entry_t;

typedef struct
{

    // SECTION 1: SYNCHRONIZATION PRIMITIVES
    pthread_mutex_t game_mutex;  // Protects board state (variables below)
    pthread_mutex_t turn_mutex;  // Protects turn coordination logic
    
    pthread_cond_t turn_cond;    // Signal broadcast by Server: "Check if it's your turn"
    pthread_cond_t sched_cond;   // Signal sent by Client: "I finished my move"
    
    int turn_complete;           // Flag: 1 = Client is done, Server should schedule next

    // SECTION 2: GAME CONFIGURATION & LOBBY
    int player_count;            // Current number of connected players
    int target_players;          // How many players needed to start (Set by Host)
    int current_turn;            // Player ID (0 to MAX_PLAYERS-1) who is currently playing
    int active[MAX_PLAYERS];     // 1 = Player Connected, 0 = Empty Slot

    // SECTION 3: SHARED BOARD STATE
    char secret_word[MAX_WORD_LEN];  // The target word (e.g., "apple")
    char revealed[MAX_WORD_LEN];     // The masked word (e.g., "_pp_e")
    int guessed[26];                 // Array tracking used alphabet (A-Z)

    // SECTION 4: INDIVIDUAL PLAYER STATS
    int remaining_attempts[MAX_PLAYERS];
    int scores[MAX_PLAYERS];
    int player_eliminated[MAX_PLAYERS]; // 1 = Dead (0 lives left)

    // SECTION 5: LOGGING & SYSTEM STATE
    int log_count;
    game_log_entry_t logs[MAX_LOG_ENTRIES];
    game_phase_t phase;          // Waiting, Running, or Ended

} shared_state_t;

#endif