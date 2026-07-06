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
 * Cada buffer é autocontido: inclui o header ANSI (\033[H), os dados de
 * cada linha e os terminadores \r\n — eliminando a necessidade de um
 * buffer de output separado. O fwrite em render_flush opera diretamente
 * sobre o buffer ativo.
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
    size_t tile_width;  /**< Largura de cada célula em caracteres. */
    size_t tile_height; /**< Altura de cada célula em linhas. */

    uint8_t *vehicle_assets[VEHICLE_TYPE_COUNT - 1][DIRECTION_COUNT - 1]; /**< Assets indexados por (VehicleType-1, Direction-1). */
    uint8_t *tile_assets[TILE_TYPE_COUNT]; /**< Assets indexados por mapeamento consecutivo de TileType. */

    uint8_t *buffers[2]; /**< Buffers autocontidos: [\033[H][row0][\r\n][row1][\r\n]... */
    int active_buffer;   /**< Índice do buffer sendo composto no tick atual (0 ou 1). */

    size_t buffer_width;  /**< Largura do buffer em caracteres (tile_width * map_width). */
    size_t buffer_height; /**< Altura do buffer em linhas (tile_height * map_height). */
};


/* ============================================================================
 * Funções auxiliares internas
 * ============================================================================ */

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


/**
 * @internal
 * @brief Pré-inicializa um buffer com o header ANSI, espaços e terminadores
 *        de linha — definindo o layout fixo do buffer autocontido.
 *
 * Layout resultante:
 * @code
 * ['\033','[','H'] | [row0: buffer_width espaços]['\r','\n'] | [row1...]...
 * @endcode
 *
 * As sequências \r\n e o header \033[H nunca são sobrescritos por
 * render_write_tile, que escreve apenas na região de dados de cada linha.
 *
 * @param buffer       Buffer já alocado a ser inicializado.
 * @param buffer_width Largura de uma linha de dados em caracteres.
 * @param buffer_height Número de linhas do frame.
 */
static void render_init_buffer(uint8_t *buffer,
        const size_t buffer_width, const size_t buffer_height) {

    size_t out = 0;

    /* Header ANSI: posiciona cursor na home sem limpar a tela */
    buffer[out++] = '\033';
    buffer[out++] = '[';
    buffer[out++] = 'H';

    for (size_t row = 0; row < buffer_height; row++) {
        memset(buffer + out, ' ', buffer_width);
        out += buffer_width;
        buffer[out++] = '\r';
        buffer[out++] = '\n';
    }
}


/**
 * @internal
 * @brief Escreve um asset no buffer ativo na posição de tile (x, y).
 *
 * Considera o layout autocontido do buffer (header de 3 bytes + \r\n por
 * linha), escrevendo o asset linha por linha para respeitar o row-major
 * layout do frame.
 *
 * Se @p asset for NULL, preenche a região do tile com espaços.
 *
 * @param render Instância do renderizador.
 * @param x      Coluna do tile (em unidades de tile, não de caractere).
 * @param y      Linha do tile (em unidades de tile, não de caractere).
 * @param asset  Buffer do asset a escrever, ou NULL para espaços em branco.
 */
static void render_write_tile(Render *render,
        const size_t x, const size_t y, const uint8_t *asset) {

    uint8_t *buffer = render->buffers[render->active_buffer];

    const size_t row_stride  = render->buffer_width + 2; /* +2 para \r\n */
    const size_t home_offset = 3;                        /* \033[H */

    for (size_t row = 0; row < render->tile_height; row++) {
        const size_t buf_offset =
            home_offset +
            (y * render->tile_height + row) * row_stride +
            (x * render->tile_width);

        const size_t asset_offset = row * render->tile_width;

        if (asset) {
            memcpy(buffer + buf_offset, asset + asset_offset, render->tile_width);
        } else {
            memset(buffer + buf_offset, ' ', render->tile_width);
        }
    }
}


/**
 * @internal
 * @brief Retorna o asset de um veículo para uma combinação de tipo e direção.
 *
 * Retorna NULL se a combinação for inválida ou se nenhum asset tiver sido
 * carregado para ela — o fallback de espaços em branco é aplicado por
 * render_write_tile.
 *
 * @param render    Instância do renderizador.
 * @param type      Tipo do veículo.
 * @param direction Direção atual do veículo.
 * @return Ponteiro para o buffer do asset, ou NULL.
 */
static const uint8_t *render_get_vehicle_asset(Render *render,
        const VehicleType type, const Direction direction) {

    if (type <= NO_VEHICLE || type >= VEHICLE_TYPE_COUNT) return NULL;
    if (direction <= DIRECTION_NONE || direction >= DIRECTION_COUNT) return NULL;

    const int t = (int)type - 1;
    const int d = (int)direction - 1;

    return render->vehicle_assets[t][d];
}


/**
 * @internal
 * @brief Envia o buffer ativo para o terminal e troca o buffer ativo.
 *
 * Realiza um único fwrite do buffer autocontido (já inclui \033[H e \r\n),
 * seguido de fflush. Após o flush, troca o active_buffer para o próximo
 * frame ser composto no buffer inativo.
 *
 * @param render Instância do renderizador.
 */
