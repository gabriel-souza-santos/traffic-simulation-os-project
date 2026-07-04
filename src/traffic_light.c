/**
 * @file traffic_light.c
 * @brief Implementação do controlador global de semáforos.
 *
 * Este módulo gerencia todas as interseções da malha viária,
 * sincronizando a troca de luzes com o relógio global da simulação.
 *
 * A máquina de estados utilizada é:
 *
 * GREEN -> YELLOW -> RED -> YELLOW -> GREEN
 *
 * Não são permitidas transições diretas entre GREEN e RED.
 *
 * @date 2026-07-04
 * @author José Dhonatan
 */

#include "traffic_light.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief Tempo (em ticks) que um eixo permanece aberto.
 */
#define GREEN_TIME 20

/**
 * @brief Tempo (em ticks) da fase para limpar a faixa de transição.
 */
#define YELLOW_TIME 8

/**
 * @bried Tempo (em ticks) da fase que os carros ficaram parados.
 */
#define RED_TIME 20 // Não necessario, colocado para melhorar compreensão

/**
 * @internal
 * @brief Representa uma única luz de trânsito.
 *
 * Cada WaitPoint existente na malha possui exatamente uma luz.
 */
typedef struct {

  WaitPoint wait_point;

  TrafficLightColor current;

  TrafficLightColor target;

} TrafficLightState;

/**
 * @internal
 * @brief Fases da máquina de estados.
 */
typedef enum {

  PHASE_GREEN,

  PHASE_YELLOW

} TrafficLightPhase;

/**
 * @internal
 * @brief Implementação concreta do controlador global.
 */
struct TrafficLight {

  /**
   * @brief Todas as interseções cadastradas.
   */
  Intersection *intersections;

  /**
   * @brief Quantidade de interseções.
   */
  int intersection_count;

  /**
   * @brief Vetor contendo todas as luzes.
   */
  TrafficLightState *lights;

  /**
   * @brief Número total de luzes.
   */
  int light_count;

  /**
   * @brief Exclusão mútua para acesso às luzes.
   */
  pthread_mutex_t mutex;

  /**
   * @brief Último tick processado.
   */
  size_t last_tick;

  /**
   * @brief Contador regressivo da fase atual.
   */
  int phase_ticks;

  /**
   * @brief Fase atual da máquina de estados.
   */
  TrafficLightPhase phase;

  /**
   * @brief Eixo atualmente liberado.
   */
  enum {

    AXIS_HORIZONTAL,

    AXIS_VERTICAL

  } current_axis;
};

/*
 * ============================================================================
 * Protótipos Privados
 * ============================================================================
 */

static bool is_horizontal(Direction direction);

static void update_targets(TrafficLight *traffic_light);

static void transition_light(TrafficLightState *light);

static void update_cycle(TrafficLight *traffic_light);

static int find_light(TrafficLight *traffic_light, Coord position);

/*
 * ============================================================================
 * API Pública
 * ============================================================================
 */

/**
 * @brief Cria um controlador de semáforos.
 */
TrafficLight *traffic_light_new(int num, Intersection *intersections) {
  // TODO:
  // Alocar memória para o controlador.

  // TODO:
  // Inicializar mutex.

  // TODO:
  // Salvar o vetor de interseções.

  // TODO:
  // Contabilizar a quantidade total de WaitPoints.

  // TODO:
  // Alocar o vetor de TrafficLightState.

  // TODO:
  // Inicializar todas as luzes.
  // Horizontal começa GREEN.
  // Vertical começa RED.

  // TODO:
  // Inicializar fase, eixo e contadores.

  return NULL;
}

/**
 * @brief Libera os recursos do controlador.
 */
void traffic_light_destroy(TrafficLight *traffic_light) {
  // TODO:
  // Validar ponteiro.

  // TODO:
  // Liberar vetor de luzes.

  // TODO:
  // Destruir mutex.

  // TODO:
  // Liberar controlador.
}

/**
 * @brief Thread principal do semáforo.
 */
void *traffic_light_update(void *args) {
  // TODO:
  // Converter args para TrafficLightArgs.

  // TODO:
  // Executar loop principal.

  // TODO:
  // Esperar próximo tick do relógio.

  // TODO:
  // Verificar se o tick já foi processado.

  // TODO:
  // Consultar prioridade da ambulância.

  // TODO:
  // Atualizar estados alvo.

  // TODO:
  // Atualizar máquina de estados.

  // TODO:
  // Liberar mutex.

  return NULL;
}

/**
 * @brief Consulta a cor atual de uma luz.
 */
TrafficLightColor traffic_light_get_current_light(TrafficLight *traffic_light,
                                                  Coord position) {
  // TODO:
  // Validar parâmetros.

  // TODO:
  // Localizar a luz correspondente.

  // TODO:
  // Retornar a cor atual.

  return TRAFFIC_LIGHT_NONE;
}

/*
 * ============================================================================
 * Funções Privadas
 * ============================================================================
 */

/**
 * @internal
 * @brief Determina se uma direção pertence ao eixo horizontal.
 */
static bool is_horizontal(Direction direction) {
  // TODO:
  // Retornar true para EAST/WEST.

  return false;
}

/**
 * @internal
 * @brief Atualiza os estados alvo das luzes.
 *
 * Se existir prioridade da ambulância, esta função deverá
 * substituir temporariamente a alternância normal entre eixos.
 */
static void update_targets(TrafficLight *traffic_light) {
  // TODO:
  // Percorrer todas as luzes.

  // TODO:
  // Definir target = GREEN para o eixo ativo.

  // TODO:
  // Definir target = RED para o eixo oposto.
}

/**
 * @internal
 * @brief Executa uma etapa da máquina de estados.
 */
static void transition_light(TrafficLightState *light) {
  // TODO:
  // Se current == target, não fazer nada.

  // TODO:
  // GREEN -> YELLOW.

  // TODO:
  // RED -> YELLOW.

  // TODO:
  // YELLOW -> GREEN.

  // TODO:
  // YELLOW -> RED.
}

/**
 * @internal
 * @brief Atualiza o ciclo dos semáforos.
 */
static void update_cycle(TrafficLight *traffic_light) {
  // TODO:
  // Decrementar contador da fase.

  // TODO:
  // Se ainda houver tempo restante, retornar.

  // TODO:
  // Se fase GREEN:
  //     mudar para YELLOW.

  // TODO:
  // Se fase YELLOW:
  //     inverter eixo.
  //     recalcular targets.
  //     iniciar GREEN_TIME.

  // TODO:
  // Aplicar transition_light() em todas as luzes.
}

/**
 * @internal
 * @brief Localiza uma luz pela coordenada.
 */
static int find_light(TrafficLight *traffic_light, Coord position) {
  // TODO:
  // Percorrer todas as luzes.

  // TODO:
  // Comparar coordenadas.

  // TODO:
  // Retornar índice encontrado.

  return -1;
}
