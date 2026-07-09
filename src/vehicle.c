/**
 * @file vehicle.c
 *
 * @brief Implementação do ciclo de vida e regras de deslocamento sincronizado dos veículos.
 *
 * @author José Dhonatan
 * @author Leticía Dias
 *
 * @date 2026-06-25
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "clock.h"
#include "debug.h"
#include "map.h"
#include "vehicle.h"

/**
 * @internal
 * @brief Representa o estado de um veículo num dado instante da simulação.
 */
struct Vehicle {
    int id;              /**< Identificador único do veículo. */
    Coord position;      /**< Posição atual na malha do mapa. */
    Direction direction; /**< Direção de movimento atual. */
    VehicleType type;    /**< Tipo do veículo (define velocidade e prioridade). */
};


/**
 * @internal
 * @brief Deriva a direção de movimento a partir do tipo de tile.
 *
 * Mapeia diretamente os quatro tipos de via direcional (TILE_ROAD_UP/
 * DOWN/LEFT/RIGHT) para o Direction correspondente.
 *
 * @param tile_type Tipo de tile a ser interpretado.
 * @return A direção correspondente, ou DIRECTION_NONE se o tile não for
 *         uma via com sentido definido (ex: TILE_ROAD, TILE_WAIT, TILE_BLOCKED).
 */
static Direction find_direction_from_tile(const TileType tile_type) {
    switch (tile_type) {
        case TILE_ROAD_UP:      return DIRECTION_UP;
        case TILE_ROAD_DOWN:    return DIRECTION_DOWN;
        case TILE_ROAD_LEFT:    return DIRECTION_LEFT;
        case TILE_ROAD_RIGHT:   return DIRECTION_RIGHT;
        default:                return DIRECTION_NONE;
    }
}

/**
 * @internal
 * @brief Projeta a coordenada seguinte no mapa com base na direção atual.
 *
 * Calcula a próxima posição aplicando um deslocamento de uma unidade a
 * partir de @p vehicle->position, na direção indicada por @p vehicle->direction.
 * Se a direção for @c DIRECTION_NONE (ou outro valor não mapeado), retorna a
 * própria posição atual sem alteração.
 *
 * @param vehicle Veículo cuja posição e direção atuais servem de base
 *                para a projeção.
 * @return A coordenada adjacente na direção do veículo, ou a posição
 *         atual inalterada caso a direção seja inválida/nula.
 *
 * @warning Não faz verificação de limites do mapa; a coordenada retornada
 *          pode estar fora dos limites da malha. A responsabilidade de
 *          validar é do chamador.
 *
 * @see map_is_within_bounds()
 */
static Coord find_next_position(const Vehicle *vehicle) {
    Coord next = vehicle->position;

    switch (vehicle->direction) {
        case DIRECTION_UP:      next.y--;   break;
        case DIRECTION_DOWN:    next.y++;   break;
        case DIRECTION_LEFT:    next.x--;   break;
        case DIRECTION_RIGHT:   next.x++;   break;
        default:                            break;
    }

    return next;
}

/**
 * @internal
 * @brief Verifica se um veículo está autorizado a mover-se no tick atual com
 * base na sua velocidade.
 *
 * @param vehicle Ponteiro constante para o veículo analisado.
 * @param clock Relógio global, usado para consultar o tick atual.
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
static bool should_move_now(const Vehicle *vehicle, const Clock *clock) {
    if (!vehicle) return false;

    const size_t current_tick = clock_get_tick(clock);

    switch (vehicle->type) {
        case AMBULANCE:     return true;
        case CAR_FAST:      return true;
        case CAR_MEDIUM:    return current_tick % 2 == 0;
        case CAR_SLOW:      return current_tick % 4 == 0;
        default:            return false;
    }
}

/**
 * @internal
 * @brief Verifica se a célula destino é um vizinho ortogonal direto.
 *
 * @param map Mapa usado para validar os limites do destino.
 * @param vehicle Veículo cuja posição atual serve de referência.
 * @param target Coordenada candidata a destino.
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
static bool is_adjacent(const Map *map, const Vehicle *vehicle, const Coord target) {
    if (!map || !vehicle) return false;

    // Garante o respeito aos limites da matriz global do mapa
    if (map_is_within_bounds(map, target) == false) {
        return false;
    }

    const Coord current = vehicle->position;

    const int diff_x = abs(target.x - current.x);
    const int diff_y = abs(target.y - current.y);

    // Movimento ortogonal direto: a soma das diferenças absolutas deve ser
    // exatamente 1
    return diff_x + diff_y == 1;
}

/**
 * @internal
 * @brief Projeta a trajetória frontal do veículo e checa se há um obstáculo
 * móvel imediatamente à frente.
 *
 * @param map Mapa usado para consultar ocupação da célula frontal.
 * @param vehicle Veículo cuja direção atual define a célula a ser checada.
 *
 * @return true
 * - Se a célula imediatamente à frente (com base na direção atual do veículo)
 * estiver ocupada por outro veículo (`is_occupied == true`).
 * @return false
 * - Se o ponteiro do veículo for inválido (`NULL`).
 * - Se o veículo estiver sem direção mapeada ou parado (`default` no switch).
 * - Se a célula à frente estiver fora das dimensões limítrofes do mapa.
 * - Se a célula à frente estiver totalmente desocupada.
 *
 * @note Assim como `is_cell_available`, esta função faz uso seguro de exclusão
 * mútua (`lock`) e pode abortar o programa via macro `TRY` em caso de falha de
 * concorrência com pthreads.
 */
