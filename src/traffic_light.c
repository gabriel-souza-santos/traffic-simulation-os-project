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
#include "clock.h"
#include "map.h"
#include "vehicle.h"
#include <pthread.h>

#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

static bool is_horizontal(Direction direction);

static void update_targets(TrafficLight *traffic_light);

static void transition_light(TrafficLightState *light);

static void update_cycle(TrafficLight *traffic_light);

static int find_light(TrafficLight *traffic_light, Coord position);

/*/**
 * @brief Cria e inicializa uma nova instância do controlador de semáforos.
 *
 * @details
 * A inicialização do controlador consiste em:
 * - validar os parâmetros recebidos;
 * - alocar a estrutura principal do módulo;
 * - inicializar os mecanismos de sincronização;
 * - registrar todas as interseções da malha viária;
 * - criar uma estrutura de controle para cada WaitPoint existente;
 * - configurar o estado inicial da máquina de estados.

 * Inicialmente, o eixo horizontal permanece aberto, permitindo o fluxo
 * dos veículos nas direções EAST e WEST. Consequentemente, as direções
 * NORTH e SOUTH iniciam fechadas.

 * @param num Quantidade de interseções existentes.
 * @param intersections Vetor contendo todas as interseções da malha.

 * @retval NULL Falha na validação dos parâmetros.
 * @retval NULL Falha na alocação de memória.
 * @retval TrafficLight* Ponteiro para o controlador inicializado.
 */
TrafficLight *traffic_light_new(int num, Intersection *intersections) {

  /**
   * @brief Valida os parâmetros de entrada.
   *
   * @details
   * O controlador somente pode ser criado quando existe pelo menos uma
   * interseção cadastrada e o vetor recebido é válido.
   */
  if (num <= 0 || intersections == NULL) {
    return NULL;
  }

  /**
   * @brief Aloca a estrutura principal do controlador.
   *
   * @details
   * Esta estrutura armazenará todas as informações necessárias para o
   * gerenciamento dos semáforos durante toda a execução da simulação.
   */
  TrafficLight *traffic_light = malloc(sizeof(TrafficLight));

  /**
   * @brief Verifica se a alocação foi realizada com sucesso.
   */
  if (traffic_light == NULL) {
    return NULL;
  }

  /**
   * @brief Inicializa todos os campos da estrutura.
   *
   * @details
   * Todos os bytes são preenchidos com zero para garantir que ponteiros,
   * contadores e demais atributos iniciem em um estado conhecido antes
   * de serem configurados individualmente.
   */
  memset(traffic_light, 0, sizeof(TrafficLight));

  /**
   * @brief Inicializa o mutex responsável pela sincronização.
   *
   * @details
   * O controlador poderá ser acessado simultaneamente pela thread dos
   * semáforos e pelas threads dos veículos. O mutex garante exclusão
   * mútua durante essas operações.
   */
  if (pthread_mutex_init(&traffic_light->mutex, NULL) != 0) {
    free(traffic_light);
    return NULL;
  }

  /**
   * @brief Registra as interseções gerenciadas pelo controlador.
   *
   * @note
   * O vetor não é copiado. Apenas seu endereço é armazenado.
   */
  traffic_light->intersections = intersections;
  traffic_light->intersection_count = num;

  /**
   * @brief Calcula a quantidade total de luzes da simulação.
   *
   * @details
   * Cada WaitPoint corresponde exatamente a um semáforo. Portanto,
   * basta somar a quantidade de WaitPoints existentes em todas as
   * interseções cadastradas.
   */
  traffic_light->light_count = 0;

  for (int i = 0; i < num; i++) {
    traffic_light->light_count += intersections[i].count;
  }

  /**
   * @brief Aloca o vetor contendo o estado de todas as luzes.
   */
  traffic_light->lights =
      malloc(traffic_light->light_count * sizeof(TrafficLightState));

  /**
   * @brief Trata falha durante a alocação do vetor de luzes.
   *
   * @details
   * Caso a alocação falhe, todos os recursos adquiridos anteriormente
   * devem ser liberados antes do retorno.
   */
  if (traffic_light->lights == NULL) {
    pthread_mutex_destroy(&traffic_light->mutex);
    free(traffic_light);
    return NULL;
  }

  /**
   * @brief Inicializa individualmente todas as luzes da simulação.
   *
   * @details
   * Cada WaitPoint origina exatamente uma estrutura
   * TrafficLightState. O estado inicial depende do eixo ao qual a
   * direção pertence.
   *
   * - Horizontal → GREEN
   * - Vertical → RED
   */
  int index = 0;

  for (int i = 0; i < num; i++) {

    for (int j = 0; j < intersections[i].count; j++) {

      WaitPoint wait_point = intersections[i].wait_points[j];

      traffic_light->lights[index].wait_point = wait_point;

      if (is_horizontal(wait_point.direction)) {

        traffic_light->lights[index].current = TRAFFIC_LIGHT_GREEN;

        traffic_light->lights[index].target = TRAFFIC_LIGHT_GREEN;

      } else {

        traffic_light->lights[index].current = TRAFFIC_LIGHT_RED;

        traffic_light->lights[index].target = TRAFFIC_LIGHT_RED;
      }

      index++;
    }
  }

  /**
   * @brief Inicializa a máquina de estados.
   *
   * @details
   * A simulação inicia com o eixo horizontal liberado. O contador de
   * ticks é configurado para determinar quanto tempo essa fase deverá
   * permanecer ativa antes da primeira transição.
   */
  traffic_light->phase = PHASE_GREEN;
  traffic_light->phase_ticks = GREEN_TIME;
  traffic_light->current_axis = AXIS_HORIZONTAL;
  traffic_light->last_tick = 0;

  /**
   * @brief Retorna o controlador completamente inicializado.
   */
  return traffic_light;
}

