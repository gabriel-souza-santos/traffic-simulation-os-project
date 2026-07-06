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
#include <pthread.h>

#include "vehicle.h"
#include "analyser.h"
#include "debug.h"

struct Analyser {
    MovementRequest requests[VEHICLE_COUNT];

    pthread_mutex_t slot_mutex[VEHICLE_COUNT];
    pthread_cond_t  slot_cond[VEHICLE_COUNT];

    pthread_mutex_t analyser_mutex;
    pthread_cond_t  analyser_cond;

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

        analyser->requests[i] = (MovementRequest){ .status = REQUEST_EMPTY };
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


    // Poderá ser mudado a depender da decisão de prevenção do deadlock

    // Registra a requisição no slot
    TRY(pthread_mutex_lock(&analyser->slot_mutex[id]));
    request.status = REQUEST_PENDING;
    analyser->requests[id] = request;
    TRY(pthread_mutex_unlock(&analyser->slot_mutex[id]));

    // Incrementa contador e acorda o analisador se todos submeteram
    TRY(pthread_mutex_lock(&analyser->analyser_mutex));
    analyser->pending_count++;
    if (analyser->pending_count == VEHICLE_COUNT) {
        TRY(pthread_cond_signal(&analyser->analyser_cond));
    }
    TRY(pthread_mutex_unlock(&analyser->analyser_mutex));

    // Readquire slot_mutex para dormir
    TRY(pthread_mutex_lock(&analyser->slot_mutex[id]));
    while (analyser->requests[id].status == REQUEST_PENDING) {
        TRY(pthread_cond_wait(
            &analyser->slot_cond[id],
            &analyser->slot_mutex[id]
        ));
    }
    TRY(pthread_mutex_unlock(&analyser->slot_mutex[id]));
}

static bool analyser_validate_move(Map *map, Coord from, Coord to)
{
    if (!map)
        return false;


    if (!map_is_within_bounds(map, from) ||
        !map_is_within_bounds(map, to))
        return false;


    if (map_is_blocked(map, to))
        return false;


    if (map_is_occupied(map, to))
        return false;


    return true;
}
void *analyser_update(void *analyser_args)
{
    AnalyserArgs *args = (AnalyserArgs *)analyser_args;
    Analyser *analyser = args->analyser;
    Map *map = args->map;

    while (1)
    {
        pthread_mutex_lock(&analyser->analyser_mutex);

        while (analyser->pending_count < VEHICLE_COUNT) {
            pthread_cond_wait(
                &analyser->analyser_cond,
                &analyser->analyser_mutex
            );
        }

        pthread_mutex_unlock(&analyser->analyser_mutex);

        /* CONTROLE DE CONFLITO DE DESTINO*/
        bool occupied_dest[VEHICLE_COUNT][VEHICLE_COUNT] = {0};

        /* PROCESSAMENTO DO TICK */
        for (int i = 0; i < VEHICLE_COUNT; i++)
        {
            pthread_mutex_lock(&analyser->slot_mutex[i]);

            MovementRequest *req = &analyser->requests[i];

            if (req->status == REQUEST_PENDING)
            {
                bool ok = analyser_validate_move(map, req->from, req->to);

                /* evita múltiplos veículos no mesmo destino */
                if (ok && !occupied_dest[req->to.x][req->to.y])
                {
                    occupied_dest[req->to.x][req->to.y] = true;

                    map_transfer_occupant(map, req->from, req->to);
                    req->status = REQUEST_APPROVED;
                }
                else
                {
                    req->status = REQUEST_DENIED;
                }

                pthread_cond_signal(&analyser->slot_cond[i]);
            }

            pthread_mutex_unlock(&analyser->slot_mutex[i]);
        }

        /* RESET DO TICK */
        pthread_mutex_lock(&analyser->analyser_mutex);
        analyser->pending_count = 0;
        pthread_mutex_unlock(&analyser->analyser_mutex);
    }

    return NULL;
}
    /*
     * Ao validar o pedido, chama a função:
     * map_transfer_occupant(map, request.from, request.to);
     * para atualizar o mapa
     */

     
/*
 * ============================================================================
 *
 * COMO FUNCIONA
 * -------------
 * Cada veículo tem um slot exclusivo na tabela de requisições
 *
 * Por exemplo:
 * veículo (id=0) --> slot[0]
 * veículo (id=1) --> slot[1]
 * veículo (id=2) --> slot[2]
 * ...
 *
 * como cada um tem seu slot, a memória não é compartilhada
 *
 * A cada tick:
 *
 *   1. O veículo escreve sua intenção de movimento (origem --> destino) no slot
 *      e dorme.
 *   2. O analisador varre a tabela, aprova ou nega cada requisição, e acorda
 *      os veículos individualmente.
 *   3. Cada veículo acorda, lê o resultado e executa ou não o movimento.
 *
 *
 *
 *
 *
 * RISCOS DE DEADLOCK E PREVENÇÕES ATUAIS
 * ----------------------------------------
 *
 * Risco 1 — Lock duplo em ordem inversa (usar o mutex do slot e o mutex do analisador)
 *
 *   Problema:  o veículo adquire adquire o mutex do seu slot e depois tenta adquirir
 *              o mutex do analisador. Se o analisador fizesse o caminho inverso,
 *              teríamos um deadlock circular.
 *
 *
 * Risco 3 — Aprovação dupla para o mesmo destino no mesmo tick
 *
 *   Problema:  dois veículos podem submeter requisições para a mesma célula.
 *              Como map_is_occupied reflete o estado atual do mapa (não as
 *              aprovações do tick corrente), ambos passariam na verificação.
 *
 *
 * NOTA
 * ----
 * Qualquer decisão de implementação aqui pode ser alterada caso seja
 * necessário para garantir a ausência de deadlocks.
 *
 * ============================================================================ */
