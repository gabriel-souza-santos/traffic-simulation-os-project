/**
 * @file vehicle_movement.c
 * @brief Implementação das regras de deslocamento sincronizado dos veículos.
 *
 * @date 2026-06-26
 */

#include "vehicle_movement.h"
#include "debug.h"
#include "map.h"
#include "vehicle.h"
#include <stdlib.h>

/**
 * @brief Verifica se um veículo está autorizado a mover-se no tick atual com
 * base na sua velocidade.
 *
 * @param vehicle Ponteiro constante para o veículo analisado.
 * @param currentTick O ciclo/tick atual do relógio global da simulação.
 *
 * @return true
 * - Se o veículo for uma ambulância (AMBULANCE) ou carro rápido (CAR_FAST),
 * pois movem-se sempre (todos os ticks).
 * - Se for um carro médio (CAR_MEDIUM) e o tick atual for par (múltiplo de 2).
 * - Se for um carro lento (CAR_SLOW) e o tick atual for múltiplo de 4.
 * @return false
 * - Se o ponteiro do veículo fornecido for inválido (`NULL`).
 * - Se o tipo do veículo for desconhecido/inválido (`default`).
 * - Se as condições matemáticas de divisão do tick para carros médios ou lentos
 * não forem satisfeitas.
 */
bool shouldMoveVehicle(const Vehicle *vehicle, unsigned long currentTick) {
  if (vehicle == NULL)
    return false;

  VehicleType vehicle_type = vehicle_getType(vehicle);

  // Vai verificar qual tick estamos.
  switch (vehicle_type) {
  case AMBULANCE:
  case CAR_FAST:
    return true;
  case CAR_MEDIUM:
    return (currentTick % 2 == 0);
  case CAR_SLOW:
    return (currentTick % 4 == 0);
  default:
    return false;
  }
}

/**
 * @brief Verifica se a célula destino é um vizinho ortogonal direto (adjacente
 * não diagonal).
 *
 * @param vehicle Ponteiro constante para o veículo que pretende mover-se.
 * @param targetX Coordenada X do destino alvo na matriz.
 * @param targetY Coordenada Y do destino alvo na matriz.
 *
 * @return true
 * - Se o destino estiver exatamente a 1 unidade de distância ortogonal (Cima,
 * Baixo, Esquerda ou Direita) em relação à posição atual do veículo.
 * @return false
 * - Se o ponteiro do veículo for inválido (`NULL`).
 * - Se as coordenadas de destino estiverem fora dos limites físicos da matriz
 * do mapa (limites globais).
 * - Se o movimento tentado for diagonal ou se o destino for a própria célula
 * atual do veículo (distância != 1).
 */
bool isAdjacentCell(const Vehicle *vehicle, int targetX, int targetY) {
  if (vehicle == NULL)
    return false;

  // Garante o respeito aos limites da matriz global do mapa
  if (targetX < 0 || targetX >= MAP_WIDTH || targetY < 0 ||
      targetY >= MAP_HEIGHT) {
    return false;
  }

  int currentX = vehicle_getX(vehicle);
  int currentY = vehicle_getY(vehicle);

  int diffX = abs(targetX - currentX);
  int diffY = abs(targetY - currentY);

  // Movimento ortogonal direto: a soma das diferenças absolutas deve ser
  // exatamente 1
  return (diffX + diffY == 1);
}

/**
 * @brief Verifica de forma segura e sincronizada se uma coordenada do mapa está
 * livre para tráfego.
 *
 * @param targetX Coordenada X a ser inspecionada.
 * @param targetY Coordenada Y a ser inspecionada.
 *
 * @return true
 * - Se a célula destino for transitável (diferente de TILE_BLOCKED) E não
 * estiver atualmente ocupada por outro veículo (`is_occupied == false`).
 * @return false
 * - Se as coordenadas alvo ultrapassarem os limites permitidos do mapa.
 * - Se a célula for um bloco intransitável (parede/obstáculo).
 * - Se a célula já contiver um veículo parado ou passando por ela.
 * * @note Esta função pode travar a execução do programa (via macro `TRY`) caso
 * ocorra uma falha crítica no sistema de threads ao tentar trancar ou
 * destrancar o Mutex (`lock`) da célula.
 */