/*
 * @brief Libera os recursos do controlador.
 */
/**
 * @brief Libera todos os recursos associados ao controlador de semáforos.
 *
 * @details
 * Esta função é responsável por realizar a destruição segura da instância
 * do controlador, liberando todos os recursos adquiridos durante sua
 * criação.
 *
 * A ordem de liberação é importante para evitar vazamentos de memória e
 * garantir que nenhum recurso do sistema permaneça alocado após o término
 * da simulação.
 *
 * Os seguintes recursos são liberados:
 * - vetor contendo o estado de todas as luzes;
 * - mutex utilizado para sincronização entre threads;
 * - estrutura principal do controlador.
 *
 * Caso o ponteiro recebido seja NULL, nenhuma operação é realizada.
 *
 * @param traffic_light Ponteiro para o controlador a ser destruído.
 */
void traffic_light_destroy(TrafficLight *traffic_light) {

  /*
   * Verifica se o controlador é válido.
   *
   * A função free() aceita ponteiros nulos, porém acessar membros de uma
   * estrutura inexistente resultaria em comportamento indefinido.
   * Portanto, caso o controlador não exista, a função apenas retorna.
   */
  if (traffic_light == NULL) {
    return;
  }

  /*
   * Libera o vetor contendo todas as luzes da simulação.
   *
   * Cada posição deste vetor representa o estado de um WaitPoint da
   * malha viária. Após sua liberação, o ponteiro é definido como NULL
   * para evitar referências pendentes.
   */
  free(traffic_light->lights);
  traffic_light->lights = NULL;

  /*
   * Destrói o mutex utilizado para sincronizar o acesso ao controlador.
   *
   * Após esta chamada o mutex deixa de ser um objeto válido e não poderá
   * mais ser utilizado por nenhuma thread da aplicação.
   */
  pthread_mutex_destroy(&traffic_light->mutex);

  /*
   * Libera a estrutura principal do controlador.
   */
  free(traffic_light);
}

