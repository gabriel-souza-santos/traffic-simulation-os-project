/**
 * @file map.c
 *
 * @date 2026-06-28
 */

#include <stdio.h>
#include <pthread.h>
#include "map.h"
#include "debug.h"

/**
 * @internal
 * @brief Representa uma única unidade (quadrado) de espaço dentro do mapa.
 *
 * É a estrutura central para a resolução de concorrência. Une as propriedades
 * geográficas da via com o mecanismo que garante a lei de impenetrabilidade
 * dos veículos.
 */
typedef struct {
    TileType type;        /**< A direção do fluxo ou tipo de terreno desta coordenada. */
    bool is_occupied;     /**< Estado de ocupação: true se há um carro parado aqui, false se está livre. */
} Tile;


struct Map {
    pthread_mutex_t mutex;
    Tile *tiles;
    size_t width;
    size_t height;
};

static Tile* tile_at(const Map *map, const Coord position) {
    if (!map) return NULL;
    const size_t index = position.y * map->width + position.x;
    return &map->tiles[index];
}

static bool is_tile_road(const TileType tile_type) {
    switch (tile_type) {
        case TILE_ROAD_UP:
        case TILE_ROAD_DOWN:
        case TILE_ROAD_LEFT:
        case TILE_ROAD_RIGHT:
            return true;
        default:
            return false;
    }
}

/*
 * NOTA DE IMPLEMENTAÇÃO:
 * Nas funções internas find_file_dimensions e fill_tiles
 * utiliza-se 'int' em vez de 'char' para armazenar o retorno de fgetc().
 * Como fgetc() retorna os caracteres válidos (0 a 255) ou o macro EOF (-1)
 * no fim do arquivo, um tipo 'char' de 8 bits não consegue diferenciar o EOF
 * de um caractere válido de forma segura (sofrendo overflow ou colisão de valor).
 * O tipo 'int' de 32 bits acomoda todas as possibilidades sem ambiguidades.
 */

static void find_file_dimensions(FILE *file, size_t *out_width, size_t *out_height) {
    int symbol;
    int current_width = 0;
    size_t width = 0;
    size_t height = 0;

    while ((symbol = fgetc(file)) != EOF) {
        if (symbol == '\n' || symbol == '\r') {
            if (current_width > 0) {
                if (width == 0) width = current_width; // Define a largura baseada na primeira linha válida
                height++;
                current_width = 0;
            }
        } else if (symbol != ' ') {
            current_width++;
        }
    }
    // Captura a última linha caso o arquivo não termine com '\n'
    if (current_width > 0) {
        if (width == 0) width = current_width;
        height++;
    }

    *out_width = width;
    *out_height = height;
}

static void fill_tiles(const Map *map, FILE *file) {
    if (!map) return;

    int symbol;

    // Volta ao início do arquivo
    rewind(file);
    Coord current_position = {0, 0};

    while ((symbol = fgetc(file)) != EOF && (size_t)current_position.y < map->height) {
        // Ignora quebras de linha e avança o eixo Y
        if (symbol == '\n' || symbol == '\r') {
            if (current_position.x > 0) {
                current_position.y++;
                current_position.x = 0;
            }
            continue;
        }

        // Ignora espaços em branco completamente
        if (symbol == ' ') continue;

        // Preenche o grid se estivermos dentro dos limites lógicos
        if ((size_t)current_position.x < map->width) {
            Tile *tile = tile_at(map, current_position);

            switch (symbol) {
                case TILE_ROAD_UP:
                case TILE_ROAD_DOWN:
                case TILE_ROAD_LEFT:
                case TILE_ROAD_RIGHT:
                case TILE_ROAD:
                case TILE_WAIT:
                    tile->type = (TileType)symbol;
                    break;
                default:
                    tile->type = TILE_BLOCKED;
                    tile->is_occupied = true;
                    break;
            }
            current_position.x++;
        }
    }
}


/*
 * ============================================================================
 * API Pública
 * ============================================================================
 */


