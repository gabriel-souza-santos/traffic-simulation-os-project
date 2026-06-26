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
  unsigned long current_tick; /**< Tick atual da simulação. */
} GlobalClock;

// ============================================================================
// Funções de Acesso (Getters/Setters) para o tipo opaco Vehicle
// @note A implementação destas funções DEVE ficar em 'vehicle.c'.
// ============================================================================
int vehicle_get_x(const Vehicle *vehicle);
int vehicle_get_y(const Vehicle *vehicle);
VehicleType vehicle_get_type(const Vehicle *vehicle);
int vehicle_get_direction(const Vehicle *vehicle);
void vehicle_set_position(Vehicle *vehicle, int x, int y);

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
bool should_move_vehicle(const Vehicle *vehicle, unsigned long current_tick);

/**
 * @brief Verifica se uma célula de destino é adjacente direta (não diagonal).
 */
bool is_adjacent_cell(const Vehicle *vehicle, int target_x, int target_y);

/**
 * @brief Verifica se a célula de destino não está bloqueada ou ocupada.
 */
bool is_cell_available(int target_x, int target_y);

/**
 * @brief Analisa a célula imediatamente à frente do veículo com base na sua
 * direção.
 */
bool has_vehicle_ahead(const Vehicle *vehicle);

/**
 * @brief Impede mudanças de faixa ilegais (ultrapassagens) em vias de mão
 * única.
 */
bool is_overtaking(const Vehicle *vehicle, int target_x, int target_y);

/**
 * @brief Executa a movimentação atômica do veículo utilizando ordenação de
 * locks contra deadlocks.
 */
bool vehicle_move(Vehicle *vehicle, int target_x, int target_y,
                 const GlobalClock *clock);

#endif // URBAN_TRAFFIC_VEHICLE_MOVEMENT_H