bool isCellAvailable(int targetX, int targetY) {
  if (targetX < 0 || targetX >= MAP_WIDTH || targetY < 0 ||
      targetY >= MAP_HEIGHT) {
    return false;
  }

  bool available = false;

  // Exclusão mútua individual por célula para leitura segura do estado
  TRY(pthread_mutex_lock(&map[targetY][targetX].lock));

  if (map[targetY][targetX].tile != TILE_BLOCKED &&
      !map[targetY][targetX].is_occupied) {
    available = true;
  }

  TRY(pthread_mutex_unlock(&map[targetY][targetX].lock));
  return available;
}

/**
 * @brief Projeta a trajetória frontal do veículo e checa se há um obstáculo
 * móvel imediatamente à frente.
 *
 * @param vehicle Ponteiro constante para o veículo cuja frente será analisada.
 *
 * @return true
 * - Se a célula imediatamente à frente (com base na direção atual do veículo)
 * estiver ocupada por outro veículo (`is_occupied == true`).
 * @return false
 * - Se o ponteiro do veículo for inválido (`NULL`).
 * - Se o veículo estiver sem direção mapeada ou parado (`default` no switch).
 * - Se a célula à frente estiver fora das dimensões limítrofes do mapa.
 * - Se a célula à frente estiver totalmente desocupada.
 * * @note Assim como `isCellAvailable`, esta função faz uso seguro de exclusão
 * mútua (`lock`) e pode abortar o programa via macro `TRY` em caso de falha de
 * concorrência com pthreads.
 */
bool hasVehicleAhead(const Vehicle *vehicle) {
  if (vehicle == NULL)
    return false;

  int currentX = vehicle_getX(vehicle);
  int currentY = vehicle_getY(vehicle);
  int dir = vehicle_getDirection(vehicle);

  // Variaveis temporarias , para calcular  a posição da frente do veivulo.
  int nextX = currentX;
  int nextY = currentY;

  // Projetar a coordenada no mapa baseada na orientação atual
  switch (dir) {
  case DIRECTION_UP:
    nextY--;
    break; // Sobe uma linha na matriz
  case DIRECTION_DOWN:
    nextY++;
    break; // Desce uma linha na matriz
  case DIRECTION_LEFT:
    nextX--;
    break; // Recua uma coluna
  case DIRECTION_RIGHT:
    nextX++;
    break; // Avança uma coluna
  default:
    return false; // Se o carro estiver parado/sem direção, não há nada "à
                  // frente"
  }
  // Aborta caso a projeção saia dos limites físicos do mapa da simulação
  if (nextX < 0 || nextX >= MAP_WIDTH || nextY < 0 || nextY >= MAP_HEIGHT) {
    return false;
  }

  bool aheadOccupied = false;
  TRY(pthread_mutex_lock(&map[nextY][nextX].lock));
  aheadOccupied = map[nextY][nextX].is_occupied;
  TRY(pthread_mutex_unlock(&map[nextY][nextX].lock));

  return aheadOccupied;
}

/**
 * @brief Determina se o movimento pretendido caracteriza uma ultrapassagem
 * lateral proibida em vias de sentido único.
 *
 * @param vehicle Ponteiro constante para o veículo analisado.
 * @param targetX Coordenada X de destino pretendida.
 * @param targetY Coordenada Y de destino pretendida.
 *
 * @return true
 * - Se o veículo estiver numa via estrita vertical (`^` ou `v`) e tentar
 * mover-se para os lados (`targetX != currentX`).
 * - Se o veículo estiver numa via estrita horizontal (`<` ou `>`) e tentar
 * mover-se para cima/baixo (`targetY != currentY`).
 * @return false
 * - Se o ponteiro do veículo for inválido (`NULL`).
 * - Se o veículo estiver em cruzamentos abertos (`TILE_ROAD`) ou pontos de
 * parada/espera (`TILE_WAIT`), onde desvios e conversões direcionais são
 * explicitamente permitidos.
 * - Se o movimento seguir em linha reta respeitando o fluxo natural daquela
 * única faixa.
 */
