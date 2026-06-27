#include "deadlock.h"

#include <pthread.h>

#include "map.h"

/*
 * Gera um identificador único para uma célula.
 * Utilizado para definir uma ordem global de aquisição.
 */
static int cell_id(int x, int y)
{
    return y * MAP_WIDTH + x;
}

bool try_move_vehicle(Vehicle *vehicle,
                      int next_x,
                      int next_y)
{
    MapCell *current = &map[vehicle->y][vehicle->x];
    MapCell *target = &map[next_y][next_x];

    pthread_mutex_t *first_lock;
    pthread_mutex_t *second_lock;

    int current_id = cell_id(vehicle->x, vehicle->y);
    int target_id = cell_id(next_x, next_y);

    /*
     * Ordem global fixa.
     * Todos os veículos adquirem recursos
     * exatamente na mesma ordem.
     */
    if(current_id < target_id)
    {
        first_lock = &current->lock;
        second_lock = &target->lock;
    }
    else
    {
        first_lock = &target->lock;
        second_lock = &current->lock;
    }

    pthread_mutex_lock(first_lock);

    if(pthread_mutex_trylock(second_lock) != 0)
    {
        pthread_mutex_unlock(first_lock);
        return false;
    }

    /*
     * Verifica se destino está livre.
     */
    if(target->is_occupied)
    {
        pthread_mutex_unlock(second_lock);
        pthread_mutex_unlock(first_lock);
        return false;
    }

    /*
     * Atualiza ocupação.
     */
    current->is_occupied = false;
    target->is_occupied = true;

    vehicle->x = next_x;
    vehicle->y = next_y;

    pthread_mutex_unlock(second_lock);
    pthread_mutex_unlock(first_lock);

    return true;
}



