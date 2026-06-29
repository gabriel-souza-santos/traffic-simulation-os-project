/**
 * @file clock.c
 *
 * @date 2026-06-25
 */

#include <stddef.h>
#include <pthread.h>

#include "clock.h"
#include "debug.h"
#include "vehicle.h"


struct Clock {
    size_t current_tick;
    size_t completed_count;
    pthread_mutex_t mutex;
    pthread_cond_t cond_clock;
    pthread_cond_t cond_vehicles;
};


Clock *clock_new(void) {
    Clock *clock = malloc(sizeof(Clock));
    CHECK_NULL(clock);

    TRY(pthread_mutex_init(&clock->mutex, NULL));
    TRY(pthread_cond_init(&clock->cond_clock, NULL));
    TRY(pthread_cond_init(&clock->cond_vehicles, NULL));
    clock->current_tick = 0;
    clock->completed_count = 0;

    return clock;
}


void clock_destroy(Clock *clock) {
    CHECK_NULL(clock);
    TRY(pthread_mutex_destroy(&clock->mutex));
    TRY(pthread_cond_destroy(&clock->cond_clock));
    TRY(pthread_cond_destroy(&clock->cond_vehicles));
    free(clock);
}


/**
 * @internal
 * @brief Implementação da leitura do tick atual.
 *
 * Protege a leitura de current_tick com o mesmo mutex usado por
 * clock_update na escrita, evitando leitura concorrente não sincronizada.
 */
size_t clock_get_tick(Clock *clock) {
    CHECK_NULL(clock);

    pthread_mutex_lock(&clock->mutex);
    const size_t current_tick = clock->current_tick;
    pthread_mutex_unlock(&clock->mutex);

    return current_tick;
}


void *clock_update(void *arg) {
    CHECK_NULL(arg);
    Clock *clock = (Clock *)arg;

    for (int i = 0; i < TICKS; i++) {
        TRY(pthread_mutex_lock(&clock->mutex));

        while (clock->completed_count < VEHICLE_COUNT) {
            TRY(pthread_cond_wait(&clock->cond_clock, &clock->mutex));
        }

        clock->completed_count = 0;
        clock->current_tick++;

        TRY(pthread_cond_broadcast(&clock->cond_vehicles));
        TRY(pthread_mutex_unlock(&clock->mutex));
    }

    return NULL;
}


void clock_signal(Clock *clock, const size_t last_tick) {
    CHECK_NULL(clock);

    TRY(pthread_mutex_lock(&clock->mutex));
    clock->completed_count++;

    if (clock->completed_count > VEHICLE_COUNT) {
        fprintf(stderr, "Error: 'clock->completed_count' was corrupted");
    }

    if (clock->completed_count == VEHICLE_COUNT) {
        TRY(pthread_cond_signal(&clock->cond_clock));
    }

    while (clock->current_tick == last_tick) {
        TRY(pthread_cond_wait(&clock->cond_vehicles, &clock->mutex));
    }

    TRY(pthread_mutex_unlock(&clock->mutex));
}
