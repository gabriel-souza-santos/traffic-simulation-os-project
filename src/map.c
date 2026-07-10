/**
 * @file map.c
 * @brief Implementação do mapa da malha viária e do controle de ocupação das células.
 *
 * @author Gabriel Souza
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
    TileType type;        /**< O tipo de terreno desta coordenada contendo direção de fluxo. */
    bool is_occupied;     /**< Estado de ocupação: true se é bloqueada ou há um carro, false se está livre. */
} Tile;


/**
 * @internal
 * @brief Estrutura interna do mapa: grid de tiles e mecanismo de sincronização.
 *
 * Atualmente utiliza um único mutex global (@c mutex) que protege todo o
 * array de tiles. Qualquer leitura ou escrita ao campo @c is_occupied de
 * qualquer tile deve ocorrer com este mutex adquirido.
 *
 * @warning Granularidade grossa (lock único para o mapa). Planejado
 *          para ser substituído em iterações futuras.
 */
struct Map {
    // TODO: Remover trava global, implementada momentaneamente para acelerar o desenvolvimento
    pthread_mutex_t mutex;  /**< Mutex global. */
    Tile *tiles;            /**< Array unidimensional de células. */
    size_t width;           /**< Largura do mapa. */
    size_t height;          /**< Altura do mapa. */
};


/**
 * @internal
 * @brief Calcula o endereço do tile correspondente a uma coordenada 2D.
 *
 * Converte a coordenada (x, y) para o índice linear no array 1D de tiles
 * (@p map->tiles), assumindo layout row-major (linha por linha).
 *
 * @warning Não realiza verificação de limites (bounds checking);
 *          responsabilidade é do chamador fazer essa verificação.
 *
 * @see map_is_within_bounds()
 */
static Tile* tile_at(const Map *map, const Coord position) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return NULL;
    }

    const size_t index = position.y * map->width + position.x;
    return &map->tiles[index];
}


/**
 * @internal
 * @brief Verifica se um tipo de tile representa uma via transitável válida.
 *
 * Considera via qualquer uma das quatro orientações de fluxo
 * (TILE_ROAD_UP/DOWN/LEFT/RIGHT). Não inclui TILE_ROAD genérico,
 * TILE_WAIT ou TILE_BLOCKED.
 */
