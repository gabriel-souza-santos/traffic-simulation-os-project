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

/**
 * @brief Tipo opaco que representa um veículo e o seu contexto interno.
 * Oculta os detalhes de implementação das outras partes do código para
 * garantir o encapsulamento seguro.
 */
typedef struct Vehicle Vehicle;

/**
 * @brief Aloca e inicializa um novo veículo.
 * @param id Identificador único numérico (cast para intptr_t para facilitar uso em threads).
 * @return Um ponteiro para a nova instância de Vehicle, ou aborta via CHECK_NULL em caso de falha.
 */
Vehicle *vehicle_new(intptr_t id);

/**
 * @brief Destrói um veículo e libera seus recursos.
 * @param vehicle Ponteiro genérico (void*) para o veículo a ser destruído.
 */
void vehicle_destroy(Vehicle *vehicle);

/**
 * @brief Rotina principal executada pela thread de cada veículo.
 *
 * @details Esta função encapsula toda a lógica de física e concorrência do veículo.
 * O seu fluxo de execução em loop consiste em:
 * 1. Esperar o relógio (`clock_signal`) e os semáforos atualizarem.
 * 2. Verificar se o tick atual corresponde à sua velocidade (VehicleType).
 * 3. Calcular a próxima célula baseada em sua `Direction`.
 * 4. Verificar validade (se não é parede ou fim do mapa).
 * 5. Tentar adquirir o lock da próxima célula (`is_occupied`). Se falhar ou estiver ocupada, cancela o movimento.
 * 6. Se livre, move-se marcando o tile destino como ocupado e liberando o tile anterior.
 * 7. Atualiza sua direção interna caso esteja em um cruzamento ou curva.
 * 8. Chama `clock_signal` para sinalizar conclusão e dormir até o próximo tick.
 *
 * @param vehicle Ponteiro genérico (void*) que deve ser feito o cast para (Vehicle*).
 * @return NULL, respeitando a assinatura padrão da API Pthreads.
 */
void *vehicle_update(void *vehicle);

// TODO: Funções Getter para obter informações dos veículos de forma segura

#endif //URBAN_TRAFFIC_VEHICLE_H