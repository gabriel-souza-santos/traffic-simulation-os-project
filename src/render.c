/**
 * @file render.c
 *
 * @date 2026-04-07
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "render.h"
#include "clock.h"
#include "vehicle.h"
#include "map.h"
#include "debug.h"

/**
 * @internal
 * @brief Estado interno do renderizador ASCII simplificado.
 */
struct Render {
    size_t tile_width;  /**< Largura de cada célula em caracteres. */
    size_t tile_height; /**< Altura de cada célula em linhas. */

    uint8_t *vehicle_assets[VEHICLE_TYPE_COUNT - 1][DIRECTION_COUNT - 1];
    uint8_t *tile_assets[TILE_TYPE_COUNT];

    char *buffer; /**< Buffer linear string (com terminação '\0') para renderização estável */

    size_t buffer_width;  /**< Largura total da tela em caracteres (tile_width * map_width). */
    size_t buffer_height; /**< Altura total da tela em linhas (tile_height * map_height). */
};

/* ============================================================================
 * Funções auxiliares internas
 * ============================================================================ */

/**
 * @internal
 * @brief Mapeia um TileType para um índice consecutivo no array tile_assets.
 */
static int tile_type_to_index(const TileType type) {
    switch (type) {
        case TILE_BLOCKED:    return 0;
        case TILE_ROAD_UP:    return 1;
        case TILE_ROAD_DOWN:  return 2;
        case TILE_ROAD_LEFT:  return 3;
        case TILE_ROAD_RIGHT: return 4;
        case TILE_ROAD:       return 5;
        case TILE_WAIT:       return 6;
        default:              return -1;
    }
}

/**
 * @internal
 * @brief Lê um arquivo de asset e retorna um buffer contendo apenas os
 * caracteres visuais (sem quebras de linha).
 */
static uint8_t *load_asset_from_file(const char *file_name, const size_t asset_size) {
    if (!file_name || asset_size == 0) return NULL;

    FILE *file = fopen(file_name, "r");
    if (!file) return NULL;

    uint8_t *buffer = malloc(asset_size);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    memset(buffer, ' ', asset_size);

    size_t written = 0;
    int symbol;

    while ((symbol = fgetc(file)) != EOF && written < asset_size) {
        if (symbol == '\n' || symbol == '\r') continue;
        buffer[written++] = (uint8_t)symbol;
    }

    fclose(file);
    return buffer;
}

/**
 * @internal
 * @brief Limpa o buffer de caracteres preenchendo-o com espaços e aplicando as quebras de linha corretas.
 */
static void render_clear_internal_buffer(Render *render) {
    size_t out = 0;
    for (size_t row = 0; row < render->buffer_height; row++) {
        memset(render->buffer + out, ' ', render->buffer_width);
        out += render->buffer_width;
        render->buffer[out++] = '\n';
    }
    render->buffer[out] = '\0'; /* Finaliza a string de forma segura */
}

/**
 * @internal
 * @brief Escreve um asset na matriz lógica do buffer de caracteres.
 */
static void render_write_tile(Render *render, const size_t x, const size_t y, const uint8_t *asset) {
    const size_t row_stride = render->buffer_width + 1; /* +1 para o '\n' */

    for (size_t row = 0; row < render->tile_height; row++) {
        const size_t buf_offset = ((y * render->tile_height + row) * row_stride) + (x * render->tile_width);
        const size_t asset_offset = row * render->tile_width;

        if (asset) {
            memcpy(render->buffer + buf_offset, asset + asset_offset, render->tile_width);
        } else {
            memset(render->buffer + buf_offset, ' ', render->tile_width);
        }
    }
}

/**
 * @internal
 * @brief Retorna o asset de um veículo para uma combinação de tipo e direção.
 */
static const uint8_t *render_get_vehicle_asset(Render *render, const VehicleType type, const Direction direction) {
    if (type <= NO_VEHICLE || type >= VEHICLE_TYPE_COUNT) return NULL;
    if (direction <= DIRECTION_NONE || direction >= DIRECTION_COUNT) return NULL;

    const int t = (int)type - 1;
    const int d = (int)direction - 1;

    return render->vehicle_assets[t][d];
}

/* ============================================================================
 * API Pública
 * ============================================================================ */

Render *render_new(const Map *map, const size_t tile_width, const size_t tile_height) {
    if (!tile_width || !tile_height) return NULL;

    Render *render = malloc(sizeof(Render));
    if (!render) return NULL;

    render->tile_width  = tile_width;
    render->tile_height = tile_height;
    render->buffer_width  = tile_width  * map_get_width(map);
    render->buffer_height = tile_height * map_get_height(map);

    /* Tamanho exato da string: (largura + '\n') * altura + '\0' de terminação */
    const size_t buffer_size = (render->buffer_width + 1) * render->buffer_height + 1;

    render->buffer = malloc(buffer_size);
    if (!render->buffer) {
        free(render);
        return NULL;
    }

    memset(render->tile_assets,    0, sizeof(render->tile_assets));
    memset(render->vehicle_assets, 0, sizeof(render->vehicle_assets));

    return render;
}

