/**
 * @file deadlock.c
 *
 * @brief Implementação da estratégia look-ahead para prevenção de deadlocks.
 *
 * @date 2026-07-05
 * @author sarahmendes-ufca
 */
#include "deadlock.h"
#include <stdlib.h>
/**
 * Referência global opcional para o mapa.
 * Atualmente não utilizada na lógica de validação.
 * Mantida apenas para compatibilidade com possíveis extensões futuras.
 */
static const Map *g_map = NULL;

/**
 * Inicializa o módulo de validação.
 * Armazena referência ao mapa caso futuras versões implementem
 * estratégias mais avançadas de análise de tráfego.
 */
void deadlock_init(const Map *map)
{
    g_map = map;
}

/**
 * Regra principal de prevenção de deadlock.
 */

bool deadlock_validate_move(Map *map, Coord from, Coord to)
{
    if (!map ||
        !map_is_within_bounds(map, from) ||
        !map_is_within_bounds(map, to))
    {
        return false;
    }

    if (map_is_blocked(map, to))
        return false;

    if (map_is_occupied(map, to))
        return false;

    return true;
}