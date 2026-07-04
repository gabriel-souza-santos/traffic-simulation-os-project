#include "deadlock.h"

/// @brief 
/// @param map 
/// @param current 
/// @param target 
/// @return 
bool try_move_vehicle(
    Map *map,
    Coord current,
    Coord target)
{
    if (map == NULL)
    {
        return false;
    }

    if (current.x == target.x &&
        current.y == target.y)
    {
        return true;
    }

    if (!map_is_within_bounds(map, current))
    {
        return false;
    }

    if (!map_is_within_bounds(map, target))
    {
        return false;
    }

    return map_transfer_occupant(
        map,
        current,
        target
    );
}