void render_destroy(Render *render) {
    if (!render) return;

    free(render->buffer);

    for (int i = 0; i < TILE_TYPE_COUNT; i++) {
        free(render->tile_assets[i]);
    }

    for (int i = 0; i < VEHICLE_TYPE_COUNT - 1; i++) {
        for (int j = 0; j < DIRECTION_COUNT - 1; j++) {
            free(render->vehicle_assets[i][j]);
        }
    }

    free(render);
}

void render_load_tile_asset(Render *render, const TileType type, const char *file_name) {
    if (!render || !file_name) return;

    const int mapped_index = tile_type_to_index(type);
    if (mapped_index < 0) return;

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *asset = load_asset_from_file(file_name, asset_size);
    if (!asset) return;

    free(render->tile_assets[mapped_index]);
    render->tile_assets[mapped_index] = asset;
}

void render_load_tile_asset_multi(Render *render, const char *file_name, const TileType *types, const int count) {
    if (!render || !file_name || !types || count <= 0) return;

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *source = load_asset_from_file(file_name, asset_size);
    if (!source) return;

    for (int i = 0; i < count; i++) {
        const int index = tile_type_to_index(types[i]);
        if (index < 0) continue;

        uint8_t *copy = malloc(asset_size);
        if (!copy) continue;

        memcpy(copy, source, asset_size);
        free(render->tile_assets[index]);
        render->tile_assets[index] = copy;
    }

    free(source);
}

void render_load_vehicle_asset(Render *render, const VehicleType type, const Direction direction, const char *file_name) {
    if (!render || !file_name) return;
    if (type <= NO_VEHICLE || type >= VEHICLE_TYPE_COUNT) return;
    if (direction <= DIRECTION_NONE || direction >= DIRECTION_COUNT) return;

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *asset = load_asset_from_file(file_name, asset_size);
    if (!asset) return;

    const int t = (int)type - 1;
    const int d = (int)direction - 1;

    free(render->vehicle_assets[t][d]);
    render->vehicle_assets[t][d] = asset;
}

void render_load_vehicle_asset_all_directions(Render *render, const VehicleType type, const char *file_name) {
    if (!render || !file_name) return;
    if (type <= NO_VEHICLE || type >= VEHICLE_TYPE_COUNT) return;

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *source = load_asset_from_file(file_name, asset_size);
    if (!source) return;

    const int t = (int)type - 1;

    for (int d = 0; d < DIRECTION_COUNT - 1; d++) {
        uint8_t *copy = malloc(asset_size);
        if (!copy) continue;

        memcpy(copy, source, asset_size);
        free(render->vehicle_assets[t][d]);
        render->vehicle_assets[t][d] = copy;
    }

    free(source);
}

/**
 * @brief Loop principal da Thread do Renderizador (Modo Simples Garantido).
 */
void *render_update(void *render_args) {
    if (!render_args) return NULL;

    const RenderArgs *args = (RenderArgs *)render_args;
    Clock     *clock    = args->clock;
    Map       *map      = args->map;
    Render    *render   = args->render;
    Vehicle  **vehicles = args->vehicles;

    const size_t map_width  = map_get_width(map);
    const size_t map_height = map_get_height(map);

    for (int t = 0; t < TICKS; t++) {
        const size_t current_tick = clock_get_tick(clock);

        /* 1. Reseta a estrutura do buffer preenchendo-o com espaços e quebras de linha */
        render_clear_internal_buffer(render);

        /* 2. Redesenho Total: Reconstrói os tiles do mapa de fundo no buffer */
        for (size_t y = 0; y < map_height; y++) {
            for (size_t x = 0; x < map_width; x++) {
                const Coord pos    = {(int)x, (int)y};
                const TileType type = map_get_tile_type(map, pos);
                const int index    = tile_type_to_index(type);

                const uint8_t *asset = (index >= 0 && render->tile_assets[index])
                    ? render->tile_assets[index]
                    : NULL;

                render_write_tile(render, x, y, asset);
            }
        }

        /* 3. Sobreposição: Insere TODOS os veículos ativos nas posições atuais */
        for (int i = 0; i < VEHICLE_COUNT; i++) {
            const Coord pos        = vehicle_get_position(vehicles[i]);
            const VehicleType type = vehicle_get_type(vehicles[i]);
            const Direction dir    = vehicle_get_direction(vehicles[i]);

            const uint8_t *asset = render_get_vehicle_asset(render, type, dir);

            /* Validação de salvaguarda de limites do mapa antes de injetar */
            if (pos.x >= 0 && (size_t)pos.x < map_width && pos.y >= 0 && (size_t)pos.y < map_height) {
                render_write_tile(render, (size_t)pos.x, (size_t)pos.y, asset);
            }
        }

        /* 4. Flush Determinístico: Limpa o terminal, descarrega a string e sincroniza */
        if (system("clear") == 0) {
            printf("%s", render->buffer);
            fflush(stdout);
        }

        clock_signal(clock, current_tick);
    }

    return NULL;
}