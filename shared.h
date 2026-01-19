#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

#define MAX_PLAYERS 5

typedef struct {
    pthread_mutex_t game_mutex;
    pthread_mutex_t turn_mutex;

    int player_count;
    int current_turn;
    int active[MAX_PLAYERS];
} shared_state_t;

#endif
