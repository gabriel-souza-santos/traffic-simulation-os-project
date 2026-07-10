/**
 * @file analyser.h
 *
 * @brief Gerenciamento de requisições de movimentação e
 * prevenção de deadlocks
 *
 * @date 2026-07-04
 */

#ifndef URBAN_TRAFFIC_ANALYSER_H
#define URBAN_TRAFFIC_ANALYSER_H

#include "map.h"

/** @name Estruturas de dados */
/** @{ */

// Forward declaration
typedef struct Clock Clock;

/**
 * @brief Estado atual de uma requisição de movimento.
 */
typedef enum {
    REQUEST_EMPTY,    /**< O slot está livre/limpo no buffer atual. */
    REQUEST_PENDING,  /**< O veículo submeteu a intenção, aguardando análise. */
    REQUEST_APPROVED, /**< Movimento validado e aprovado pelo analisador. */
    REQUEST_DENIED,   /**< Movimento negado (colisão de destino ou obstáculo). */
} RequestStatus;

/**
 * @brief Estrutura que encapsula a intenção de movimento de um veículo.
 */
typedef struct {
    Coord from;           /**< Coordenada de origem (onde o veículo está). */
    Coord to;             /**< Coordenada de destino pretendida. */
    RequestStatus status; /**< Veredito atual desta requisição. */
} MovementRequest;

/**
 * @brief Tipo opaco que representa o árbitro central de movimentos.
 */
typedef struct Analyser Analyser;

/**
 * @brief Argumentos passados para a thread do analisador.
 */
typedef struct {
    Analyser *analyser; /**< Ponteiro para a instância do analisador. */
    Clock *clock;       /**< Ponteiro para o relógio global. */
    Map *map;           /**< Ponteiro para o mapa (para validação física). */
} AnalyserArgs;

/** @} */

/**
 * @brief Cria e inicializa o analisador de tráfego.
 *
 * Configura os buffers duplos de requisições e inicializa as variáveis
 * de condição e mutexes granulares (um por veículo) e o global do analisador.
 *
 * @return Ponteiro para a nova instância, ou NULL em caso de falha.
 */
Analyser *analyser_new(void);

/**
 * @brief Destrói o analisador e libera seus recursos.
 *
 * @param analyser Ponteiro para o analisador.
 */
void analyser_destroy(Analyser *analyser);

/**
 * @brief Loop principal da thread do analisador.
 *
 * Aguarda até que todos os veículos submetam suas requisições no tick atual.
 * Em seguida, resolve conflitos de destino via matriz de ocupação e responde
 * individualmente a cada veículo.
 *
 * @param analyser_args Ponteiro para AnalyserArgs.
 * @return NULL.
 */
void *analyser_update(void *analyser_args);

/**
 * @brief Submete uma requisição de movimento e bloqueia o veículo até o veredito.
 *
 * @details
 * Função bloqueante. A thread do veículo que chamar esta função dormirá
 * em sua variável de condição individual (`slot_cond`) até que a thread
 * do analisador processe todos os pedidos do tick e emita o sinal de despertar.
 *
 * @param analyser Ponteiro para o analisador.
 * @param id Identificador único do veículo (usado como índice).
 * @param request Dados do movimento pretendido.
 */
void analyser_request(Analyser *analyser, int id, MovementRequest request);

/**
 * @brief Retorna o ponteiro para o buffer de requisições do tick ANTERIOR.
 *
 * Utilizado pelo módulo de Renderização (se aplicar redesenho seletivo) para
 * saber exatamente quais movimentos foram aprovados no último ciclo fechado.
 *
 * @param analyser Ponteiro para o analisador.
 * @return Ponteiro para o array inativo de MovementRequest.
 */
MovementRequest *analyser_get_previous_requests(Analyser *analyser);

/**
 * @brief Consulta o status final de uma requisição no tick atual.
 *
 * @param analyser Ponteiro para o analisador.
 * @param id Identificador do veículo.
 * @return O status atualizado da requisição (Aprovado, Negado, etc).
 */
RequestStatus analyser_get_status(Analyser *analyser, int id);

/**
 * @brief Alterna os buffers internos (Double Buffering).
 *
 * Chamada EXCLUSIVAMENTE pelo relógio (Clock) no momento de virada de tick,
 * garantindo que o buffer antigo fique disponível para leitura e o novo seja limpo.
 *
 * @param analyser Ponteiro para o analisador.
 */
void analyser_swap_buffers(Analyser *analyser);

#endif //URBAN_TRAFFIC_ANALYSER_H