static bool is_valid_road(const TileType tile_type) {
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


/**
 * @internal
 * @brief Determina largura e altura do mapa a partir do arquivo-fonte.
 *
 * Percorre o arquivo caractere a caractere, contando colunas por linha e
 * número de linhas válidas. Espaços são ignorados na contagem de largura;
 * a largura final é definida pela primeira linha não vazia encontrada.
 *
 * @note Utiliza-se @c int (não @c char) para armazenar o retorno,
 *       já que @c fgetc() retorna valores de 0 a 255 ou o macro @c EOF
 *       (-1); um @c char de 8 bits não diferencia @c EOF de um caractere
 *       válido de forma segura.
 *
 * @param file Arquivo já aberto, contendo dados do mapa.
 * @param[out] out_width Largura detectada.
 * @param[out] out_height Altura detectada.
 */
static void find_dimensions(FILE *file, size_t *out_width, size_t *out_height) {
    if (!file || !out_width || !out_height) {
        LOG("Error: NULL pointer encountered.\n"
            "Passed Arguments:\n"
            "file: %p,\nout_width: %p,\nout_height: %p.",
            file, out_width, out_height);
        return;
    }

    int symbol;
    int current_width = 0;
    size_t width = 0;
    size_t height = 0;

    rewind(file); // Volta ao início do arquivo

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


/**
 * @internal
 * @brief Preenche as células do mapa a partir do conteúdo do arquivo.
 *
 * Percorre o arquivo aberto, ignorando quebras de linha e espaços,
 * mapeando cada caractere válido para o TileType correspondente.
 * Caracteres não reconhecidos marcam o tile como TILE_BLOCKED e
 * já o definem como ocupado.
 *
 * @param map Mapa já alocado com dimensões e array de tiles definidos.
 * @param file Arquivo de onde os símbolos serão lidos.
 */
static void fill_tiles(const Map *map, FILE *file) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return;
    }

    int symbol;
    Coord current_position = {0, 0};

    rewind(file); // Volta ao início do arquivo

    while ((symbol = fgetc(file)) != EOF && (size_t)current_position.y < map->height) {
        // Ignora quebras de linha e avança o eixo Y
        if (symbol == '\n' || symbol == '\r') {
            if (current_position.x > 0) {
                current_position.y++;
                current_position.x = 0;
            }
            continue;
        }

        if (symbol == ' ') continue; // Ignora espaços em branco

        // Preenche a célula se estiver dentro dos limites lógicos
        if ((size_t)current_position.x < map->width) {
            Tile *tile = tile_at(map, current_position);

            switch (symbol) {
                case TILE_ROAD_UP:
                case TILE_ROAD_DOWN:
                case TILE_ROAD_LEFT:
                case TILE_ROAD_RIGHT:
                case TILE_TURN_UP:
                case TILE_TURN_DOWN:
                case TILE_TURN_LEFT:
                case TILE_TURN_RIGHT:
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


/**
 * @internal
 * @brief Implementação da construção do mapa a partir de arquivo.
 *
 * Em duas passagens: primeiro calcula as dimensões (@c find_file_dimensions),
 * depois aloca a estrutura e o array de tiles com calloc, garantindo que
 * o estado padrão (@c TILE_BLOCKED, e @c is_occupied como @c false).
 * E por fim popula o grid com @c fill_tiles.
 *
 * @warning Falhas em qualquer etapa de alocação resultam em cleanup
 *          manual e retorno de NULL.
 */
Map *map_new(const char *file_path) {
    if (!file_path) {
        LOG("Error: file path not provided.");
        return NULL;
    }

    FILE *file = fopen(file_path, "r");
    if (!file) {
        LOG("Error: failed to open file '%s'.", file_path);
        return NULL;
    }

    size_t width = 0;
    size_t height = 0;

    find_dimensions(file, &width, &height);

    // Arquivo vazio/inválido
    if (width == 0 || height == 0) {
        LOG("Error: width or height is zero."
            "File '%s' contains invalid map dimensions:\n"
            "width: %zu,\n height: %zu.\n"
            "Width and Height are calculated from valid characters.",
            file_path, width, height);
        fclose(file);
        return NULL;
    }

    // Alocação da estrutura do Map
    Map *map = malloc(sizeof(Map));
    if (!map) {
        LOG("Error: failed to allocate memory for 'map'.");
        fclose(file);
        return NULL;
    }

    map->width = width;
    map->height = height;

    if (pthread_mutex_init(&map->mutex, NULL) != 0) {
        LOG("Error: failed to initialize 'map->mutex'.");
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
        LOG("Error: failed to allocate memory for 'map->tiles'.");
        free(map);
        fclose(file);
        return NULL;
    }

    fill_tiles(map, file);

    fclose(file);
    return map;
}


/**
 * @internal
 * @brief Implementação da liberação dos recursos do mapa.
 *
 * Usa as macros CHECK_NULL e TRY para validação defensiva
 * e verificação do retorno de pthread_mutex_destroy.
 */
void map_destroy(Map *map) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return;
    }

    if (!map->tiles) {
        free(map);
        LOG("Error: 'map->tiles' is NULL.");
        return;
    }

    TRY(pthread_mutex_destroy(&map->mutex));
    free(map->tiles);
    free(map);
}


/**
 * @internal
 * @brief Retorna o campo @p map->width armazenado na estrutura Map.
 *
 * Leitura simples, sem necessidade de sincronização, pois é definido
 * uma única vez na criação do mapa e nunca alterado depois.
 */
size_t map_get_width(const Map *map) {
    return map? map->width : 0;
}


/**
 * @internal
 * @brief Retorna o campo @p map->height armazenado na estrutura Map.
 *
 * Leitura simples, sem necessidade de sincronização, pois é definido
 * uma única vez na criação do mapa e nunca alterado depois.
 */
size_t map_get_height(const Map *map) {
    return map? map->height : 0;
}


/**
 * @internal
 * @brief Implementação da verificação de limites do mapa.
 *
 * Compara @p position contra @p map->width e @p map->height após o
 * cast para @c size_t, o que naturalmente descarta valores negativos,
 * já que x/y negativos se tornam valores muito altos ao serem convertidos.
 */
bool map_is_within_bounds(const Map *map, const Coord position) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return false;
    }

    return position.x >= 0 &&
           position.y >= 0 &&
           (size_t)position.x < map->width &&
           (size_t)position.y < map->height;
}


/**
 * @internal
 * @brief Implementação da consulta de tipo de tile.
 *
 * Faz verificação de limites com @c map_is_within_bounds, caso esteja
 * fora dos limites do mapa, retorna @c TILE_BLOCKED. Delega para
 * @c tile_at buscar a célula do mapa e retorna o seu tipo.
 *
 */