bool isOvertaking(const Vehicle *vehicle, int targetX, int targetY) {
  if (vehicle == NULL)
    return false;

  int currentX = vehicle_getX(vehicle);
  int currentY = vehicle_getY(vehicle);

  TileType currentTile = map[currentY][currentX].tile;

  // Interseções (.) e pontos de espera (!) permitem conversões/mudanças livres
  if (currentTile == TILE_ROAD || currentTile == TILE_WAIT) {
    return false;
  }

  // Em vias direcionais estritas (^, v, <, >), desvios laterais configuram
  // ultrapassagem proibida
  if (currentTile == TILE_ROAD_UP || currentTile == TILE_ROAD_DOWN) {
    if (targetX != currentX)
      return true; // Tentativa de desvio para a esquerda/direita em fluxo
                   // vertical
  }

  if (currentTile == TILE_ROAD_LEFT || currentTile == TILE_ROAD_RIGHT) {
    if (targetY != currentY)
      return true; // Tentativa de desvio para cima/baixo em fluxo horizontal
  }

  return false;
}

/**
 * @brief Orquestra e executa o deslocamento de um veículo no mapa de
 * forma segura impedindo Deadlocks.
 *
 * @param vehicle Ponteiro para o objeto do veículo que executará a ação.
 * @param targetX Coordenada X da célula destino.
 * @param targetY Coordenada Y da célula destino.
 * @param clock Ponteiro constante para a estrutura do relógio global.
 *
 * @return true
 * - Se todas as validações (velocidade, adjacência, regras de trânsito)
 * passarem com sucesso, os locks forem adquiridos e a célula estiver livre. O
 * mapa global e a posição do veículo são alterados de forma atómica.
 * @return false
 * - Se os ponteiros `vehicle` ou `clock` forem nulos.
 * - Se o veículo não puder mover-se neste ciclo específico (`shouldMoveVehicle`
 * falhar).
 * - Se o destino não for adjacente (`isAdjacentCell` falhar).
 * - Se o movimento violar regras de ultrapassagem (`isOvertaking` retornar
 * verdadeiro).
 * - Se, após trancar a região crítica (Double-Check), a célula destino tiver
 * mudado de estado e se tornado ocupada ou bloqueada por outra thread
 * concorrente.
 * * @note Esta função bloqueia a execução da thread até adquirir
 * simultaneamente os locks da célula atual e da célula destino. A ordenação por
 * endereço de memória (`&map... < &map...`) impede a ocorrência de Deadlocks em
 * disputas circulares. Aborta o programa caso ocorra um erro fatal de mutex do
 * pthreads.
 */
bool moveVehicle(Vehicle *vehicle, int targetX, int targetY,
                 const GlobalClock *clock) {
  if (vehicle == NULL || clock == NULL)
    return false;

  // 1. Valida restrições temporais de velocidade
  if (!shouldMoveVehicle(vehicle, clock->currentTick)) {
    return false;
  }

  // 2. Valida restrições físicas de adjacência
  if (!isAdjacentCell(vehicle, targetX, targetY)) {
    return false;
  }

  // 3. Valida regras de trânsito (ultrapassagem proibida)
  if (isOvertaking(vehicle, targetX, targetY)) {
    return false;
  }

  int currentX = vehicle_getX(vehicle);
  int currentY = vehicle_getY(vehicle);

  // 4. Prevenção Absoluta de Deadlocks: Ordenação Baseada no Endereço de
  // Memória dos Locks. Garante que se dois veículos adjacentes tentarem
  // disputar as mesmas células invertidas, eles sempre travarão os mutexes
  // exatamente na mesma ordem cronológica.
  if (&map[currentY][currentX].lock < &map[targetY][targetX].lock) {
    TRY(pthread_mutex_lock(&map[currentY][currentX].lock));
    TRY(pthread_mutex_lock(&map[targetY][targetX].lock));
  } else {
    TRY(pthread_mutex_lock(&map[targetY][targetX].lock));
    TRY(pthread_mutex_lock(&map[currentY][currentX].lock));
  }

  bool moveSuccess = false;

  // 5. Verificação em dupla checagem (Double-checked locking) dentro da seção
  // crítica
  if (map[targetY][targetX].tile != TILE_BLOCKED &&
      !map[targetY][targetX].is_occupied) {

    // Alteração atômica do estado da malha viária global
    map[currentY][currentX].is_occupied = false;
    map[targetY][targetX].is_occupied = true;

    // Sincronização dos metadados internos do veículo correspondente
    vehicle_setPosition(vehicle, targetX, targetY);
    moveSuccess = true;
  }

  // 6. Liberação dos recursos na ordem inversa de aquisição
  TRY(pthread_mutex_unlock(&map[currentY][currentX].lock));
  TRY(pthread_mutex_unlock(&map[targetY][targetX].lock));

  return moveSuccess;
}
