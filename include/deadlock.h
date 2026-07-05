#ifndef URBAN_TRAFFIC_DEADLOCK_H
#define URBAN_TRAFFIC_DEADLOCK_H

#include <stdbool.h>

#include "vehicle.h"

bool try_move_vehicle(Vehicle *vehicle,
                      int next_x,
                      int next_y);

#endif