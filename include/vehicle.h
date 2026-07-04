/**
 * @file vehicle.h
 * @brief Definições e rotinas das threads dos veículos da simulação.
 *
 * Este módulo gerencia o ciclo de vida, estado e comportamento das entidades
 * móveis (carros e ambulâncias) que navegam pela malha viária, garantindo
 * o respeito à exclusão mútua e ao relógio global discreto.
 *
 * @date 2026-06-10
 */
#ifndef URBAN_TRAFFIC_VEHICLE_H
#define URBAN_TRAFFIC_VEHICLE_H

#include <stdint.h>
#include "map.h"

/** @brief Quantidade de veículos rodando simultaneamente na simulação. */
#define VEHICLE_COUNT 10 //valores permitidos: [10-20]

/* Garante em tempo de compilação que a contagem de veículos respeite os limites do projeto */
#if VEHICLE_COUNT < 10 || VEHICLE_COUNT > 20
#undef VEHICLE_COUNT
#define VEHICLE_COUNT 10
#endif //#if VEHICLE_COUNT < 10 || VEHICLE_COUNT > 20

/**
 * @brief Classifica o papel e a velocidade de cada veículo na simulação.
 *
 * O tipo do veículo afeta a frequência com que a sua thread se movimenta
 * em relação aos ticks do relógio global.
 */
typedef enum {
    NO_VEHICLE, /**< Valor auxiliar indicando ausência de veículo associado. */
    AMBULANCE,  /**< Veículo de emergência. Deve ter prioridade em cruzamentos. */
    CAR_FAST,   /**< Carro rápido. Executa o seu movimento a cada 1 tick do relógio. */
    CAR_MEDIUM, /**< Carro médio. Executa o seu movimento a cada 2 ticks do relógio. */
    CAR_SLOW,   /**< Carro lento. Executa o seu movimento a cada 4 ticks do relógio. */
} VehicleType;

#define VEHICLE_TYPE_COUNT 5

/**
 * @brief Indica o sentido atual de movimento do veículo no mapa.
 * * Usado para calcular a próxima coordenada (X, Y) que o veículo tentará
 * acessar e travar (lock) durante o seu turno.
 */
typedef enum {
    DIRECTION_NONE,  /**< Veículo parado ou sem direção definida. */
    DIRECTION_UP,    /**< Movimento para o Norte (Y decrescente na matriz). */
    DIRECTION_DOWN,  /**< Movimento para o Sul (Y crescente na matriz). */
    DIRECTION_LEFT,  /**< Movimento para o Oeste (X decrescente na matriz). */
    DIRECTION_RIGHT, /**< Movimento para o Leste (X crescente na matriz). */
} Direction;

#define DIRECTION_COUNT 5

/**
 * @brief Tipo opaco que representa um veículo e o seu contexto interno.
 * Oculta os detalhes de implementação das outras partes do código para
 * garantir o encapsulamento seguro.
 *
 * Deve ser instanciado sempre por meio de um ponteiro:
 * @code
 * Vehicle *vehicle;
 * @endcode
 */
typedef struct Vehicle Vehicle;

/**
 * @brief Aloca e inicializa um novo veículo.
 *
 * @param map Mapa onde o veículo será inserido; usado para reservar um
 *            ponto de spawn livre.
 * @param id Identificador único do veículo. Os quatro primeiros ids (0-3)
 *           recebem tipos fixos (garantindo ao menos uma ambulância e um
 *           carro de cada velocidade); os demais recebem tipo aleatório.
 * @return Um ponteiro para a nova instância de Vehicle, ou `NULL` se não
 *         houver spawn point livre no mapa ou se a direção inicial
 *         derivada do tile reservado for inválida.
 */
Vehicle *vehicle_new(Map *map, int id);

/**
 * @brief Destrói um veículo e libera seus recursos.
 * @param vehicle Ponteiro para o veículo a ser destruído.
 */
void vehicle_destroy(Vehicle *vehicle);

/**
 * @brief Rotina principal executada pela thread de cada veículo.
 *
 * @param vehicle Ponteiro genérico (void*) que deve ser feito o cast para (Vehicle*).
 * @return NULL, respeitando a assinatura padrão da API Pthreads.
 */
void *vehicle_update(void *vehicle);

/**
 * @brief Retorna a coordenada de prioridade de passagem ativa na simulação.
 *
 * Consulta o estado atual da ambulância e deriva a coordenada que indica
 * qual eixo da malha deve receber prioridade de sinal verde. O valor
 * retornado pode assumir três formas:
 *
 * - **Ambos os campos inválidos** (NULL_COORD): nenhuma prioridade ativa;
 *   os semáforos devem seguir a alternância normal.
 * - **Apenas @c x válido**: todos os pontos de espera com o mesmo @c x
 *   devem receber verde; os demais, vermelho.
 * - **Apenas @c y válido**: todos os pontos de espera com o mesmo @c y
 *   devem receber verde; os demais, vermelho.
 *
 * @note O campo inválido de uma coordenada parcial corresponde ao valor
 *       sentinela de NULL_COORD (ver map.h).
 *
 * @return A coordenada de prioridade ativa, ou NULL_COORD se não houver
 *         ambulância em trânsito ou se ela não estiver em posição de
 *         solicitar prioridade.
 */
Coord vehicle_get_priority_coord(void);

// TODO: Funções Getter para obter informações dos veículos de forma segura

Coord vehicle_get_position(const Vehicle *vehicle);

VehicleType vehicle_get_type(const Vehicle *vehicle);

Direction vehicle_get_direction(const Vehicle *vehicle);

#endif //URBAN_TRAFFIC_VEHICLE_H