/**
 * @brief Thread principal responsável pelo gerenciamento dos semáforos.
 *
 * @details
 * Esta função permanece executando durante toda a simulação,
 * sincronizando o controlador de semáforos com o relógio global.
 *
 * Ao final do processamento de cada tick, a thread sinaliza ao módulo
 * Clock que concluiu seu trabalho através de clock_signal(). Dessa forma,
 * o controlador participa da barreira de sincronização juntamente com
 * as demais threads da simulação.
 *
 * Para cada tick são executadas as seguintes etapas:
 * - aguardar o próximo tick;
 * - proteger o controlador por meio do mutex;
 * - verificar prioridade de ambulância;
 * - calcular o estado alvo das luzes;
 * - atualizar a máquina de estados;
 * - liberar o mutex;
 * - informar ao relógio que terminou o processamento daquele tick.
 *
 * @param args Ponteiro para TrafficLightArgs.
 *
 * @return NULL.
 */
void *traffic_light_update(void *args) {
  if (args == NULL) {
    return NULL;
  }
  TrafficLightArgs *traffic_args = (TrafficLightArgs *)args;

  TrafficLight *traffic_light = traffic_args->traffic_light;
  Clock *clock = traffic_args->clock;

  if (traffic_light == NULL) {
    return NULL;
  }
  if (clock == NULL) {
    return NULL;
  }

  /*
   * Obtém o tick inicial da simulação.
   *
   * Este valor será utilizado pelo clock para bloquear esta thread
   * até que um novo tick seja produzido.
   */
  size_t current_tick = clock_get_tick(clock);

  while (current_tick < TICKS) {

    /*
     * Aguarda o relógio avançar para o próximo tick.
     *
     * A thread permanece bloqueada sem realizar busy waiting.
     */
    clock_signal(clock, current_tick);

    /*
     * Atualiza o valor do tick atual.
     */
    current_tick = clock_get_tick(clock);

    /*
     * Garante acesso exclusivo ao controlador durante toda a
     * atualização das luzes.
     */
    pthread_mutex_lock(&traffic_light->mutex);

    /*
     * Consulta a prioridade da ambulância.
     *
     * Caso exista uma ambulância solicitando prioridade, esta
     * informação será utilizada pela função update_targets().
     */
    // TODO:
    // Coord priority = vehicle_get_priority();

    /*
     * Calcula o estado alvo de todas as luzes.
     */
    update_targets(traffic_light);

    /*
     * Executa uma etapa da máquina de estados.
     */
    update_cycle(traffic_light);

    /*
     * Registra o último tick processado.
     */
    traffic_light->last_tick = current_tick;

    /*
     * Libera o controlador para outras threads.
     */
    pthread_mutex_unlock(&traffic_light->mutex);
  }

  return NULL;
}

/**
 * @brief Retorna a cor atualmente exibida por um semáforo.
 *
 * @details
 * Esta função localiza a luz associada ao WaitPoint informado e retorna
 * sua cor atual. O estado retornado corresponde exatamente à cor visível
 * para os veículos naquele instante da simulação.
 *
 * Como o estado das luzes pode ser alterado concorrentemente pela thread
 * responsável pelo controlador de semáforos, o acesso ao vetor de luzes
 * deve ocorrer sob proteção do mutex interno da estrutura.
 *
 * Caso a posição informada não corresponda a nenhum WaitPoint cadastrado,
 * a função retorna TRAFFIC_LIGHT_NONE.
 *
 * @param traffic_light Ponteiro para o controlador global de semáforos.
 * @param position Coordenada da célula WAIT cuja luz será consultada.
 *
 * @return A cor atualmente exibida pelo semáforo.
 */