static bool has_vehicle_ahead(Map *map, const Vehicle *vehicle) {
    if (!vehicle || !map) return false;

    const Coord next_position = find_next_position(vehicle);

    // Aborta caso a projeção saia dos limites físicos do mapa da simulação
    if (map_is_within_bounds(map, next_position) == false) {
        return false;
    }

    return map_is_occupied(map, next_position);
}

/**
 * @internal
 * @brief Determina se o movimento pretendido caracteriza ultrapassagem
 * lateral proibida em vias de sentido único.
 * ...
 * @param map Mapa usado para consultar o tipo da célula atual do veículo.
 * @param vehicle Veículo cuja via atual será analisada.
 * @param target Coordenada de destino pretendida.
 *
 * @return true
 * - Se o veículo estiver numa via estrita vertical (`^` ou `v`) e tentar
 * mover-se para os lados.
 * - Se o veículo estiver numa via estrita horizontal (`<` ou `>`) e tentar
 * mover-se para cima/baixo.
 * @return false
 * - Se o ponteiro do veículo for inválido (`NULL`).
 * - Se o veículo estiver em cruzamentos abertos (`TILE_ROAD`) ou pontos de
 * parada/espera (`TILE_WAIT`), onde desvios e conversões direcionais são
 * explicitamente permitidos.
 * - Se o movimento seguir em linha reta respeitando o fluxo natural daquela
 * única faixa.
 */
static bool is_overtaking(const Map *map, const Vehicle *vehicle, const Coord target) {
    if (!vehicle) return false;

    const Coord current_position = vehicle->position;
    const TileType current_tile = map_get_tile_type(map, current_position);

    // Interseções e pontos de espera permitem conversões/mudanças livres
    if (current_tile == TILE_ROAD || current_tile == TILE_WAIT) {
        return false;
    }

    // Em vias direcionais estritas (^, v, <, >), desvios laterais
    // configuram ultrapassagem proibida
    if (current_tile == TILE_ROAD_UP || current_tile == TILE_ROAD_DOWN) {
        if (target.x != current_position.x) {
            return true;    // Tentativa de desvio para a esquerda/direita
        }                   // em fluxo vertical
    }

    if (current_tile == TILE_ROAD_LEFT || current_tile == TILE_ROAD_RIGHT) {
        if (target.y != current_position.y) {
            return true;    // Tentativa de desvio para cima/baixo
        }                   // em fluxo horizontal
    }

    return false;
}



    if (vehicle == NULL || clock == NULL)
        return false;



    vehicle->position = target;
    return true;
}

/*
 * ============================================================================
 * API Pública
 * ============================================================================
 */

/**
 * @internal
 * @brief Implementação da criação de um veículo.
 *
 * Atribui o tipo do veículo de forma determinística para os 4 primeiros IDs
 * (garantindo ao menos uma ambulância e um carro de cada velocidade), e
 * aleatória para os demais. Reserva um ponto de spawn livre via
 * map_reserve_spawn_point e deriva a direção inicial a partir do tipo de
 * tile ocupado.
 *
 * @warning Retorna NULL (liberando a memória já alocada) se não houver
 *          spawn point livre, ou se o tile reservado não mapear para uma
 *          direção válida.
 */