static void render_flush(Render *render) {
    const size_t total = 3 + render->buffer_height * (render->buffer_width + 2);
    fwrite(render->buffers[render->active_buffer], 1, total, stdout);
    fflush(stdout);
    render->active_buffer ^= 1;
}


/* ============================================================================
 * API Pública
 * ============================================================================ */

/**
 * @internal
 * @brief Implementação da criação do renderizador.
 *
 * Valida as dimensões, aloca a estrutura, inicializa os dois buffers de
 * frame com layout autocontido (render_init_buffer), e zera os arrays de
 * assets. Não há buffer de output separado — cada buffer já está pronto
 * para fwrite após render_write_tile.
 */
Render *render_new(const Map *map, const size_t tile_width, const size_t tile_height) {
    if (!tile_width || !tile_height) {
        LOG("Error: tile dimensions cannot be 0: "
            "tile_width: %zu, tile_height: %zu.",
            tile_width, tile_height);
        return NULL;
    }

    Render *render = malloc(sizeof(Render));
    if (!render) {
        LOG("Error: failed to allocate memory for 'render'");
        return NULL;
    }

    render->tile_width  = tile_width;
    render->tile_height = tile_height;

    render->buffer_width  = tile_width  * map_get_width(map);
    render->buffer_height = tile_height * map_get_height(map);

    /* Tamanho do buffer autocontido: header(3) + height * (width + \r\n(2)) */
    const size_t buffer_size = 3 + render->buffer_height * (render->buffer_width + 2);

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

    render_init_buffer(render->buffers[0], render->buffer_width, render->buffer_height);
    render_init_buffer(render->buffers[1], render->buffer_width, render->buffer_height);

    render->active_buffer = 0;

    memset(render->tile_assets,    0, sizeof(render->tile_assets));
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
            LOG("Warning: failed to allocate memory for tile type %c at index %d, "
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
            LOG("Warning: failed to allocate memory for direction index %d, "
                "skipping.", d);
            continue;
        }

        memcpy(copy, source, asset_size);
        free(render->vehicle_assets[t][d]);
        render->vehicle_assets[t][d] = copy;
    }

    free(source);
}


/**
 * @internal
 * @brief Implementação da rotina principal da thread do renderizador.
 *
 * No tick 0, realiza um full draw do mapa e dos veículos. Nos ticks
 * seguintes, opera em modo seletivo: atualiza apenas as células
 * afetadas por movimentos aprovados no tick anterior, lidos via
 * analyser_get_previous_requests.
 *
 * Em ambos os casos, ao final compõe o frame com render_flush e bloqueia
 * até o próximo tick via clock_signal.
 */
void *render_update(void *render_args) {
    if (!render_args) {
        LOG("Error: parameter 'render_args' is NULL.");
        return NULL;
    }

    const RenderArgs *args = (RenderArgs *)render_args;

    Analyser  *analyser = args->analyser;
    Clock     *clock    = args->clock;
    Map       *map      = args->map;
    Render    *render   = args->render;
    Vehicle  **vehicles = args->vehicles;

    const size_t map_width  = map_get_width(map);
    const size_t map_height = map_get_height(map);

    while (1) {
        const size_t current_tick = clock_get_tick(clock);

        if (current_tick == 0) {

            /* Full draw: compõe o mapa inteiro */
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

            /* Sobrepõe veículos no estado inicial */
            for (int i = 0; i < VEHICLE_COUNT; i++) {
                const Coord pos = vehicle_get_position(vehicles[i]);
                const VehicleType type = vehicle_get_type(vehicles[i]);
                const Direction dir = vehicle_get_direction(vehicles[i]);

                const uint8_t *asset = render_get_vehicle_asset(render, type, dir);
                render_write_tile(render, (size_t)pos.x, (size_t)pos.y, asset);
            }

        } else {

            /* Redesenho seletivo: apenas movimentos aprovados */
            const MovementRequest *requests = analyser_get_previous_requests(analyser);

            for (int i = 0; i < VEHICLE_COUNT; i++) {
                if (requests[i].status != REQUEST_APPROVED) continue;

                const Coord from = requests[i].from;
                const Coord to   = requests[i].to;

                /* Restaura tile de origem — veículo saiu de lá */
                const TileType from_type  = map_get_tile_type(map, from);
                const int from_index = tile_type_to_index(from_type);
                const uint8_t *tile_asset = (from_index >= 0 && render->tile_assets[from_index])
                    ? render->tile_assets[from_index]
                    : NULL;
                render_write_tile(render, (size_t)from.x, (size_t)from.y, tile_asset);

                /* Desenha veículo na célula de destino */
                const VehicleType type = vehicle_get_type(vehicles[i]);
                const Direction   dir  = vehicle_get_direction(vehicles[i]);
                const uint8_t *veh_asset = render_get_vehicle_asset(render, type, dir);
                render_write_tile(render, (size_t)to.x, (size_t)to.y, veh_asset);
            }
        }

        /* Flush e sinalização ao clock */
        render_flush(render);
        clock_signal(clock, current_tick);
    }

    return NULL;
}