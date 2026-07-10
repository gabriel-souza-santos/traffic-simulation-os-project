/**
 * @file map.h
 *
 * @brief Definição da interface pública do mapa e manipulação de coordenadas.
 *
 * * Nomenclaturas: uma célula bloqueada (blocked) é uma célula na qual veículos
 * não podem transitar e, por padrão, são tratadas como ocupadas. Células que
 * não são bloqueadas podem estar ocupadas se houver um veículo nelas, mas
 * também podem estar livres.
 *
 * @date 2026-06-28
 */
#ifndef URBAN_TRAFFIC_MAP_H
#define URBAN_TRAFFIC_MAP_H

#include <stddef.h>
#include <stdbool.h>


/**
 * @brief Representação de uma coordenada bidimensional na malha do mapa.
 * Projetada para ser passada por valor (passed-by-value) por se tratar
 * de uma estrutura leve (8 bytes em sistemas de 64-bit).
 */
typedef struct {
    int x;  /**< Posição horizontal (coluna da matriz). */
    int y;  /**< Posição vertical (linha da matriz). */
} Coord;

/** @brief Valor auxiliar para representar coordenadas inválidas */
#define NULL_COORD ((Coord){-1, -1})

/**
 * @brief Define a natureza física e a direção das células do mapa.
 *
 * Utilizado para guiar a lógica de movimentação dos veículos.
 * O código deve verificar esta informação antes de atualizar a posição
 * do veículo.
 */
typedef enum {
    TILE_BLOCKED    = '\0', /**< Célula intransitável (Veículos não entram). */
    TILE_ROAD_UP    = '^',  /**< Via com sentido norte (para cima na matriz). */
    TILE_ROAD_DOWN  = 'v',  /**< Via com sentido sul (para baixo na matriz). */
    TILE_ROAD_LEFT  = '<',  /**< Via com sentido oeste (esquerda na matriz). */
    TILE_ROAD_RIGHT = '>',  /**< Via com sentido leste (direita na matriz). */
    TILE_ROAD       = '.',  /**< Via sem sentido definido (usado em interseções) */
    TILE_WAIT       = '!',  /**< Célula que indica uma área de espera/parada */

    /* O veículo pode escolher mudar a direção ou seguir com a sua */
    TILE_TURN_LEFT  = 'l',
    TILE_TURN_RIGHT = 'r',
    TILE_TURN_UP    = 'u',
    TILE_TURN_DOWN  = 'd',
} TileType;

#define TILE_TYPE_COUNT 11

/**
 * @brief Estrutura opaca para o mapa, para reforçar o encapsulamento.
 *
 * Deve ser sempre usada por meio de um ponteiro:
 * @code{.c}
 * Map *map;
 * @endcode
 */
typedef struct Map Map;


/**
 * @brief Aloca recursos, inicializa recursos internos e carrega o mapa.
 *
 * O mapa é construído em memória a partir de um arquivo de configuração
 * conforme os símbolos definidos em TileType. Qualquer caractere desconhecido
 * é tratado como TILE_BLOCKED.
 * * @see TileType
 *
 * @param file_path Caminho do arquivo de texto contendo a disposição da malha viária.
 * @return Ponteiro para a instância do mapa criado, ou `NULL` em caso de falha de leitura.
 */
Map *map_new(const char *file_path);


/**
 * @brief Libera toda a memória alocada para a estrutura do mapa, destruindo
 * também todos os recursos associados às células viárias.
 *
 * @param map Ponteiro para o mapa que será desalocado.
 */
void map_destroy(Map *map);


/**
 * @brief Consulta a largura total (quantidade de colunas) do mapa atual.
 *
 * @param map Ponteiro constante para a estrutura do mapa.
 * @return O número de células no eixo X (largura). Retorna 0 se o mapa for inválido.
 */
size_t map_get_width(const Map *map);


/**
 * @brief Consulta a altura total (quantidade de linhas) do mapa atual.
 *
 * @param map Ponteiro constante para a estrutura do mapa.
 * @return O número de células no eixo Y (altura). Retorna 0 se o mapa for inválido.
 */
size_t map_get_height(const Map *map);


/**
 * @brief Obtém o tipo de uma célula numa dada coordenada.
 *
 * @param map Ponteiro constante para a estrutura do mapa.
 * @param position Estrutura de coordenada contendo as posições X e Y desejadas.
 * @return O valor correspondente do enum TileType. Se a coordenada estiver fora dos
 * limites do mapa, retorna TILE_BLOCKED.
 */
TileType map_get_tile_type(const Map *map, Coord position);


/**
 * @brief Checa se uma coordenada está contida dentro das dimensões físicas do mapa.
 *
 * @param map Ponteiro constante para a estrutura do mapa.
 * @param position Coordenada analisada.
 * @return true Se a posição respeitar os limites de largura e altura.
 * @return false Se a posição for negativa ou ultrapassar os limites da matriz.
 */
bool map_is_within_bounds(const Map *map, Coord position);


/**
 * @brief Checa se uma célula é permanentemente intransitável (obstáculo ou parede).
 *
 * @param map Ponteiro constante para a estrutura do mapa.
 * @param position Coordenada alvo da verificação.
 * @return true Se o tipo da célula for igual a TILE_BLOCKED.
 * @return false Se a célula for uma via transitável por veículos.
 */
bool map_is_blocked(const Map *map, Coord position);


/**
 * @brief Checa se uma célula está ocupada no momento por um veículo ou obstáculo físico.
 *
 * Esta verificação realiza uma leitura thread-safe com exclusão mútua local.
 * Células são consideradas ocupadas se houver um veículo nelas ou se a célula
 * for do tipo TILE_BLOCKED.
 *
 * @param map Ponteiro para a estrutura do mapa.
 * @param position Coordenada alvo de inspeção concorrente.
 * @return true Se a célula estiver indisponível para tráfego imediato.
 * @return false Se a célula estiver livre e apta a receber um veículo.
 */
bool map_is_occupied(Map *map, Coord position);


/**
 * @brief Realiza a transferência de um ocupante entre duas células.
 *
 * Usa mecanismos internos para prevenção de deadlocks. Realiza checagem do estado de
 * destino e, se livre, limpa a ocupação da célula antiga e ativa a ocupação na nova
 * célula de destino.
 *
 * @param map Ponteiro para o objeto do mapa.
 * @param from Coordenada de origem (posição atual do veículo).
 * @param to Coordenada de destino pretendida.
 * @return true Se a movimentação foi validada.
 * @return false Se a tentativa de movimentação é considerada inválida.
 */
bool map_transfer_occupant(Map *map, Coord from, Coord to);


/**
 * @brief Procura uma célula livre no mapa e a reserva.
 *
 * Varre a malha viária em busca de uma célula transitável e que não esteja ocupada.
 * Se encontrar, altera o estado da célula para ocupada e retorna a sua coordenada.
 * É projetada para ser usada exclusivamente na criação de um veículo.
 *
 * @param map Ponteiro para o objeto mutável do mapa.
 * @return A coordenada contendo a posição reservada.
 * Se o mapa estiver cheio ou inválido, retorna NULL_COORD.
 */
Coord map_reserve_spawn_point(Map *map);

#endif //URBAN_TRAFFIC_MAP_H