Vehicle *vehicle_new(Map *map, const int id) {
    // Aloca e inicializa as propriedades de um veículo.
    // CRITÉRIO: Cada veículo possui identificador, posição, direção, velocidade, tipo, e rota.

    Vehicle *vehicle = malloc(sizeof(Vehicle));
    if (!vehicle) {
        LOG("Error: failed to allocate memory for 'vehicle'");
        return NULL;
    }

    vehicle->id = id;

    if (id == 0) {
        vehicle->type = AMBULANCE;
    } else {
        switch ((id - 1) % 3) {
            case 0:
                vehicle->type = CAR_FAST;
                break;
            case 1:
                vehicle->type = CAR_MEDIUM;
                break;
            case 2:
                vehicle->type = CAR_SLOW;
                break;
            default:
                vehicle->type = NO_VEHICLE;
                break;
        }
    }

    if (vehicle->type == NO_VEHICLE) {
        LOG("Error: 'vehicle->type' have not been properly defined.");
        free(vehicle);
        return NULL;
    }

    vehicle->position = map_reserve_spawn_point(map);

    if (vehicle->position.x == NULL_COORD.x ||
        vehicle->position.y == NULL_COORD.y) {
        free(vehicle);
        return NULL;
    }

    const TileType tile = map_get_tile_type(map, vehicle->position);
    vehicle->direction = find_direction_from_tile(tile);

    if (vehicle->direction == DIRECTION_NONE) {
        LOG("Error: 'vehicle->direction' have not been properly defined.");
        free(vehicle);
        return NULL;
    }

    return vehicle;
}

/**
 * @internal
 * @brief Implementação da liberação dos recursos do veículo.
 */
void vehicle_destroy(Vehicle *vehicle) {
    // Libera a memória alocada para o contexto do veículo.
    LOG_IF(vehicle == NULL, "Warning: parameter 'vehicle' is NULL on destroy.");
    free(vehicle);
}


void *vehicle_update(void *vehicle_args) {
    if (!vehicle_args) {
        LOG("Error: parameter 'vehicle' is NULL.");
        return NULL;
    }

    const VehicleArgs *args = (VehicleArgs *)vehicle_args;

    if (!args->shared) {
        LOG("Error: thread argument 'shared' is NULL");
        return NULL;
    }

    if (!args->shared->analyser) {
        LOG("Error: thread argument 'shared->analyser' is NULL");
        return NULL;
    }

    if (!args->shared->clock) {
        LOG("Error: thread argument 'shared->clock' is NULL");
        return NULL;
    }

    if (!args->shared->map) {
        LOG("Error: thread argument 'shared->map' is NULL");
        return NULL;
    }

    Analyser *analyser = args->shared->analyser;
    Clock *clock = args->shared->clock;
    Map *map = args->shared->map;

    Vehicle *vehicle = args->vehicle;

    const int id = vehicle->id;

    for (int t = 0; t < TICKS; t++) {
        const size_t current_tick = clock_get_tick(clock);

        // Coordenada atual (origem)
        const Coord current_position = vehicle->position;
        Coord target_position = current_position;

        // Verifica se o veículo DEVE e PODE tentar se mover neste tick
        if (should_move_now(vehicle, clock)) {
            const Coord intended_target = find_next_position(vehicle);

            // Validações locais (Física e Fronteiras)
            if (!map_is_within_bounds(map, intended_target)) {
                LOG("Warning: vehicle(%d) tried to go out of the map bounds.", id);
            }
            else if (!is_adjacent(map, vehicle, intended_target)) {
                LOG("Warning: vehicle(%d) tried an illegal teleport (not adjacent).", id);
            }
            else if (is_overtaking(map, vehicle, intended_target)) {
                LOG("Info: vehicle(%d) detected forbidden overtaking scheme.", id);
            }
            else {
                target_position = intended_target;
            }
        }

        // Monta a requisição e envia ao Analisador
        const MovementRequest request = {
            .from = current_position,
            .to = target_position,
            .status = REQUEST_PENDING,
        };

        // Bloqueia a thread do veículo até que o Analisador decida por todos
        analyser_request(analyser, id, request);
        const RequestStatus verdict = analyser_get_status(analyser, id);

        if (verdict == REQUEST_APPROVED && (current_position.x != target_position.x || current_position.y != target_position.y)) {
            vehicle->position = target_position;

            // Atualiza a direção com base no novo tile
            const TileType target_tile = map_get_tile_type(map, vehicle->position);
            const Direction new_direction = find_direction_from_tile(target_tile);

            if (new_direction != DIRECTION_NONE) {
                vehicle->direction = new_direction;
            }
            // else {
            //     LOG("Warning: vehicle(%d) does not have a defined direction at x:%d, y:%d.",
            //         id, vehicle->position.x, vehicle->position.y);
            // }
        }

        clock_signal(clock, current_tick);
    }

    return NULL;
}

Coord vehicle_get_priority_coord(void) {
  // TODO
}

Coord vehicle_get_position(const Vehicle *vehicle) {
  if (vehicle == NULL) {
    return NULL_COORD;
  }

  return vehicle->position;
}

VehicleType vehicle_get_type(const Vehicle *vehicle) {
  if (vehicle == NULL) {
    return NO_VEHICLE;
  }

  return vehicle->type;
}

Direction vehicle_get_direction(const Vehicle *vehicle) {
  if (vehicle == NULL) {
    return DIRECTION_NONE;
  }

  return vehicle->direction;
}
