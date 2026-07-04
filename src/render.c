/**
 * @file render.c
 *
 * @date 2026-04-07
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include "render.h"
#include "clock.h"
#include "vehicle.h"
#include "map.h"
#include "debug.h"


/**
 * @internal
 * @brief Estado interno do renderizador ASCII.
 *
 * Armazena os assets de tiles e veículos como blocos de bytes de tamanho
 * fixo (@c tile_width × @c tile_height), e dois buffers de frame completos
 * para double buffering.
 *
 * @note Os arrays de assets usam @c uint8_t para acomodar bytes arbitrários
 *       dos códigos de escape ANSI usados na renderização.
 * @note @c vehicle_assets exclui os valores nulos (@c NO_VEHICLE,
 *       @c DIRECTION_NONE), portanto os índices são deslocados em -1 em
 *       relação ao valor original do enum. A função de mapeamento é
 *       responsável por aplicar esse offset.
 * @note @c tile_assets usa @c TILE_TYPE_COUNT posições, mas @c TileType
 *       não é contíguo (valores ASCII); a função de mapeamento converte
 *       o valor do enum para um índice consecutivo.
 */
struct Render {
    size_t tile_width;      /**< Largura de cada célula em caracteres. */
    size_t tile_height;     /**< Altura de cada célula em linhas. */

    uint8_t *vehicle_assets[VEHICLE_TYPE_COUNT - 1][DIRECTION_COUNT - 1]; /**< Assets indexados por (VehicleType-1, Direction-1). */
    uint8_t *tile_assets[TILE_TYPE_COUNT]; /**< Assets indexados por mapeamento consecutivo de TileType. */
    uint8_t *buffers[2];    /**< Buffers de frame: [active_buffer] é escrito, [1-active_buffer] é exibido. */

    int active_buffer;      /**< Índice do buffer sendo composto no tick atual (0 ou 1). */

    size_t buffer_width;    /**< Largura do buffer (tile_width * map_width). */
    size_t buffer_height;   /**< Altura do buffer (tile_height * map_height). */
};


/**
 * @internal
 * @brief Mapeia um TileType para um índice consecutivo no array tile_assets.
 *
 * Necessário pois TileType usa valores ASCII não contíguos.
 *
 * @return Índice válido em [0, TILE_TYPE_COUNT), ou -1 se o tipo for inválido.
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
 *        caracteres visuais (sem quebras de linha).
 *
 * Aloca exatamente @p asset_size bytes. Se o arquivo contiver menos
 * caracteres visuais que @p asset_size, o restante do buffer é preenchido
 * com espaços. Se contiver mais, os excedentes são ignorados.
 *
 * @param file_name  Caminho do arquivo a ser lido.
 * @param asset_size Número exato de bytes do asset (tile_width × tile_height).
 * @return Buffer alocado com o conteúdo do asset, ou NULL em caso de falha.
 */
static uint8_t *load_asset_from_file(const char *file_name, const size_t asset_size) {
    if (!file_name) {
        LOG("Error: parameter 'file_name' is NULL");
        return NULL;
    }

    if (asset_size == 0) {
        LOG("Error: parameter 'asset_size' cannot be 0");
        return NULL;
    }

    FILE *file = fopen(file_name, "r");
    if (!file) {
        LOG("Error: failed to open file '%s'", file_name);
        return NULL;
    }

    uint8_t *buffer = malloc(asset_size);
    if (!buffer) {
        LOG("Error: failed to allocate memory for 'buffer'");
        fclose(file);
        return NULL;
    }

    /* Preenche com espaços como fallback para arquivos menores que o esperado */
    memset(buffer, ' ', asset_size);

    size_t written = 0;
    int symbol;

    while ((symbol = fgetc(file)) != EOF && written < asset_size) {
        /* Ignora quebras de linha — armazena apenas caracteres visuais */
        if (symbol == '\n' || symbol == '\r') continue;
        buffer[written++] = (uint8_t)symbol;
    }

    fclose(file);
    return buffer;
}


/*============================================================================
 * API Pública
 * ============================================================================ */

/**
 * @internal
 * @brief Implementação da criação do renderizador.
 *
 * Valida as dimensões, aloca a estrutura e inicializa todos os ponteiros
 * de asset e buffer com NULL — garantindo que verificações de asset não
 * carregado sejam confiáveis.
 */
Render *render_new(const Map *map, const size_t tile_width, const size_t tile_height) {
    if (!tile_width || !tile_height) {
        LOG("Error: tile dimensions cannot be 0:\n"
            "tile_width: %zu,\ntile_height: %zu.",
            tile_width, tile_height);
        return NULL;
    }

    Render *render = malloc(sizeof(Render));
    if (!render) {
        LOG("Error: failed to allocate memory for 'render'");
        return NULL;
    }

    render->tile_width = tile_width;
    render->tile_height = tile_height;

    render->buffer_width = tile_width * map_get_width(map);
    render->buffer_height = tile_height * map_get_height(map);

    const size_t buffer_size = render->buffer_width * render->buffer_height;

    render->buffers[0] = malloc(buffer_size);
    if (!render->buffers[0]) {
        LOG("Error: failed to allocate memory for 'render->buffers[0]'");
        free(render);
        return NULL;
    }

    render->buffers[1] = malloc(buffer_size);
    if (!render->buffers[1]) {
        LOG("Error: failed to allocate memory for 'render->buffers[1]'");
        free(render->buffers[0]);
        free(render);
        return NULL;
    }

    render->active_buffer = 0;

    memset(render->tile_assets, 0, sizeof(render->tile_assets));
    memset(render->vehicle_assets, 0, sizeof(render->vehicle_assets));

    return render;
}