TrafficLightColor traffic_light_get_current_light(TrafficLight *traffic_light,
                                                  Coord position) {
  /**
   * @brief Verifica se os parâmetros recebidos são válidos.
   *
   * @details
   * Não é possível realizar a consulta caso o controlador não exista.
   * Em caso de erro, retorna TRAFFIC_LIGHT_NONE indicando ausência de
   * uma luz válida.
   */
  if (traffic_light == NULL) {
    return TRAFFIC_LIGHT_NONE;
  }

  /**
   * @brief Obtém acesso exclusivo ao vetor de luzes.
   *
   * @details
   * A thread responsável pelos semáforos pode alterar o estado das
   * luzes simultaneamente. O mutex garante que a leitura seja realizada
   * de forma consistente.
   */
  pthread_mutex_lock(&traffic_light->mutex);

  /**
   * @brief Procura a luz correspondente à posição informada.
   *
   * @details
   * A busca percorre o vetor de TrafficLightState até encontrar o
   * WaitPoint associado à coordenada recebida.
   */
  int index = find_light(traffic_light, position);

  /**
   * @brief Caso nenhuma luz seja encontrada, libera o mutex e informa
   * que não existe um semáforo associado à posição consultada.
   */
  if (index == -1) {
    pthread_mutex_unlock(&traffic_light->mutex);
    return TRAFFIC_LIGHT_NONE;
  }

  /**
   * @brief Obtém a cor atualmente exibida pela luz encontrada.
   *
   * @details
   * O campo current representa exatamente a cor observada pelos
   * veículos durante o tick atual da simulação.
   */
  TrafficLightColor color = traffic_light->lights[index].current;

  /**
   * @brief Libera o acesso ao controlador.
   *
   * @details
   * Após a leitura do estado da luz, outras threads podem voltar a
   * acessar o controlador normalmente.
   */
  pthread_mutex_unlock(&traffic_light->mutex);

  /**
   * @brief Retorna a cor encontrada.
   */
  return color;
}

/**
 * @internal
 * @brief Determina se uma direção pertence ao eixo horizontal.
 */
/**
 * @brief Determina se uma direção pertence ao eixo horizontal.
 *
 * @details
 * O controlador de semáforos trabalha alternando a abertura entre dois
 * eixos da via:
 *
 * - Horizontal: EAST e WEST;
 * - Vertical: NORTH e SOUTH.
 *
 * Esta função é utilizada para identificar rapidamente a qual eixo uma
 * determinada direção pertence, permitindo definir quais luzes deverão
 * permanecer verdes ou vermelhas durante cada fase da máquina de estados.
 *
 * @param direction Direção do fluxo de veículos.
 *
 * @retval true A direção pertence ao eixo horizontal (EAST ou WEST).
 * @retval false A direção pertence ao eixo vertical ou é inválida.
 */
static bool is_horizontal(Direction direction) {

  switch (direction) {

  case DIRECTION_LEFT:
  case DIRECTION_RIGHT:
    return true;

  case DIRECTION_UP:
  case DIRECTION_DOWN:
  case DIRECTION_NONE:
  default:
    return false;
  }
}
/**
 * @internal
 * @brief Atualiza o estado alvo de todas as luzes da simulação.
 *
 * @details
 * Esta função percorre todas as luzes cadastradas no controlador e
 * determina qual deverá ser sua cor final (target) de acordo com o eixo
 * atualmente liberado.
 *
 * Nenhuma alteração é realizada sobre a cor atualmente exibida
 * (current). Apenas o estado desejado é atualizado. A transição entre
 * as cores será executada posteriormente pela máquina de estados,
 * garantindo que nenhuma mudança direta entre GREEN e RED ocorra.
 *
 * Caso exista prioridade para uma ambulância(TODO), esta lógica poderá ser
 * substituída temporariamente para liberar o eixo correspondente.
 *
 * @param traffic_light Ponteiro para o controlador de semáforos.
 */
static void update_targets(TrafficLight *traffic_light) {

  /**
   * @brief Percorre todas as luzes cadastradas.
   */
  for (int i = 0; i < traffic_light->light_count; i++) {

    TrafficLightState *light = &traffic_light->lights[i];

    /**
     * @brief Verifica se a luz pertence ao eixo atualmente aberto.
     *
     * @details
     * Quando o eixo da luz coincide com o eixo ativo da máquina de
     * estados, sua cor alvo passa a ser GREEN. Caso contrário,
     * permanece RED.
     */
    if (is_horizontal(light->wait_point.direction) ==
        (traffic_light->current_axis == AXIS_HORIZONTAL)) {

      /**
       * @brief Define que esta luz deverá permanecer ou tornar-se
       * verde.
       */
      light->target = TRAFFIC_LIGHT_GREEN;

    } else {

      /**
       * @brief Define que esta luz deverá permanecer ou tornar-se
       * vermelha.
       */
      light->target = TRAFFIC_LIGHT_RED;
    }
  }

  /**
   * TODO: Implementar a lógica de prioridade para ambulâncias.
   *
   * Quando houver prioridade ativa, o eixo determinado pela ambulância
   * deverá substituir temporariamente o eixo definido pela máquina de
   * estados.
   */
}

