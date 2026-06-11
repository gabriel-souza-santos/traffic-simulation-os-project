/**
 * @file map.h
 * @brief
 *
 * @date 2026-06-10
 */
#ifndef URBAN_TRAFFIC_MAP_H
#define URBAN_TRAFFIC_MAP_H

#include <stdbool.h>
#include <pthread.h>


/** @brief Largura da malha viária (número de colunas da matriz do mapa). */
#define MAP_WIDTH 64 //valores permitidos: [0-255]

/** @brief Altura da malha viária (número de linhas da matriz do mapa). */
#define MAP_HEIGHT 64 //valores permitidos: [0-255]


/**
 * @brief Define a natureza física e a direção das células do mapa.
 *
 * Utilizado para guiar a lógica de movimentação dos veículos.
 * O código deve verificar esta informação antes de atualizar uma célula.
 */
typedef enum {
    BLOCKED,    /**< Célula intransitável (Veículos não entram). */
    ROAD_UP,    /**< Via com sentido norte (para cima na matriz). */
    ROAD_DOWN,  /**< Via com sentido sul (para baixo na matriz). */
    ROAD_LEFT,  /**< Via com sentido oeste (esquerda na matriz). */
    ROAD_RIGHT, /**< Via com sentido leste (direita na matriz). */
    //... intersections
} TileType;


/**
 * @brief Representa uma única unidade (quadrado) de espaço dentro do mapa.
 *
 * É a estrutura central para a resolução de concorrência. Une as propriedades
 * geográficas da via com o mecanismo que garante a lei de impenetrabilidade
 * dos veículos.
 */
typedef struct {
    pthread_mutex_t lock; /**< Uma thread DEVE obter este lock antes de ler/alterar 'is_occupied'. */
    TileType tile;        /**< A direção do fluxo ou tipo de terreno desta coordenada. */
    bool is_occupied;     /**< Estado de ocupação: true se há um carro parado aqui, false se está livre. */
} MapCell;


/**
 * @brief Matriz bidimensional global representando a cidade.
 * * Todas as threads de veículos interagem com este mapa simultaneamente.
 * É necessário usar os locks de cada célula para fazer a travessia de forma
 * segura e evitar Race Conditions.
 */
MapCell map[MAP_HEIGHT][MAP_WIDTH];

#endif //URBAN_TRAFFIC_MAP_H