void render_destroy(Render *render) {
    if (!render) {
        LOG("Warning: parameter 'render' is NULL");
        return;
    }

    LOG_IF(!render->buffers[0], "Error: 'render->buffers[0]' is NULL on destroy");
    LOG_IF(!render->buffers[1], "Error: 'render->buffers[1]' is NULL on destroy");

    free(render->buffers[0]);
    free(render->buffers[1]);

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



/**
 * @internal
 * @brief Implementação do carregamento de asset para um único TileType.
 *
 * Libera o asset anterior do slot antes de substituí-lo (se houver),
 * garantindo que não haja vazamento de memória em recarregamentos.
 */
void render_load_tile_asset(Render *render,
        const TileType type, const char *file_name) {
    if (!render || !file_name) {
        LOG("Error: NULL parameter encountered: "
            "render: %p, file_name: %p.",
            render, file_name);
        return;
    }

    const int mapped_index = tile_type_to_index(type);
    if (mapped_index < 0) {
        LOG("Error: could not map a valid index for tile type: "
            "tile_type: %c, mapped_index: %d.",
            type, mapped_index);
        return;
    }

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *asset = load_asset_from_file(file_name, asset_size);
    if (!asset) {
        LOG("Error: failed to load asset from file '%s'", file_name);
        return;
    }

    free(render->tile_assets[mapped_index]);
    render->tile_assets[mapped_index] = asset;
}


/**
 * @internal
 * @brief Implementação do carregamento de asset para múltiplos TileTypes.
 *
 * Lê o arquivo uma única vez e copia o buffer para cada slot indicado
 * em @p types, garantindo que cada slot possua memória independente.
 * Slots com asset anterior são liberados antes da substituição.
 */
void render_load_tile_asset_multi(Render *render,
        const char *file_name, const TileType *types, const int count) {
    if (!render || !file_name || !types) {
        LOG("Error: NULL parameter encountered: "
            "render: %p, file_name: %p, types: %p.",
            render, file_name, types);
        return;
    }

    if (count <= 0) {
        LOG("Error: invalid value for parameter 'count': %d", count);
        return;
    }

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *source = load_asset_from_file(file_name, asset_size);
    if (!source) {
        LOG("Error: failed to load asset from file '%s'", file_name);
        return;
    }

    for (int i = 0; i < count; i++) {
        const int index = tile_type_to_index(types[i]);
        if (index < 0) {
            LOG("Warning: skipping invalid tile type at index %d: %c", i, types[i]);
            continue;
        }

        uint8_t *copy = malloc(asset_size);
        if (!copy) {
            LOG("Warning: failed to allocate memory for tile type %c at index %d,"
                "skipping.", types[i], i);
            continue;
        }

        memcpy(copy, source, asset_size);
        free(render->tile_assets[index]);
        render->tile_assets[index] = copy;
    }

    free(source);
}


/**
 * @internal
 * @brief Implementação do carregamento de asset de veículo para uma
 *        direção específica.
 *
 * Libera o asset anterior do slot antes de substituí-lo. Os índices
 * são deslocados em -1 para excluir os valores nulos das enums
 * (NO_VEHICLE e DIRECTION_NONE).
 */
void render_load_vehicle_asset(Render *render,
        const VehicleType type, const Direction direction,
        const char *file_name) {
    if (!render || !file_name) {
        LOG("Error: NULL parameter encountered: "
            "render: %p, file_name: %p.",
            render, file_name);
        return;
    }

    if (type <= NO_VEHICLE || type >= VEHICLE_TYPE_COUNT) {
        LOG("Error: invalid vehicle type: %d", type);
        return;
    }

    if (direction <= DIRECTION_NONE || direction >= DIRECTION_COUNT) {
        LOG("Error: invalid direction: %d", direction);
        return;
    }

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *asset = load_asset_from_file(file_name, asset_size);
    if (!asset) {
        LOG("Error: failed to load asset from file '%s'", file_name);
        return;
    }

    const int t = (int)type - 1;
    const int d = (int)direction - 1;

    free(render->vehicle_assets[t][d]);
    render->vehicle_assets[t][d] = asset;
}


/**
 * @internal
 * @brief Implementação do carregamento de asset de veículo para todas
 *        as direções.
 *
 * Lê o arquivo uma única vez e copia o buffer para cada slot de direção
 * válida do tipo informado. Cada slot recebe memória independente,
 * permitindo que direções individuais sejam sobrescritas posteriormente
 * via render_load_vehicle_asset sem afetar os demais slots.
 */
void render_load_vehicle_asset_all_directions(Render *render,
    const VehicleType type, const char *file_name) {
    if (!render || !file_name) {
        LOG("Error: NULL parameter encountered: "
            "render: %p, file_name: %p.",
            render, file_name);
        return;
    }

    if (type <= NO_VEHICLE || type >= VEHICLE_TYPE_COUNT) {
        LOG("Error: invalid vehicle type: %d", type);
        return;
    }

    const size_t asset_size = render->tile_width * render->tile_height;
    uint8_t *source = load_asset_from_file(file_name, asset_size);
    if (!source) {
        LOG("Error: failed to load asset from file '%s'", file_name);
        return;
    }

    const int t = (int)type - 1;

    for (int d = 0; d < DIRECTION_COUNT - 1; d++) {
        uint8_t *copy = malloc(asset_size);
        if (!copy) {
            LOG("Warning: failed to allocate memory for direction index %d,"
                "skipping.", d);
            continue;
        }

        memcpy(copy, source, asset_size);
        free(render->vehicle_assets[t][d]);
        render->vehicle_assets[t][d] = copy;
    }

    free(source);
}

void *render_update(void *render_args) {
    RenderArgs *args = (RenderArgs *)render_args;


}
