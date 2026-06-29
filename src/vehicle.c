/**
 * @file vehicle.c
 *
 * @brief Implementação do ciclo de vida e regras de deslocamento sincronizado dos veículos.
 *
 * Responsáveis: leticia-software-engineer e sudo-invers
 *
 * @date 2026-06-25
 */


#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "vehicle.h"
#include "clock.h"
#include "debug.h"
#include "map.h"


struct Vehicle {
    int id;             // ID do veículo
    Coord position;     // Posição atual
    Direction direction;// Direção atual
    VehicleType type;   // Tipo (AMBULANCE, CAR_FAST, CAR_MEDIUM, CAR_SLOW)
};

static Direction get_direction_from_tile(const TileType tile) {
    switch (tile) {
        case TILE_ROAD_UP:      return DIRECTION_UP;
        case TILE_ROAD_DOWN:    return DIRECTION_DOWN;
        case TILE_ROAD_LEFT:    return DIRECTION_LEFT;
        case TILE_ROAD_RIGHT:   return DIRECTION_RIGHT;
        default:                return DIRECTION_NONE;
    }
}


/**
 * @internal
 *
 * @brief Projetar a coordenada no mapa baseada na orientação atual
 *
 * Calcula a próxima posisão com base na direção e posição
 * atual do veículo.
 *
 * @warning Não faz verificações de limites do mapa.
 *
 * @param vehicle
 * @return
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
 * @brief Verifica se um veículo está autorizado a mover-se no tick atual com
 * base na sua velocidade.
 *
 * @param vehicle Ponteiro constante para o veículo analisado.
 * @param clock O relógio da simulação.
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
    if (vehicle == NULL)
        return false;

    const size_t current_tick = clock_get_tick(clock);

    // Vai verificar qual tick estamos.
    switch (vehicle->type) {
        case AMBULANCE:
        case CAR_FAST:
            return true;
        case CAR_MEDIUM:
            return current_tick % 2 == 0;
        case CAR_SLOW:
            return current_tick % 4 == 0;
        default:
            return false;
    }
}


/**
 * @brief Verifica se a célula destino é um vizinho ortogonal direto (adjacente
 * não diagonal).
 *
 * @param map
 * @param vehicle Ponteiro constante para o veículo que pretende mover-se.
 * @param target
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
 * @brief Projeta a trajetória frontal do veículo e checa se há um obstáculo
 * móvel imediatamente à frente.
 *
 * @param vehicle Ponteiro constante para o veículo cuja frente será analisada.
 * @param map
 *
 * @return true
 * - Se a célula imediatamente à frente (com base na direção atual do veículo)
 * estiver ocupada por outro veículo (`is_occupied == true`).
 * @return false
 * - Se o ponteiro do veículo for inválido (`NULL`).
 * - Se o veículo estiver sem direção mapeada ou parado (`default` no switch).
 * - Se a célula à frente estiver fora das dimensões limítrofes do mapa.
 * - Se a célula à frente estiver totalmente desocupada.
 * * @note Assim como `is_cell_available`, esta função faz uso seguro de exclusão
 * mútua (`lock`) e pode abortar o programa via macro `TRY` em caso de falha de
 * concorrência com pthreads.
 */
static bool has_vehicle_ahead(Map *map, const Vehicle *vehicle) {
    if (!vehicle || !map) return false;

    // Variaveis temporarias, para calcular a posição da frente do veículo.
    Coord next_position = vehicle->position;

    // Projetar a coordenada no mapa baseada na orientação atual
    switch (vehicle->direction) {
        case DIRECTION_UP:
            next_position.y--;
            break;          // Sobe uma linha na matriz
        case DIRECTION_DOWN:
            next_position.y++;
            break;          // Desce uma linha na matriz
        case DIRECTION_LEFT:
            next_position.x--;
            break;          // Recua uma coluna
        case DIRECTION_RIGHT:
            next_position.x++;
            break;          // Avança uma coluna
        default:
            return false;   // Se o carro estiver parado/sem direção, não há nada "à
                            // frente"
    }

    // Aborta caso a projeção saia dos limites físicos do mapa da simulação
    if (map_is_within_bounds(map, next_position) == false) {
        return false;
    }

    return map_is_occupied(map, next_position);
}


