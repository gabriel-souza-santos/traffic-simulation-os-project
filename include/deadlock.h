#ifndef URBAN_TRAFFIC_DEADLOCK_H
#define URBAN_TRAFFIC_DEADLOCK_H

#include <stdbool.h>
#include "map.h"
#include "vehicle.h"

/**
 * @brief Inicializa o módulo de análise de deadlock.
 */
void deadlock_init(const Map *map);

/**
 * @brief Valida se um movimento de veículo é seguro.
 *
 */
bool deadlock_validate_move(Map *map, Coord from, Coord to);

#endif