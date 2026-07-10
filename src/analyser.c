/**
 * @file analyser.c
 * @brief Implementação do analisador de requisições de movimento dos veículos.
 *
 * Este módulo centraliza a validação de todas as tentativas de movimento
 * dos veículos em cada tick da simulação, operando como árbitro entre as
 * threads dos veículos e o estado do mapa.

 * @date 2026-07-04
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "analyser.h"
#include "clock.h"
#include "debug.h"
#include "map.h"
#include "vehicle.h"


/**
 * @internal
 * @brief Estado interno do analisador e mecanismos de concorrência.
 *
 * Utiliza a técnica de Double Buffering para as requisições:
 * - O buffer 'active_request' recebe os pedidos do tick atual.
 * - O buffer inativo guarda a história do tick anterior para leitura sem lock (ex: renderizador).
 *
 * A concorrência é tratada de forma granular: cada veículo possui seu próprio
 * slot de mutex e variável de condição (`slot_mutex`, `slot_cond`). Isso evita
 * disputa de acesso (lock contention) quando dezenas de veículos tentam submeter
 * suas requisições simultaneamente.
 */
struct Analyser {
    MovementRequest requests[2][VEHICLE_COUNT];

    pthread_mutex_t slot_mutex[VEHICLE_COUNT];
    pthread_cond_t slot_cond[VEHICLE_COUNT];

    pthread_mutex_t analyser_mutex;
    pthread_cond_t analyser_cond;

    size_t pending_count;
    int active_request;
};

Analyser *analyser_new(void) {
    Analyser *analyser = malloc(sizeof(Analyser));
    if (!analyser) {
        LOG("Error: failed to allocate memory for 'analyser'.");
        return NULL;
    }

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        TRY(pthread_mutex_init(&analyser->slot_mutex[i], NULL));
        TRY(pthread_cond_init(&analyser->slot_cond[i], NULL));

        analyser->requests[0][i] = (MovementRequest){.status = REQUEST_EMPTY};
        analyser->requests[1][i] = (MovementRequest){.status = REQUEST_EMPTY};
    }

    TRY(pthread_mutex_init(&analyser->analyser_mutex, NULL));
    TRY(pthread_cond_init(&analyser->analyser_cond, NULL));

    analyser->active_request = 0;
    analyser->pending_count = 0;

    return analyser;
}

void analyser_destroy(Analyser *analyser) {
    if (!analyser) {
        LOG("Warning: parameter 'analyser' is NULL.");
        return;
    }

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        TRY(pthread_mutex_destroy(&analyser->slot_mutex[i]));
        TRY(pthread_cond_destroy(&analyser->slot_cond[i]));
    }

    TRY(pthread_mutex_destroy(&analyser->analyser_mutex));
    TRY(pthread_cond_destroy(&analyser->analyser_cond));

    free(analyser);
}

void analyser_request(Analyser *analyser, const int id, MovementRequest request) {
    if (!analyser) {
        LOG("Error: parameter 'analyser' is NULL.");
        return;
    }

    if (id < 0 || id >= VEHICLE_COUNT) {
        LOG("Error: invalid id: %d. Must be between 0 and %d.",
            id, VEHICLE_COUNT - 1);
        return;
    }

    // Obtem o slot ativo (valor só é alterado ao final do tick, após todos term acabdo o processamento)
    const int active = analyser->active_request;

    // Registra a requisição no slot
    TRY(pthread_mutex_lock(&analyser->slot_mutex[id]));
    {
        request.status = REQUEST_PENDING;
        analyser->requests[active][id] = request;
    }
    TRY(pthread_mutex_unlock(&analyser->slot_mutex[id]));

    // Incrementa contador e acorda o analisador se todos submeteram
    TRY(pthread_mutex_lock(&analyser->analyser_mutex));
    {
        analyser->pending_count++;
        if (analyser->pending_count == VEHICLE_COUNT) {
            TRY(pthread_cond_signal(&analyser->analyser_cond));
        }
    }
    TRY(pthread_mutex_unlock(&analyser->analyser_mutex));

    // Readquire slot_mutex para dormir
    TRY(pthread_mutex_lock(&analyser->slot_mutex[id]));
    {
        while (analyser->requests[active][id].status == REQUEST_PENDING) {
            TRY(pthread_cond_wait(
                &analyser->slot_cond[id],
                &analyser->slot_mutex[id]
            ));
        }
    }
    TRY(pthread_mutex_unlock(&analyser->slot_mutex[id]));
}


void analyser_swap_buffers(Analyser *analyser) {
    if (!analyser) {
        LOG("Error: parameter 'analyser' is NULL.");
        return;
    }

    TRY(pthread_mutex_lock(&analyser->analyser_mutex));
    {
        analyser->active_request = analyser->active_request? 0 : 1;

        for (int i = 0; i < VEHICLE_COUNT; i++) {
            analyser->requests[analyser->active_request][i].status = REQUEST_EMPTY;
        }
    }
    TRY(pthread_mutex_unlock(&analyser->analyser_mutex));
}



