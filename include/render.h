/**
 * @file render.h
 * @brief Interface do sistema de renderização ASCII da simulação.
 *
 * Este módulo é responsável por desenhar o estado atual da simulação no
 * terminal, representando o mapa, os veículos e os semáforos como blocos
 * de texto (assets) organizados numa grade de células de dimensão fixa.
 *
 * A renderização utiliza códigos de escape ANSI internamente para
 * posicionamento de cursor e formatação. Opcionalmente, pode empregar
 * double buffering para evitar flickering na atualização da tela.
 *
 * @date 2026-06-11
 */
#ifndef URBAN_TRAFFIC_RENDER_H
#define URBAN_TRAFFIC_RENDER_H

#include <stddef.h>

#include "analyser.h"
#include "clock.h"
#include "map.h"
#include "vehicle.h"
#include "traffic_light.h"


/** @name Estruturas de dados */
/** @{ */

/**
 * @brief Tipo opaco que representa o estado interno do renderizador.
 *
 * Deve ser sempre usado por meio de um ponteiro:
 * @code{.c}
 * Render *render;
 * @endcode
 */
typedef struct Render Render;

/**
 * @brief Argumentos passados para a thread do renderizador via @c pthread_create.
 */
typedef struct {
    Analyser *analyser;
    Render *render;         /**< Instância do renderizador. */
    Map *map;               /**< Mapa da simulação a ser renderizado. */
    Clock *clock;           /**< Relógio global, usado para sincronizar a renderização. */
    Vehicle **vehicles;     /**< Array de ponteiros para os veículos ativos. */
    size_t vehicle_count;   /**< Número de veículos no array. */
    TrafficLight *traffic_light;
} RenderArgs;

/** @} */


/**
 * @brief Cria e inicializa uma nova instância do renderizador.
 *
 * As dimensões de cada célula definem o tamanho esperado dos assets
 * carregados via @c render_load_tile_asset e
 * @c render_load_vehicle_asset. Assets com dimensões divergentes
 * podem produzir saída malformada no terminal.
 *
 * @param map         Mapa da simulação.
 * @param tile_width  Largura de cada célula em caracteres.
 * @param tile_height Altura de cada célula em linhas.
 * @return Ponteiro para a nova instância, ou NULL em caso de falha.
 */
Render *render_new(const Map *map, size_t tile_width, size_t tile_height);

/**
 * @brief Destrói a instância do renderizador e libera todos os recursos
 *        associados, incluindo os assets carregados.
 *
 * @param render Ponteiro para o renderizador a ser destruído.
 */
void render_destroy(Render *render);

/**
 * @brief Rotina principal da thread do renderizador.
 *
 * A cada tick do relógio, lê o estado atual do mapa e dos veículos
 * fornecidos em @c RenderArgs e atualiza a exibição no terminal,
 * compondo cada célula com o asset correspondente ao seu tipo e estado.
 *
 * @note A thread bloqueia sem busy-waiting entre ticks, sincronizada
 *       via @c clock_signal.
 * @note Cells sem asset carregado são renderizadas como blocos de espaços
 *       em branco com as dimensões configuradas em @c render_new.
 *
 * @param render_args Ponteiro para @c RenderArgs, passado como @c void* pela
 *             API Pthreads.
 * @return NULL, respeitando a assinatura padrão exigida pela API Pthreads.
 */
void *render_update(void *render_args);

/**
 * @brief Carrega um asset de célula do mapa a partir de um arquivo de texto.
 *
 * O arquivo deve conter um bloco de texto com exatamente as dimensões
 * configuradas em @c render_new (@c tile_width × @c tile_height caracteres
 * por linha). O asset é associado ao @p type informado e será utilizado
 * sempre que uma célula desse tipo for renderizada.
 *
 * Caso nenhum asset seja carregado para um determinado @c TileType, aquela
 * célula será renderizada como um bloco de espaços em branco.
 *
 * @param render    Ponteiro para a instância do renderizador.
 * @param type      Tipo de célula do mapa ao qual o asset será associado.
 * @param file_name Caminho do arquivo de texto contendo o asset.
 */
void render_load_tile_asset(Render *render,
    TileType type, const char *file_name);