Map *map_new(const char *file_path) {
    if (!file_path) return NULL;

    FILE *file = fopen(file_path, "r");
    if (!file) {
        // TODO: Tratar erro de abertura do arquivo
        return NULL;
    }

    size_t width = 0;
    size_t height = 0;

    find_file_dimensions(file, &width, &height);

    // Arquivo vazio/inválido
    if (width == 0 || height == 0) {
        fclose(file);
        return NULL;
    }

    // Alocação da estrutura do Map
    Map *map = malloc(sizeof(Map));
    if (!map) {
        fclose(file);
        return NULL;
    }

    map->width = width;
    map->height = height;

    if (pthread_mutex_init(&map->mutex, NULL) != 0) {
        free(map);
        fclose(file);
        return NULL;
    }

    /*
     * Aloca o array 1D usando calloc para que tudo seja inicializado com 0
     * Como TILE_BLOCKED = '\0' e is_occupied = false (0), o estado padrão é
     * assegurado com o calloc.
     */
    map->tiles = calloc(width * height, sizeof(Tile));
    if (!map->tiles) {
        free(map);
        fclose(file);
        return NULL;
    }

    fill_tiles(map, file);

    fclose(file);
    return map;
}


void map_destroy(Map *map) {
    CHECK_NULL(map);
    CHECK_NULL(map->tiles);

    TRY(pthread_mutex_destroy(&map->mutex));

    free(map->tiles);
    free(map);
}

size_t map_get_width(const Map *map) {
    return map? map->width : 0;
}

size_t map_get_height(const Map *map) {
    return map? map->height : 0;
}

TileType map_get_tile_type(const Map *map, const Coord position) {
    if (!map) return TILE_BLOCKED;
    return tile_at(map, position)->type;
}

bool map_is_within_bounds(const Map *map, const Coord position) {
    if (!map) return false;

    return position.x >= 0 &&
           position.y >= 0 &&
           (size_t)position.x < map->width &&
           (size_t)position.y < map->height;
}

bool map_is_blocked(const Map *map, const Coord position) {
    if (!map) return false;
    return tile_at(map, position)->type == TILE_BLOCKED;
}

bool map_is_occupied(Map *map, const Coord position) {
    if (!map) return true;

    if (map_is_within_bounds(map, position) == false) {
        return true;
    }

    pthread_mutex_lock(&map->mutex);
    const bool is_occupied = tile_at(map, position)->is_occupied;
    pthread_mutex_unlock(&map->mutex);

    return is_occupied;
}

bool map_transfer_occupant(Map *map, const Coord from, const Coord to) {
    if (!map || !map_is_within_bounds(map, from) || !map_is_within_bounds(map, to)) {
        return false;
    }

    bool has_moved = false;

    // TODO: Remover trava global, implementada momentaneamente para acelerar o desenvolvimento

    pthread_mutex_lock(&map->mutex);

    Tile *origin = tile_at(map, from);
    Tile *destination = tile_at(map, to);

    if (destination->type != TILE_BLOCKED && !destination->is_occupied) {
        origin->is_occupied = false;
        destination->is_occupied = true;
        has_moved = true;
    }

    pthread_mutex_unlock(&map->mutex);

    return has_moved;
}

Coord map_reserve_spawn_point(Map *map) {
    if (!map) return NULL_COORD;

    pthread_mutex_lock(&map->mutex);
    Coord spawn_point;

    // TODO: Gerar melhor distribuição para implementações futuras

    for (size_t y = 0; y < map->height; y++) {
        for (size_t x = 0; x < map->width; x++) {
            spawn_point.x = (int)x;
            spawn_point.y = (int)y;
            Tile *tile = tile_at(map, spawn_point);

            if (is_valid_road(tile->type) && !tile->is_occupied) {
                tile->is_occupied = true;

                pthread_mutex_unlock(&map->mutex);
                return spawn_point;
            }
        }
    }

    pthread_mutex_unlock(&map->mutex);
    return NULL_COORD;
}