/**
 * @brief Determina se o movimento pretendido caracteriza uma ultrapassagem
 * lateral proibida em vias de sentido único.
 *
 * @param map
 * @param vehicle Ponteiro constante para o veículo analisado.
 * @param target
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


/**
 * @brief Orquestra e executa o deslocamento de um veículo no mapa de
 * forma segura impedindo Deadlocks.
 *
 * @param map
 * @param vehicle Ponteiro para o objeto do veículo que executará a ação.
 * @param target
 * @param clock Ponteiro constante para a estrutura do relógio global.
 *
 * @return true
 * - Se todas as validações (velocidade, adjacência, regras de trânsito)
 * passarem com sucesso, os locks forem adquiridos e a célula estiver livre. O
 * mapa global e a posição do veículo são alterados de forma atómica.
 * @return false
 * - Se o veículo não puder mover-se neste ciclo específico (`should_move_now`
 * falhar).
 * - Se os ponteiros `vehicle` ou `clock` forem nulos.
 * - Se o destino não for adjacente (`is_adjacent_cell` falhar).
 * - Se o movimento violar regras de ultrapassagem (`is_overtaking` retornar
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
static bool update_position(Map *map, Vehicle *vehicle,
    const Coord target, const Clock *clock) {

    if (vehicle == NULL || clock == NULL)
        return false;

    // 1. Valida restrições temporais de velocidade
    if (!should_move_now(vehicle, clock)) {
        return false;
    }

    // 2. Valida restrições físicas de adjacência
    if (!is_adjacent(map, vehicle, target)) {
        return false;
    }

    // 3. Valida regras de trânsito (ultrapassagem proibida)
    if (is_overtaking(map, vehicle, target)) {
        return false;
    }

    const Coord current = vehicle->position;

    if (map_transfer_occupant(map, current, target) == false) {
        return false;
    }

    vehicle->position = target;
    return true;
}


/*
 * ============================================================================
 * API Pública
 * ============================================================================
 */


Vehicle *vehicle_new(Map *map, const int id) {
    // Aloca e inicializa as propriedades de um veículo.
    // CRITÉRIO: Cada veículo possui identificador, posição, direção, velocidade, tipo, e rota.

    Vehicle *vehicle = malloc(sizeof(Vehicle));
    CHECK_NULL(vehicle);

    vehicle->id = id;

    /*
     * A sequência de condicoes else if a seguir garante a presença de pelo menos um carro com
     * cada velocidade indicada no relógio global, pois se mantivéssemos aleatoriamente em todos os carros
     * poderiam ocorrer sorteios especificos todos os carros terem a mesma velocidade.
     */

    // TODO: Melhorar o fator aleatório

    if (id == 0) {
        vehicle->type = AMBULANCE;
    } else if (id == 1) {
        vehicle->type = CAR_FAST;
    } else if (id == 2) {
        vehicle->type = CAR_MEDIUM;
    } else if (id == 3) {
        vehicle->type = CAR_SLOW;
    } else {
        vehicle->type = (VehicleType)(rand() % 3 + 2);
    }

    vehicle->position = map_reserve_spawn_point(map);

    if (vehicle->position.x == NULL_COORD.x ||
        vehicle->position.y == NULL_COORD.y) {
        free(vehicle);
        return NULL;
    }

    const TileType tile = map_get_tile_type(map, vehicle->position);
    vehicle->direction = get_direction_from_tile(tile);

    if (vehicle->direction == DIRECTION_NONE) {
        free(vehicle);
        return NULL;
    }

    return vehicle;
}


void vehicle_destroy(Vehicle *vehicle) {
    // Libera a memória alocada para o contexto do veículo.
    CHECK_NULL(vehicle);
    free(vehicle);
}


void *vehicle_update(void *vehicle) {
    // Rotina principal executada por cada thread de veículo.
    // CRITÉRIOS: Respeitar direção da via, não atravessar paredes (BLOCKED) e não sair do mapa.

    // TODO

    return NULL;
}
