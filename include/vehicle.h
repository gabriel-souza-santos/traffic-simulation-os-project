/**
 * @file vehicle.h
 * @brief
 *
 * @date 2026-06-10
 */
#ifndef URBAN_TRAFFIC_VEHICLE_H
#define URBAN_TRAFFIC_VEHICLE_H


/** @brief Quantidade de veículos rodando simultaneamente na simulação. */
#define VEHICLE_COUNT 10 //valores permitidos: [10-20]


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
 * @brief Armazena o contexto completo da thread de um veículo.
 *
 * Um ponteiro para esta estrutura deve ser passado como argumento na chamada
 * de pthread_create(), dando à thread identidade, comportamento e localização atual.
 */
typedef struct {
    VehicleType type; /**< Define as regras de movimento e prioridade. */
    int id;           /**< Identificador único do carro (debug). */
    int x;            /**< Posição atual do veículo no eixo X (coluna). */
    int y;            /**< Posição atual do veículo no eixo Y (linha). */
} Vehicle;


/**
 * @brief Array global que aloca a memória do contexto de todos os veículos.
 *
 * Pode ser iterado pela thread de renderização (ASCII) caso ela precise
 * de informações adicionais que não estão declaradas no mapa.
 */
Vehicle cars[VEHICLE_COUNT];

#endif //URBAN_TRAFFIC_VEHICLE_H