/**
 * @file vehicle.c
 * @brief Implementação da criação, ciclo de vida e movimentação das threads dos veículos.
 *
 * Responsável: @leticia-software-engineer
 * @date 2026-06-25
 */

//inclusão das bibliotecas
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "vehicle.h"
#include "map.h"
#include "clock.h"

//Struct para veículo com atributos necessários
struct Vehicle {
    int id;
    int direction;    // Direção atual
    int x_position;   // Posição atual X
    int y_position;   // Posição atual Y
    VehicleType type; // Tipo (AMBULANCE, CAR_FAST, CAR_MEDIUM, CAR_SLOW)
    size_t last_tick; // Controle interno de rota/ticks de velocidade
};

// Referência ao relógio global externo que gerencia os ciclos
extern Clock *global_clock;

/**
 * @brief Aloca e inicializa as propriedades de um veículo.
 * CRITÉRIO: Cada veículo possui identificador, posição, direção, velocidade, tipo, e rota.
 */
Vehicle *vehicle_new(intptr_t id) {
    Vehicle *vehicle = malloc(sizeof(Vehicle)); // a memoria é alocada dinamicamente e corresponde ao struct Vehicle
    if (vehicle == NULL) {
        return NULL; //se a alocação falhar a função retorna nulo, evitando quebrar a lógica do programa
    }

    vehicle->id = (int)id; // dá um id para o veículo para que ele possa ser identificado de maneira única
    vehicle->last_tick = 0; // o veículo entra na simulação com velocidade 0 até ela ser definida aleatoriamente seguindo o relógio global

    //ID 0 é sempre a ambulância.
    if (id == 0) {
        vehicle->type = AMBULANCE;
    }
    /*A sequência de condicoes else if a seguir garante a presença de pelo menos um carro com
     * cada velocidade indicada no relógio global, pois se mantessemos aleatoriamente em todos os carros
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
    vehicle->x_position = 2 + ((int)id * 2);
    vehicle->y_position = 2;
    vehicle->direction = ROAD_RIGHT; // Direção inicial padrão da pista de partida

    // Atualiza o estado na matriz global do mapa para ocupar o espaço inicial [cite: 23, 39]
    map[vehicle->y_position][vehicle->x_position].is_occupied = true;

    return vehicle;
}

/**
 * @brief Libera a memória alocada para o contexto do veículo.
 */
void vehicle_destroy(void *vehicle) {
    if (vehicle != NULL) {
        free(vehicle);
    }
}

/**
 * @brief Rotina principal executada por cada thread de veículo.
 * CRITÉRIOS: Respeitar direção da via, não atravessar paredes (BLOCKED) e não sair do mapa.
 */
void *vehicle_update(void *vehicle) {


    return NULL;
}