/**
 * @brief Carrega um asset de veículo a partir de um arquivo de texto.
 *
 * O arquivo deve conter um bloco de texto com exatamente as dimensões
 * configuradas em @c render_new. O asset é associado à combinação
 * (@p type, @p direction), permitindo que um mesmo tipo de veículo tenha
 * representações visuais distintas por direção de movimento.
 *
 * Caso nenhum asset seja carregado para uma combinação específica, o
 * veículo será renderizado como um bloco de espaços em branco.
 *
 * @param render    Ponteiro para a instância do renderizador.
 * @param type      Tipo do veículo ao qual o asset será associado.
 * @param direction Direção de movimento associada a este asset.
 * @param file_name Caminho do arquivo de texto contendo o asset.
 */
void render_load_vehicle_asset(Render *render,
    VehicleType type, Direction direction, const char *file_name);


/**
 * @brief Carrega um asset de célula e o associa a múltiplos tipos de tile.
 *
 * Lê o arquivo uma única vez e associa o mesmo bloco de texto a todos os
 * @c TileType informados em @p types, evitando releituras redundantes do
 * arquivo para tiles que compartilham a mesma representação visual.
 *
 * O arquivo deve conter um bloco de texto com exatamente as dimensões
 * configuradas em @c render_new. Tipos sem asset carregado são
 * renderizados como blocos de espaços em branco.
 *
 * @param render    Ponteiro para a instância do renderizador.
 * @param file_name Caminho do arquivo de texto contendo o asset.
 * @param types     Array de tipos de tile que receberão este asset.
 * @param count     Número de elementos em @p types.
 */
void render_load_tile_asset_multi(Render *render,
    const char *file_name, const TileType *types, int count);

/**
 * @brief Carrega um asset de veículo e o associa a todas as direções.
 *
 * Lê o arquivo uma única vez e registra o mesmo bloco de texto para todas
 * as combinações (@p type, Direction), útil para veículos cuja
 * representação visual não varia com a direção de movimento.
 *
 * Direções individualmente sobrescritas após esta chamada (via
 * @c render_load_vehicle_asset) têm precedência sobre o asset genérico
 * aqui registrado.
 *
 * @param render    Ponteiro para a instância do renderizador.
 * @param type      Tipo do veículo ao qual o asset será associado.
 * @param file_name Caminho do arquivo de texto contendo o asset.
 */
void render_load_vehicle_asset_all_directions(Render *render,
    VehicleType type, const char *file_name);

/**
 * @brief Carrega o asset visual associado a uma cor de semáforo.
 *
 * @param render Ponteiro para o renderizador.
 * @param color Cor do semáforo (RED, GREEN ou YELLOW).
 * @param file_name Caminho do arquivo de asset a ser carregado.
 */
void render_load_traffic_light_asset(Render *render, TrafficLightColor color, const char *file_name);


/**
 * @brief Carrega a sprite de um tile específico diretamente a partir de uma string.
 *
 * @param render Ponteiro para o renderizador.
 * @param type Tipo do tile que receberá a sprite.
 * @param sprite_data String contendo o desenho ASCII.
 */
void render_load_tile_asset_from_string(Render *render, TileType type, const char *sprite_data);

/**
 * @brief Carrega a mesma sprite para múltiplos tipos de tiles simultaneamente a partir de uma string.
 *
 * @param render Ponteiro para o renderizador.
 * @param sprite_data String contendo o desenho ASCII.
 * @param types Array contendo os tipos de tiles que receberão a sprite.
 * @param count Quantidade de elementos no array 'types'.
 */
void render_load_tile_asset_multi_from_string(Render *render, const char *sprite_data, const TileType *types, int count);

/**
 * @brief Carrega a sprite de um veículo para uma direção específica a partir de uma string.
 *
 * @param render Ponteiro para o renderizador.
 * @param type Tipo do veículo.
 * @param direction Direção correspondente à sprite.
 * @param sprite_data String contendo o desenho ASCII.
 */
void render_load_vehicle_asset_from_string(Render *render, VehicleType type, Direction direction, const char *sprite_data);

/**
 * @brief Carrega a mesma sprite para todas as direções de um tipo de veículo a partir de uma string.
 *
 * @param render Ponteiro para o renderizador.
 * @param type Tipo do veículo.
 * @param sprite_data String contendo o desenho ASCII.
 */
void render_load_vehicle_asset_all_directions_from_string(Render *render, VehicleType type, const char *sprite_data);

/**
 * @brief Carrega a sprite de um estado (cor) do semáforo a partir de uma string.
 *
 * @param render Ponteiro para o renderizador.
 * @param color Cor do semáforo que receberá a sprite.
 * @param sprite_data String contendo o desenho ASCII.
 */
void render_load_traffic_light_asset_from_string(Render *render, TrafficLightColor color, const char *sprite_data);

#endif //URBAN_TRAFFIC_RENDER_H