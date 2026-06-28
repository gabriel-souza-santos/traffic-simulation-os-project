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
#include "../include/debug.h"


struct Vehicle {
    int id;             // ID do veículo
    int x;              // Posição atual no eixo X
    int y;              // Posição atual no eixo Y
    int direction;      // Direção atual
    VehicleType type;   // Tipo (AMBULANCE, CAR_FAST, CAR_MEDIUM, CAR_SLOW)
};


/**
 * @brief Verifica se um veículo está autorizado a mover-se no tick atual com
 * base na sua velocidade.
 *
 * @param vehicle Ponteiro constante para o veículo analisado.
 * @param current_tick O ciclo/tick atual do relógio global da simulação.
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
bool should_move_now(const Vehicle *vehicle, unsigned long current_tick) {
    if (vehicle == NULL)
        return false;

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
 * @param vehicle Ponteiro constante para o veículo que pretende mover-se.
 * @param target_x Coordenada X do destino alvo na matriz.
 * @param target_y Coordenada Y do destino alvo na matriz.
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
bool is_adjacent_cell(const Vehicle *vehicle, const int target_x, const int target_y) {
    if (vehicle == NULL)
        return false;

    // Garante o respeito aos limites da matriz global do mapa
    if (target_x < 0 || target_x >= MAP_WIDTH || target_y < 0 ||
        target_y >= MAP_HEIGHT) {
        return false;
    }

    const int current_x = vehicle->x;
    const int current_y = vehicle->y;

    const int diff_x = abs(target_x - current_x);
    const int diff_y = abs(target_y - current_y);

    // Movimento ortogonal direto: a soma das diferenças absolutas deve ser
    // exatamente 1
    return diff_x + diff_y == 1;
}


/**
 * @brief Verifica de forma segura e sincronizada se uma coordenada do mapa está
 * livre para tráfego.
 *
 * @param target_x Coordenada X a ser inspecionada.
 * @param target_y Coordenada Y a ser inspecionada.
 *
 * @return true
 * - Se a célula destino for transitável (diferente de TILE_BLOCKED) E não
 * estiver atualmente ocupada por outro veículo (`is_occupied == false`).
 * @return false
 * - Se as coordenadas alvo ultrapassarem os limites permitidos do mapa.
 * - Se a célula for um bloco intransitável (parede/obstáculo).
 * - Se a célula já contiver um veículo parado ou passando por ela.
 * * @note Esta função pode travar a execução do programa (via macro `TRY`) caso
 * ocorra uma falha crítica no sistema de threads ao tentar trancar ou
 * destrancar o Mutex (`lock`) da célula.
 */
bool is_cell_available(const int target_x, const int target_y) {
    if (target_x < 0 || target_x >= MAP_WIDTH || target_y < 0 ||
        target_y >= MAP_HEIGHT) {
        return false;
    }

    bool is_available = false;

    // Exclusão mútua individual por célula para leitura segura do estado
    TRY(pthread_mutex_lock(&map[target_y][target_x].lock));

    if (map[target_y][target_x].tile != TILE_BLOCKED &&
        !map[target_y][target_x].is_occupied) {
        is_available = true;
    }

    TRY(pthread_mutex_unlock(&map[target_y][target_x].lock));
    return is_available;
}