/**
 * @internal
 * @brief Executa uma única etapa da máquina de estados de uma luz.
 *
 * @details
 * Esta função atualiza a cor atualmente exibida pelo semáforo
 * (current) aproximando-a da cor desejada (target).
 *
 * A transição sempre respeita a seguinte máquina de estados:
 *
 * GREEN  → YELLOW → RED
 *
 * RED    → YELLOW → GREEN
 *
 * Dessa forma nunca ocorre uma mudança direta entre GREEN e RED,
 * garantindo que todos os veículos tenham um intervalo de segurança
 * representado pela luz amarela.
 *
 * @param light Ponteiro para o estado da luz que será atualizado.
 */
static void transition_light(TrafficLightState *light) {

  /**
   * @brief Valida o parâmetro recebido.
   *
   * @details
   * Caso o ponteiro seja inválido, nenhuma atualização pode ser
   * realizada.
   */
  if (light == NULL) {
    return;
  }

  /**
   * @brief Verifica se a luz já atingiu o estado desejado.
   *
   * @details
   * Quando a cor atual coincide com a cor alvo, nenhuma transição é
   * necessária durante este tick.
   */
  if (light->current == light->target) {
    return;
  }

  /**
   * @brief Executa uma etapa da máquina de estados.
   *
   * @details
   * Apenas uma mudança de estado é realizada por chamada da função.
   */
  switch (light->current) {

  /**
   * @brief Inicia o fechamento de uma via.
   *
   * @details
   * Antes de fechar completamente uma via, a luz obrigatoriamente
   * passa pelo estado YELLOW.
   */
  case TRAFFIC_LIGHT_GREEN:

    light->current = TRAFFIC_LIGHT_YELLOW;
    break;

  /**
   * @brief Inicia a abertura de uma via.
   *
   * @details
   * Antes de abrir completamente uma via, a luz também passa pelo
   * estado YELLOW.
   */
  case TRAFFIC_LIGHT_RED:

    light->current = TRAFFIC_LIGHT_YELLOW;
    break;

  /**
   * @brief Finaliza a transição iniciada anteriormente.
   *
   * @details
   * Quando a luz já está amarela, o próximo estado depende da cor
   * alvo definida pela máquina de estados.
   */
  case TRAFFIC_LIGHT_YELLOW:

    if (light->target == TRAFFIC_LIGHT_GREEN) {
      light->current = TRAFFIC_LIGHT_GREEN;
    } else {
      light->current = TRAFFIC_LIGHT_RED;
    }

    break;

  /**
   * @brief Estado inválido.
   *
   * @details
   * Caso a luz esteja em um estado inesperado, nenhuma alteração é
   * realizada.
   */
  default:
    break;
  }
}

/**
 * @internal
 * @brief Atualiza a máquina temporal dos semáforos.
 *
 * @details
 * Esta função controla o tempo de permanência de cada fase do semáforo.
 * Ela deve ser executada exatamente uma vez a cada tick da simulação.
 *
 * Enquanto ainda houver tempo restante na fase atual, apenas decrementa
 * o contador e mantém o estado atual.
 *
 * Quando o contador chega a zero, ocorre a transição para a próxima fase
 * da máquina de estados:
 *
 * GREEN → YELLOW → GREEN (eixo oposto)
 *
 * Após definir a nova fase, uma etapa da máquina de estados é aplicada a
 * todas as luzes da simulação por meio de transition_light().
 *
 * @param traffic_light Controlador global dos semáforos.
 */
