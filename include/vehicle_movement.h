/**
 * @file vehicle_movement.h
 * @brief Regras de deslocamento dos veículos na simulação.
 *
 * @date 2026-06-25
 */
#ifndef URBAN_TRAFFIC_VEHICLE_MOVEMENT_H
#define URBAN_TRAFFIC_VEHICLE_MOVEMENT_H

#include "vehicle.h"
#include <stdbool.h>

/**
 * @brief Representa o relógio global discreto da simulação.
 *
 * Todos os veículos utilizam o valor atual do tick para determinar
 * quando devem executar um movimento.
 */
typedef struct {
  unsigned long currentTick; /**< Tick atual da simulação. */
} GlobalClock;

/**
 * @brief Verifica se um veículo pode se mover no tick atual.
 *
 * A frequência de movimentação depende do tipo do veículo:
 * - CAR_FAST: a cada 1 tick.
 * - CAR_MEDIUM: a cada 2 ticks.
 * - CAR_SLOW: a cada 4 ticks.
 *
 * @param vehicle Veículo analisado.
 * @param currentTick Tick atual do relógio global.
 *
 * @return true se o veículo deve se mover.
 * @return false caso contrário.
 */
bool shouldMoveVehicle(const Vehicle *vehicle, unsigned long currentTick);

/**
 * @brief Verifica se uma célula é adjacente à posição atual do veículo.
 *
 * Essa função impede teletransporte e garante que um veículo
 * avance apenas para células vizinhas.
 *
 * @param vehicle Veículo que deseja se mover.
 * @param targetX Coordenada X de destino.
 * @param targetY Coordenada Y de destino.
 *
 * @return true se a célula é adjacente.
 * @return false caso contrário.
 */
bool isAdjacentCell(const Vehicle *vehicle, int targetX, int targetY);

/**
 * @brief Verifica se a célula de destino está livre.
 *
 * Dois veículos não podem ocupar a mesma célula simultaneamente.
 *
 * @param targetX Coordenada X de destino.
 * @param targetY Coordenada Y de destino.
 *
 * @return true se a célula está livre.
 * @return false caso contrário.
 */
bool isCellAvailable(int targetX, int targetY);

/**
 * @brief Verifica se existe um veículo imediatamente à frente.
 *
 * Utilizada para impedir que um veículo atravesse ou pule outro.
 *
 * @param vehicle Veículo analisado.
 *
 * @return true se existe um veículo à frente.
 * @return false caso contrário.
 */
bool hasVehicleAhead(const Vehicle *vehicle);

/**
 * @brief Verifica se o movimento configuraria uma ultrapassagem.
 *
 * Em vias de mão única, um veículo não pode ultrapassar outro
 * veículo que esteja à sua frente.
 *
 * @param vehicle Veículo analisado.
 * @param targetX Coordenada X de destino.
 * @param targetY Coordenada Y de destino.
 *
 * @return true se houver tentativa de ultrapassagem.
 * @return false caso contrário.
 */
bool isOvertaking(const Vehicle *vehicle, int targetX, int targetY);

/**
 * @brief Move um veículo para uma nova posição.
 *
 * O movimento só deve ocorrer se:
 * - O tick permitir movimentação.
 * - A célula de destino for adjacente.
 * - A célula estiver livre.
 * - Não houver salto sobre outro veículo.
 * - Não houver ultrapassagem em mão única.
 *
 * @param vehicle Veículo a ser movimentado.
 * @param targetX Coordenada X de destino.
 * @param targetY Coordenada Y de destino.
 * @param clock Relógio global da simulação.
 *
 * @return true se o movimento foi realizado.
 * @return false caso contrário.
 */
bool moveVehicle(Vehicle *vehicle, int targetX, int targetY,
                 const GlobalClock *clock);

#endif // URBAN_TRAFFIC_VEHICLE_MOVEMENT_H