/**
 * @brief Projeta a trajetória frontal do veículo e checa se há um obstáculo
 * móvel imediatamente à frente.
 *
 * @param vehicle Ponteiro constante para o veículo cuja frente será analisada.
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
bool has_vehicle_ahead(const Vehicle *vehicle) {
    if (vehicle == NULL)
        return false;

    // Variaveis temporarias , para calcular  a posição da frente do veículo.
    int next_x = vehicle->x;
    int next_y = vehicle->y;

    // Projetar a coordenada no mapa baseada na orientação atual
    switch (vehicle->direction) {
        case DIRECTION_UP:
            next_y--;
            break; // Sobe uma linha na matriz
        case DIRECTION_DOWN:
            next_y++;
            break; // Desce uma linha na matriz
        case DIRECTION_LEFT:
            next_x--;
            break; // Recua uma coluna
        case DIRECTION_RIGHT:
            next_x++;
            break; // Avança uma coluna
        default:
            return false; // Se o carro estiver parado/sem direção, não há nada "à
            // frente"
    }
    // Aborta caso a projeção saia dos limites físicos do mapa da simulação
    if (next_x < 0 || next_x >= MAP_WIDTH || next_y < 0 || next_y >= MAP_HEIGHT) {
        return false;
    }

    bool is_ahead_occupied = false;

    TRY(pthread_mutex_lock(&map[next_y][next_x].lock));
    is_ahead_occupied = map[next_y][next_x].is_occupied;
    TRY(pthread_mutex_unlock(&map[next_y][next_x].lock));

    return is_ahead_occupied;
}


/**
 * @brief Determina se o movimento pretendido caracteriza uma ultrapassagem
 * lateral proibida em vias de sentido único.
 *
 * @param vehicle Ponteiro constante para o veículo analisado.
 * @param target_x Coordenada X de destino pretendida.
 * @param target_y Coordenada Y de destino pretendida.
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
bool is_overtaking(const Vehicle *vehicle, const int target_x, const int target_y) {
    if (vehicle == NULL)
        return false;

    const int current_x = vehicle->x;
    const int current_y = vehicle->y;

    const TileType current_tile = map[current_y][current_x].tile;

    // Interseções (.) e pontos de espera (!) permitem conversões/mudanças livres
    if (current_tile == TILE_ROAD || current_tile == TILE_WAIT) {
        return false;
    }

    // Em vias direcionais estritas (^, v, <, >), desvios laterais configuram
    // ultrapassagem proibida
    if (current_tile == TILE_ROAD_UP || current_tile == TILE_ROAD_DOWN) {
        if (target_x != current_x)
            return true; // Tentativa de desvio para a esquerda/direita em fluxo
        // vertical
    }

    if (current_tile == TILE_ROAD_LEFT || current_tile == TILE_ROAD_RIGHT) {
        if (target_y != current_y)
            return true; // Tentativa de desvio para cima/baixo em fluxo horizontal
    }

    return false;
}


/**
 * @brief Orquestra e executa o deslocamento de um veículo no mapa de
 * forma segura impedindo Deadlocks.
 *
 * @param vehicle Ponteiro para o objeto do veículo que executará a ação.
 * @param target_x Coordenada X da célula destino.
 * @param target_y Coordenada Y da célula destino.
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
bool update_position(Vehicle *vehicle, int target_x, int target_y,
                     const Clock *clock) {

    if (vehicle == NULL || clock == NULL)
        return false;

    // 1. Valida restrições temporais de velocidade
    const size_t current_tick = clock_get_tick(clock);
    if (!should_move_now(vehicle, current_tick)) {
        return false;
    }

    // 2. Valida restrições físicas de adjacência
    if (!is_adjacent_cell(vehicle, target_x, target_y)) {
        return false;
    }

    // 3. Valida regras de trânsito (ultrapassagem proibida)
    if (is_overtaking(vehicle, target_x, target_y)) {
        return false;
    }

    const int current_x = vehicle->x;
    const int current_y = vehicle->y;

    // 4. Prevenção Absoluta de Deadlocks: Ordenação Baseada no Endereço de
    // Memória dos Locks. Garante que se dois veículos adjacentes tentarem
    // disputar as mesmas células invertidas, eles sempre travarão os mutexes
    // exatamente na mesma ordem cronológica.
    if (&map[current_y][current_x].lock < &map[target_y][target_x].lock) {
        TRY(pthread_mutex_lock(&map[current_y][current_x].lock));
        TRY(pthread_mutex_lock(&map[target_y][target_x].lock));
    } else {
        TRY(pthread_mutex_lock(&map[target_y][target_x].lock));
        TRY(pthread_mutex_lock(&map[current_y][current_x].lock));
    }

    bool has_moved = false;

    // 5. Verificação em dupla checagem (Double-checked locking) dentro da seção
    // crítica
    if (map[target_y][target_x].tile != TILE_BLOCKED &&
        !map[target_y][target_x].is_occupied) {
        // Alteração atômica do estado da malha viária global
        map[current_y][current_x].is_occupied = false;
        map[target_y][target_x].is_occupied = true;

        // Sincronização dos metadados internos do veículo correspondente
        vehicle->x = target_x;
        vehicle->y = target_y;
        has_moved = true;
    }

    // 6. Liberação dos recursos na ordem inversa de aquisição
    TRY(pthread_mutex_unlock(&map[current_y][current_x].lock));
    TRY(pthread_mutex_unlock(&map[target_y][target_x].lock));

    return has_moved;
}


/*
 * ============================================================================
 * API Pública
 * ============================================================================
 */


Vehicle *vehicle_new(intptr_t id) {
    // Aloca e inicializa as propriedades de um veículo.
    // CRITÉRIO: Cada veículo possui identificador, posição, direção, velocidade, tipo, e rota.

    Vehicle *vehicle = malloc(sizeof(Vehicle)); // a memoria é alocada dinamicamente e corresponde ao struct Vehicle
    CHECK_NULL(vehicle);

    vehicle->id = (int)id; // dá um id para o veículo para que ele possa ser identificado de maneira única

    //ID 0 é sempre a ambulância.
    if (id == 0) {
        vehicle->type = AMBULANCE;
    }

    /*
     *A sequência de condicoes else if a seguir garante a presença de pelo menos um carro com
     * cada velocidade indicada no relógio global, pois se mantivéssemos aleatoriamente em todos os carros
     * poderia ocorrer de em sorteios especificos todos os carros terem a mesma velocidade.
     */
    else if (id == 1) {
        // ID 1 é garantido como Carro Rápido
        vehicle->type = CAR_FAST;
    }
    else if (id == 2) {
        // ID 2 é garantido como Carro Médio
        vehicle->type = CAR_MEDIUM;
    }
    else if (id == 3) {
        // ID 3 é garantido como Carro Lento
        vehicle->type = CAR_SLOW;
    }
    else {
        // Distribui os demais carros entre as velocidades
        vehicle->type = (VehicleType)((rand() % 3) + 2);
    }

    // Posições iniciais geradas em locais válidos de partida na pista
    vehicle->x = 2 + ((int)id * 2);
    vehicle->y = 2;
    vehicle->direction = ROAD_RIGHT; // Direção inicial padrão da pista de partida

    // Atualiza o estado na matriz global do mapa para ocupar o espaço inicial [cite: 23, 39]
    map[vehicle->y_position][vehicle->x_position].is_occupied = true;

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