static void update_cycle(TrafficLight *traffic_light) {

  /**
   * @brief Consome um tick da fase atual.
   *
   * @details
   * Cada fase (GREEN ou YELLOW) permanece ativa durante um número
   * determinado de ticks. A cada chamada desta função, um tick é
   * consumido.
   */
  traffic_light->phase_ticks--;

  /**
   * @brief Verifica se a fase atual ainda não terminou.
   *
   * @details
   * Caso ainda exista tempo restante, nenhuma alteração na máquina de
   * estados é realizada durante este tick.
   */
  if (traffic_light->phase_ticks > 0) {
    return;
  }

  /**
   * @brief Trata a mudança de fase.
   */
  switch (traffic_light->phase) {

  /**
   * @brief Final do período verde.
   *
   * @details
   * O eixo atualmente aberto permanecerá amarelo durante alguns
   * ticks antes de ser completamente fechado.
   */
  case PHASE_GREEN:

    traffic_light->phase = PHASE_YELLOW;
    traffic_light->phase_ticks = YELLOW_TIME;

    break;

  /**
   * @brief Final do período amarelo.
   *
   * @details
   * Após o tempo de segurança da luz amarela:
   *
   * - o eixo ativo é invertido;
   * - os estados alvo das luzes são recalculados;
   * - inicia-se um novo período verde.
   */
  case PHASE_YELLOW:

    if (traffic_light->current_axis == AXIS_HORIZONTAL) {
      traffic_light->current_axis = AXIS_VERTICAL;
    } else {
      traffic_light->current_axis = AXIS_HORIZONTAL;
    }

    update_targets(traffic_light);

    traffic_light->phase = PHASE_GREEN;
    traffic_light->phase_ticks = GREEN_TIME;

    break;
  }

  /**
   * @brief Atualiza todas as luzes da simulação.
   *
   * @details
   * Cada luz executa exatamente uma etapa da máquina de estados,
   * aproximando sua cor atual da cor alvo calculada anteriormente.
   */
  for (int i = 0; i < traffic_light->light_count; i++) {
    transition_light(&traffic_light->lights[i]);
  }
}

/**
 * @internal
 * @brief Localiza uma luz pela coordenada.
 */
/**
 * @brief Localiza uma luz de trânsito a partir da coordenada de um WaitPoint.
 *
 * @details
 * Percorre o vetor contendo todas as luzes cadastradas no controlador e
 * procura aquela associada à coordenada informada.
 *
 * Como cada WaitPoint da malha viária possui exatamente um semáforo
 * correspondente, a busca é encerrada assim que uma correspondência é
 * encontrada.
 *
 * Esta função é utilizada internamente pelo módulo para converter uma
 * coordenada do mapa no índice da estrutura TrafficLightState
 * correspondente.
 *
 * @param traffic_light Ponteiro para o controlador global de semáforos.
 * @param position Coordenada da célula WAIT a ser localizada.
 *
 * @retval >= 0 Índice da luz encontrada no vetor de luzes.
 * @retval -1 Não existe nenhuma luz associada à coordenada informada.
 */
static int find_light(TrafficLight *traffic_light, Coord position) {

  /**
   * @brief Valida os parâmetros recebidos.
   *
   * @details
   * Caso o controlador seja inválido, nenhuma busca pode ser realizada.
   */
  if (traffic_light == NULL) {
    return -1;
  }

  /**
   * @brief Percorre todas as luzes cadastradas.
   *
   * @details
   * O controlador mantém um vetor linear contendo o estado de todos os
   * semáforos da simulação. Cada elemento corresponde a um único
   * WaitPoint existente na malha viária.
   */
  for (int i = 0; i < traffic_light->light_count; i++) {

    /**
     * @brief Verifica se a posição da luz coincide com a coordenada
     * procurada.
     *
     * @details
     * A comparação é realizada utilizando as componentes X e Y da
     * coordenada. Havendo igualdade em ambas, a luz correspondente foi
     * encontrada.
     */
    if (traffic_light->lights[i].wait_point.cell.x == position.x &&
        traffic_light->lights[i].wait_point.cell.y == position.y) {

      /**
       * @brief Retorna o índice da luz localizada.
       *
       * @details
       * O índice retornado poderá ser utilizado para acessar
       * diretamente a estrutura TrafficLightState correspondente.
       */
      return i;
    }
  }

  /**
   * @brief Nenhuma luz foi encontrada.
   *
   * @details
   * A coordenada informada não pertence a nenhum WaitPoint cadastrado
   * pelo controlador.
   */
  return -1;
}