TileType map_get_tile_type(const Map *map, const Coord position) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return TILE_BLOCKED;
    }

    if (!map_is_within_bounds(map, position)) return TILE_BLOCKED;
    return tile_at(map, position)->type;
}


/**
 * @internal
 * @brief Implementação da verificação de célula bloqueada.
 *
 * Consulta direta de @c tile->type sem necessidade de lock, já que o
 * campo type é imutável após a construção do mapa, definido apenas em
 * @c fill_tiles, nunca reescrito depois.
 *
 * @see fill_tiles()
 */
bool map_is_blocked(const Map *map, const Coord position) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return false;
    }
    return tile_at(map, position)->type == TILE_BLOCKED;
}


/**
 * @internal
 * @brief Implementação da verificação de ocupação.
 *
 * Valida os limites antes de qualquer acesso (retornando true/"ocupado"
 * para posições fora do mapa, por segurança). Em seguida, adquire o
 * mutex global do mapa para ler @c tile->is_occupied de forma thread-safe.
 */
bool map_is_occupied(Map *map, const Coord position) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return true;
    }

    if (map_is_within_bounds(map, position) == false) {
        return true;
    }

    TRY(pthread_mutex_lock(&map->mutex));
    const Tile *tile = tile_at(map, position);

    const bool is_occupied = tile->is_occupied ||
                             tile->type == TILE_BLOCKED;

    TRY(pthread_mutex_unlock(&map->mutex));
    return is_occupied;
}


/**
 * @internal
 * @brief Implementação da transferência de ocupação entre duas células.
 *
 * Adquire o mutex global do mapa antes de inspecionar/alterar qualquer
 * tile, garantindo que a verificação de disponibilidade do destino e a
 * atualização de ambos os tiles ocorram como operação atômica — evitando
 * condição de corrida em que dois veículos viam a célula de destino livre
 * simultaneamente.
 *
 * A transferência só ocorre se o destino não for TILE_BLOCKED e não
 * estiver ocupado; caso contrário, nenhum estado é alterado.
 */
bool map_transfer_occupant(Map *map, const Coord from, const Coord to) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return true;
    }

    if (!map_is_within_bounds(map, from) || !map_is_within_bounds(map, to)) {
        LOG("Warning: coordinate is out of bounds.\n"
            "Map dimensions: %zu x %zu.\n"
            "From x: %d, y: %d,\nto x: %d, y: %d.",
            map->width, map->height,
            from.x, from.y, to.x, to.y);
        return false;
    }

    bool has_moved = false;
    TRY(pthread_mutex_lock(&map->mutex));

    Tile *origin = tile_at(map, from);
    Tile *destination = tile_at(map, to);

    if (destination->type != TILE_BLOCKED && !destination->is_occupied) {
        origin->is_occupied = false;
        destination->is_occupied = true;
        has_moved = true;
    }

    TRY(pthread_mutex_unlock(&map->mutex));
    return has_moved;
}


/**
 * @internal
 * @brief Implementação da busca e reserva de ponto de spawn.
 *
 * Mantém o mutex global adquirido durante toda a varredura linear do
 * grid (linha por linha), retornando antecipadamente (com unlock) na
 * primeira célula de via livre encontrada. Caso nenhuma célula livre
 * seja encontrada, libera o mutex e retorna NULL_COORD.
 *
 * @warning Varredura sequencial simples (sem aleatoriedade); todos os
 *          spawns tendem a ocorrer nas primeiras posições do grid até
 *          que estas fiquem ocupadas.
 */
Coord map_reserve_spawn_point(Map *map) {
    if (!map) {
        LOG("Error: parameter 'map' is NULL.");
        return NULL_COORD;
    }

    TRY(pthread_mutex_lock(&map->mutex));
    Coord spawn_point;


    for (size_t y = 2; y < map->height; y++) {
        for (size_t x = 0; x < map->width; x++) {
            spawn_point.x = (int)x;
            spawn_point.y = (int)y;
            Tile *tile = tile_at(map, spawn_point);

            if (is_valid_road(tile->type) && !tile->is_occupied) {
                tile->is_occupied = true;

                TRY(pthread_mutex_unlock(&map->mutex));
                return spawn_point;
            }
        }
    }

    TRY(pthread_mutex_unlock(&map->mutex));
    LOG("Warning: failed to found an available spawn point coordinate.");
    return NULL_COORD;
}
