/**
 * @file clock.c
 *
 * @author Gabriel Souza
 * @date 2026-06-25
 */

#include <stddef.h>
#include <pthread.h>

#include "clock.h"
#include "debug.h"


/**
 * @internal
 * @brief Estrutura interna do relógio global e os seus mecanismos de sincronização.
 *
 * Implementa uma barreira de duas fases entre a thread do relógio e as
 * threads trabalhadoras: cada thread sinaliza conclusão do tick atual via
 * @c cond_clock (o último a terminar acorda o relógio); o relógio, ao
 * avançar o tick, libera todos os trabalhadores de uma só vez via
 * @c cond_workers (broadcast).
 */
struct Clock {
    size_t current_tick;        /**< Tick atual da simulação. Escrito apenas por clock_update, sob mutex. */
    size_t completed_count;     /**< Quantidade de trabalhadores que já sinalizaram conclusão do tick atual. */
    size_t total_workers;       /**< Quantidade total de trabalhadores. */
    pthread_mutex_t mutex;      /**< Protege current_tick e completed_count. */
    pthread_cond_t cond_clock;  /**< Sinalizada pelo último trabalhadores a concluir o tick; acorda a thread do relógio. */
    pthread_cond_t cond_workers;/**< Broadcast pela thread do relógio ao avançar o tick; acorda todos os trabalhadores. */
};


/**
 * @internal
 * @brief Implementação da criação do relógio.
 *
 * Aloca a estrutura e inicializa mutex e variáveis de condição via
 * @c pthread_*_init, encerrando o programa (via @c TRY) em caso de
 * falha de alocação ou inicialização.
 */
Clock *clock_new(const size_t total_workers) {
    if (total_workers == 0) {
        LOG("Error: parameter 'total_workers' cannot be zero.");
        return NULL;
    }

    Clock *clock = malloc(sizeof(Clock));
    if (!clock) {
        LOG("Error: failed to allocate memory for 'clock'.");
        return NULL;
    }

    TRY(pthread_mutex_init(&clock->mutex, NULL));
    TRY(pthread_cond_init(&clock->cond_clock, NULL));
    TRY(pthread_cond_init(&clock->cond_workers, NULL));

    clock->total_workers = total_workers;
    clock->current_tick = 0;
    clock->completed_count = 0;

    return clock;
}


/**
 * @internal
 * @brief Implementação da destruição do relógio.
 *
 * Destrói mutex e variáveis de condição via TRY (que aborta em caso de
 * erro) antes de liberar a memória da estrutura.
 */
void clock_destroy(Clock *clock) {
    if (!clock) {
        LOG("Error: parameter 'clock' is NULL.");
        return;
    }

    TRY(pthread_mutex_destroy(&clock->mutex));
    TRY(pthread_cond_destroy(&clock->cond_clock));
    TRY(pthread_cond_destroy(&clock->cond_workers));
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

    if (!clock) {
        LOG("Error: parameter 'clock' is NULL.");
        return 0;
    }

    pthread_mutex_lock(&clock->mutex);
    const size_t current_tick = clock->current_tick;
    pthread_mutex_unlock(&clock->mutex);

    return current_tick;
}


/**
 * @internal
 * @brief Implementação do laço principal da thread do relógio.
 *
 * A cada iteração, bloqueia em @c cond_clock enquanto
 * @c completed_count for menor que @c VEHICLE_COUNT — isto é, espera que todos
 * os veículos sinalizem conclusão do tick atual. Ao acordar, zera
 * @c completed_count, incrementa o tick atual e libera todos os veículos de
 * uma vez com @c pthread_cond_broadcast em @c cond_vehicles.
 *
 * @note O uso de @c while (em vez de @c if) ao redor de @c pthread_cond_wait
 *       protege contra despertar repentino de uma thread.
 */
void *clock_update(void *clock_args) {
    if (!clock_args) {
        LOG("Error: parameter 'clock_args' is NULL.");
        return NULL;
    }

    const ClockArgs *args = (ClockArgs *)clock_args;
    Clock *clock = args->clock;

    for (int i = 0; i < TICKS; i++) {
        TRY(pthread_mutex_lock(&clock->mutex));

        while (clock->completed_count < clock->total_workers) {
            TRY(pthread_cond_wait(&clock->cond_clock, &clock->mutex));
        }

        clock->completed_count = 0;
        clock->current_tick++;

        TRY(pthread_cond_broadcast(&clock->cond_workers));
        TRY(pthread_mutex_unlock(&clock->mutex));
    }

    return NULL;
}


/**
 * @internal
 * @brief Implementação da sinalização de conclusão de tick por uma thread trabalhadora.
 *
 * Incrementa @c completed_count sob lock; se o valor atingir a quantidade de
 * trabalhadores, sinaliza @c cond_clock para acordar a thread do relógio. Em seguida,
 * bloqueia em @c cond_workers enquanto o tick atual permanecer igual ao último
 * tick informado pela thread, sem busy-waiting, até o relógio avançar.
 */
void clock_signal(Clock *clock, const size_t tick) {
    if (!clock) {
        LOG("Error: parameter 'clock' is NULL.");
        return;
    }

    TRY(pthread_mutex_lock(&clock->mutex));
    clock->completed_count++;

    LOG_IF(clock->completed_count > clock->total_workers,
        "Warning: 'clock->completed_count' have been corrupted\n."
        "Max: %zu,\ncompleted_count: %zu.",
        clock->total_workers, clock->completed_count);


    if (clock->completed_count == clock->total_workers) {
        TRY(pthread_cond_signal(&clock->cond_clock));
    }

    while (clock->current_tick == tick) {
        TRY(pthread_cond_wait(&clock->cond_workers, &clock->mutex));
    }

    TRY(pthread_mutex_unlock(&clock->mutex));
}