void *analyser_update(void *analyser_args) {
    if (!analyser_args) {
        LOG("Error: parameter 'analyser_args' is NULL.");
        return NULL;
    }
    const AnalyserArgs *args = (AnalyserArgs *) analyser_args;

    if (!args->analyser) {
        LOG("Error: thread argument 'analyser' is NULL.");
        return NULL;
    }

    if (!args->clock) {
        LOG("Error: thread argument 'clock' is NULL.");
        return NULL;
    }

    if (!args->map) {
        LOG("Error: thread argument 'map' is NULL.");
        return NULL;
    }

    Analyser *analyser = args->analyser;
    Map *map = args->map;
    Clock *clock = args->clock;

    const size_t map_width = map_get_width(map);
    const size_t map_height = map_get_height(map);

    /**
     * @internal
     * Matriz de resolução de conflitos de destino (VLA - Variable Length Array).
     * * Mapeia fisicamente o grid para garantir que dois ou mais veículos
     * não tentem ocupar o mesmo (x,y) simultaneamente no mesmo tick.
     * * @note Alocado na stack (Pilha). Como as dimensões são contidas (ex: max 255x255)
     * e um bool ocupa 1 byte, o consumo máximo é de ~65KB, o que é perfeitamente
     * seguro contra stack overflows em threads POSIX padrões (normalmente 2MB a 8MB).
     */
    bool destinations[map_width][map_height];

    const size_t total_ticks = clock_get_total_ticks(clock);
    for (size_t t = 0; t < total_ticks; t++) {
        // Faz reset das requisições (sizeof é avaliado em runtime)
        memset(destinations, false, sizeof(destinations));

        int active;
        TRY(pthread_mutex_lock(&analyser->analyser_mutex));
        {
            while (analyser->pending_count < VEHICLE_COUNT) {
                TRY(pthread_cond_wait(
                    &analyser->analyser_cond,
                    &analyser->analyser_mutex
                ));
            }
            analyser->pending_count = 0;
            active = analyser->active_request;
        }
        TRY(pthread_mutex_unlock(&analyser->analyser_mutex));

        // Análise das requisições
        for (int i = 0; i < VEHICLE_COUNT; i++) {
            TRY(pthread_mutex_lock(&analyser->slot_mutex[i]));
            {
                MovementRequest *request = &analyser->requests[active][i];

                // Se o slot não está pendente, destrava e pula o veículo
                if (request->status != REQUEST_PENDING) {
                    TRY(pthread_mutex_unlock(&analyser->slot_mutex[i]));
                    continue;
                }

                const bool is_idle = request->from.x == request->to.x &&
                                     request->from.y == request->to.y;

                const bool is_occupied = destinations[request->to.x][request->to.y];

                if (is_idle) {
                    destinations[request->to.x][request->to.y] = true;
                    request->status = REQUEST_APPROVED;
                }
                else if (is_occupied) {
                    request->status = REQUEST_DENIED;
                }
                else if (map_transfer_occupant(map, request->from, request->to)) {
                    // Mapa aceitou a transferência física do veículo
                    destinations[request->to.x][request->to.y] = true;
                    request->status = REQUEST_APPROVED;
                }
                else {
                    // Mapa recusou
                    request->status = REQUEST_DENIED;
                }

                // Acorda o veículo correspondente e libera o seu slot mutex
                TRY(pthread_cond_signal(&analyser->slot_cond[i]));
            }
            TRY(pthread_mutex_unlock(&analyser->slot_mutex[i]));
        }

        // Dorme e espera o próximo tick
        const size_t tick = clock_get_tick(clock);
        clock_signal(clock, tick);
    }

    return NULL;
}

RequestStatus analyser_get_status(Analyser *analyser, int id) {
    if (!analyser) {
        LOG("Error: parameter 'analyser' is NULL.");
        return REQUEST_EMPTY;
    }

    if (id < 0 || id >= VEHICLE_COUNT) {
        LOG("Error: invalid id: %d. Must be between 0 and %d.",
            id, VEHICLE_COUNT - 1);
        return REQUEST_EMPTY;
    }

    const int active = analyser->active_request;
    RequestStatus status;
    TRY(pthread_mutex_lock(&analyser->slot_mutex[id]));
    {
        status = analyser->requests[active][id].status;
    }
    TRY(pthread_mutex_unlock(&analyser->slot_mutex[id]));

    return status;
}

MovementRequest *analyser_get_previous_requests(Analyser *analyser) {
    if (!analyser) {
        LOG("Error: parameter 'analyser' is NULL.");
        return NULL;
    }

    const int inactive = 1 - analyser->active_request;
    return analyser->requests[inactive];
}


