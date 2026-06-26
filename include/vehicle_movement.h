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

// TODO: Verificar se esta estrutura será unificada com o tipo opaco/privado
// 'Clock'
/**
 * @brief Representa o relógio global discreto da simulação.
 * definido em 'clock.h' para evitar redundâncias no controle de tempo.
 */
typedef struct {
  unsigned long currentTick; /**< Tick atual da simulação. */
} GlobalClock;

// ============================================================================
// Funções de Acesso (Getters/Setters) para o tipo opaco Vehicle
// @note A implementação destas funções DEVE ficar em 'vehicle.c'.
// ============================================================================
int vehicle_getX(const Vehicle *vehicle);
int vehicle_getY(const Vehicle *vehicle);
VehicleType vehicle_getType(const Vehicle *vehicle);
int vehicle_getDirection(const Vehicle *vehicle);
void vehicle_setPosition(Vehicle *vehicle, int x, int y);

// ============================================================================
// Regras de Movimentação e Validação de Tráfego
// ============================================================================

/**
 * @brief Verifica se um veículo pode se mover no tick atual baseado na sua
 * velocidade.
 *
 * - AMBULANCE / CAR_FAST: move-se a cada 1 tick.
 * - CAR_MEDIUM: move-se a cada 2 ticks.
 * - CAR_SLOW: move-se a cada 4 ticks.
 */
bool shouldMoveVehicle(const Vehicle *vehicle, unsigned long currentTick);

/**
 * @brief Verifica se uma célula de destino é adjacente direta (não diagonal).
 */
bool isAdjacentCell(const Vehicle *vehicle, int targetX, int targetY);

/**
 * @brief Verifica se a célula de destino não está bloqueada ou ocupada.
 */
bool isCellAvailable(int targetX, int targetY);

/**
 * @brief Analisa a célula imediatamente à frente do veículo com base na sua
 * direção.
 */
bool hasVehicleAhead(const Vehicle *vehicle);

/**
 * @brief Impede mudanças de faixa ilegais (ultrapassagens) em vias de mão
 * única.
 */
bool isOvertaking(const Vehicle *vehicle, int targetX, int targetY);

/**
 * @brief Executa a movimentação atômica do veículo utilizando ordenação de
 * locks contra deadlocks.
 */
bool moveVehicle(Vehicle *vehicle, int targetX, int targetY,
                 const GlobalClock *clock);

#endif // URBAN_TRAFFIC_VEHICLE_MOVEMENT_H
