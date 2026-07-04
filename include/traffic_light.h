/**
 * @file traffic_light.h
 * @brief Definição da interface dos semáforos e controle de interseções.
 *
 * Este módulo gerencia o estado das luzes de trânsito nas interseções da
 * malha viária, coordenando o acesso de veículos aos cruzamentos de forma
 * sincronizada com o relógio global. Inclui suporte à prioridade de
 * ambulâncias, que pode forçar a abertura de luzes em um eixo específico.
 *
 * @date 2026-07-03
 */
#ifndef URBAN_TRAFFIC_TRAFFIC_LIGHT_H
#define URBAN_TRAFFIC_TRAFFIC_LIGHT_H

#include "clock.h"
#include "map.h"
#include "vehicle.h"

/** @name Estruturas de dados */
/** @{ */

/**
 * @brief Associa uma célula de espera (TILE_WAIT) à direção de fluxo que
 *        ela representa dentro de uma interseção.
 *
 * Cada ponto de espera corresponde a uma célula onde veículos bloqueiam
 * antes de entrar no cruzamento. A direção indica qual fluxo aquela célula
 * controla, permitindo que o semáforo abra ou feche eixos independentemente.
 */
typedef struct {
  Coord cell;          /**< Coordenada da célula TILE_WAIT no mapa. */
  Direction direction; /**< Direção do fluxo que aguarda nesta célula. */
} WaitPoint;

/**
 * @brief Descreve uma interseção da malha viária, agrupando os pontos de
 *        espera que a compõem.
 *
 * Deve ser construída com as coordenadas de todas as células TILE_WAIT
 * pertencentes a um mesmo cruzamento, cada uma associada à sua direção
 * de fluxo correspondente.
 */
typedef struct {
  WaitPoint *wait_points; /**< Array de pontos de espera desta interseção. */
  int count;              /**< Número de pontos de espera. */
} Intersection;

/**
 * @brief Argumentos passados para a thread do semáforo via pthread_create.
 */
typedef struct TrafficLight
    TrafficLight; // Fix: No struct abaixo, estava dando erro quando o
                  // compilador analisava ela.
typedef struct {
  Map *map;                    /**< Mapa da simulação. */
  Clock *clock;                /**< Relógio global da simulação. */
  TrafficLight *traffic_light; /**< Instância do semáforo a ser gerenciada. */
} TrafficLightArgs;

/**
 * @brief Representa os possíveis estados de uma luz de trânsito.
 *
 * O estado TRAFFIC_LIGHT_YELLOW é utilizado como estado intermediário
 * obrigatório na transição entre RED e GREEN (em ambas as direções),
 * evitando que dois fluxos conflitantes recebam sinal verde simultaneamente.
 *
 * @note Nenhuma transição direta RED → GREEN ou GREEN → RED é permitida,
 *       mesmo em situações de prioridade de ambulância.
 */
typedef enum {
  TRAFFIC_LIGHT_NONE,   /**< Ausência de semáforo na posição consultada. */
  TRAFFIC_LIGHT_RED,    /**< Via fechada; veículos devem aguardar. */
  TRAFFIC_LIGHT_GREEN,  /**< Via aberta; veículos podem avançar. */
  TRAFFIC_LIGHT_YELLOW, /**< Estado de transição; nenhum veículo deve avançar.
                         */
} TrafficLightColor;

/**
 * @brief Tipo opaco que representa o semáforo global e seus mecanismos
 * internos.
 *
 * Deve ser sempre usado por meio de um ponteiro:
 * @code{.c}
 * TrafficLight *traffic_light;
 * @endcode
 */
typedef struct TrafficLight TrafficLight;

/** @} */

/** @name API pública */
/** @{ */

/**
 * @brief Cria e inicializa uma nova instância do semáforo global.
 *
 * Recebe o array de interseções que o semáforo deve gerenciar e
 * inicializa o estado inicial de todas as luzes, além dos recursos
 * internos de sincronização.
 *
 * @param num Número de interseções no array.
 * @param intersections Array de interseções da malha viária a serem
 *                      controladas por este semáforo.
 * @return Ponteiro para a nova instância, ou NULL em caso de falha.
 */
TrafficLight *traffic_light_new(int num, Intersection *intersections);

/**
 * @brief Destrói a instância do semáforo e libera todos os recursos
 *        associados.
 *
 * @param traffic_light Ponteiro para o semáforo a ser destruído.
 */
void traffic_light_destroy(TrafficLight *traffic_light);

/**
 * @brief Rotina principal da thread do semáforo, responsável por atualizar
 *        o estado das luzes a cada tick do relógio.
 *
 * A cada tick, esta função: consulta a coordenada de prioridade da
 * ambulância via o módulo de veículos; determina o estado alvo de cada
 * luz com base nessa prioridade ou na lógica de alternância padrão; e
 * aplica as transições de estado respeitando a passagem obrigatória por
 * YELLOW.
 *
 * @par Prioridade da ambulância
 * A coordenada de prioridade pode ter três formas:
 * - Ambos os valores inválidos (NULL_COORD): sem prioridade ativa,
 *   alternância normal.
 * - Apenas @c x válido: todos os pontos de espera com o mesmo @c x
 *   recebem verde; os demais recebem vermelho.
 * - Apenas @c y válido: todos os pontos de espera com o mesmo @c y
 *   recebem verde; os demais recebem vermelho.
 *
 * @note A atualização ocorre no máximo uma vez por tick; a thread bloqueia
 *       sem busy-waiting até o próximo tick via clock_signal.
 * @note Nenhuma transição direta RED → GREEN é realizada; YELLOW é sempre
 *       o estado intermediário, inclusive em situações de prioridade.
 *
 * @param args Ponteiro para TrafficLightArgs, passado como void* pela API
 *             Pthreads.
 * @return NULL, respeitando a assinatura padrão exigida pela API Pthreads.
 */
void *traffic_light_update(void *args);

/**
 * @brief Retorna a cor atual da luz associada a uma célula de espera.
 *
 * A coordenada informada deve corresponder a um WaitPoint cadastrado em
 * alguma das interseções gerenciadas por este semáforo. Caso a coordenada
 * não esteja mapeada ou seja inválida, retorna TRAFFIC_LIGHT_NONE.
 *
 * @param traffic_light Ponteiro para a instância do semáforo.
 * @param position Coordenada da célula de espera (TILE_WAIT) a consultar.
 * @return A cor atual da luz naquela posição, ou TRAFFIC_LIGHT_NONE se
 *         a posição não estiver mapeada.
 */
TrafficLightColor traffic_light_get_current_light(TrafficLight *traffic_light,
                                                  Coord position);

#endif // URBAN_TRAFFIC_TRAFFIC_LIGHT_H
