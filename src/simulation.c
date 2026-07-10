/**
 * @file simulation.c
 *
 * @date 2026-07-05
 */
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "simulation.h"

#include "analyser.h"
#include "render.h"
#include "map.h"
#include "vehicle.h"
#include "clock.h"
#include "debug.h"
#include "traffic_light.h"


#define PATH_MAP_DATA "res/data/map-data.txt"

#define PATH_LIGHT_RED    "res/traffic-light/light-red.txt"
#define PATH_LIGHT_GREEN  "res/traffic-light/light-green.txt"
#define PATH_LIGHT_YELLOW "res/traffic-light/light-yellow.txt"

#define PATH_TILE_ROAD    "res/tile/tile-road.txt"
#define PATH_TILE_BLOCKED "res/tile/tile-blocked.txt"

#define PATH_AMBULANCE_LEFT   "res/vehicle/ambulance-left.txt"
#define PATH_AMBULANCE_RIGHT  "res/vehicle/ambulance-right.txt"
#define PATH_CAR_FAST_LEFT    "res/vehicle/car-fast-left.txt"
#define PATH_CAR_FAST_RIGHT   "res/vehicle/car-fast-right.txt"
#define PATH_CAR_MEDIUM_LEFT  "res/vehicle/car-medium-left.txt"
#define PATH_CAR_MEDIUM_RIGHT "res/vehicle/car-medium-right.txt"
#define PATH_CAR_SLOW_LEFT    "res/vehicle/car-slow-left.txt"
#define PATH_CAR_SLOW_RIGHT   "res/vehicle/car-slow-right.txt"

struct Simulation {
    Analyser *analyser;
    Clock *clock;
    Map *map;
    Render *render;
    TrafficLight *traffic_light;
    Vehicle *vehicles[VEHICLE_COUNT];

    pthread_t thread_analyser;
    pthread_t thread_clock;
    pthread_t thread_render;
    pthread_t thread_traffic_light;
    pthread_t thread_vehicles[VEHICLE_COUNT];
};

/* ============================================================================
 * Funções Auxiliares Internas
 * ============================================================================ */


/**
 * @internal
 * @brief Exibe e processa o menu CLI antes da simulação iniciar.
 */
static void prompt_cli_configuration(size_t *out_ticks, char *out_mode) {
    char buffer[256];

    printf("=== Urban Traffic Simulator ===\n\n");
    printf("Use default settings? [Y/n] ");

    if (!fgets(buffer, sizeof(buffer), stdin)) return;

    /* Se o usuário digitar 'n' ou 'N', entramos no modo de configuração manual */
    if (buffer[0] == 'n' || buffer[0] == 'N') {
        printf("\n> Total ticks [%zu]: ", *out_ticks);
        if (fgets(buffer, sizeof(buffer), stdin) && buffer[0] != '\n') {
            long ticks = strtol(buffer, NULL, 10);
            if (ticks > 0) {
                *out_ticks = (size_t)ticks;
            }
        }

        printf("\n- A: ASCII arts (16x4 tiles)\n- C: Single characters (1x1 tiles)\n> Rendering Mode [%c/c]: ", *out_mode);
        if (fgets(buffer, sizeof(buffer), stdin) && buffer[0] != '\n') {
            char mode = (char)toupper(buffer[0]);
            if (mode == 'A' || mode == 'C') {
                *out_mode = mode;
            }
        }
        printf("\n");
    }
}

/**
 * @internal
 * @brief Carrega os assets de um veículo agrupando direções que compartilham a mesma arte.
 */
static void load_vehicle_assets(Render *render, VehicleType type, const char *path_left, const char *path_right) {
    render_load_vehicle_asset(render, type, DIRECTION_LEFT,  path_left);
    render_load_vehicle_asset(render, type, DIRECTION_DOWN,  path_left);
    render_load_vehicle_asset(render, type, DIRECTION_RIGHT, path_right);
    render_load_vehicle_asset(render, type, DIRECTION_UP,    path_right);
}

/* ============================================================================
 * API Pública
 * ============================================================================ */

Simulation *simulation_new(void) {
    srand(time(NULL));
    DEBUG_INIT("out/debug.log");

    Simulation *simulation = malloc(sizeof(Simulation));
    if (!simulation) {
        LOG("Error: failed to allocate memory for 'simulation'.");
        return NULL;
    }

    /* Valores padrão */
    size_t total_ticks = 100;
    char render_mode = 'A';

    /* Invoca a interface de linha de comando para sobrescrever os padrões se desejado */
    prompt_cli_configuration(&total_ticks, &render_mode);

    /* Dimensões da célula dependem do modo de renderização escolhido */
    const size_t tile_width  = (render_mode == 'A') ? 16 : 3;
    const size_t tile_height = (render_mode == 'A') ? 4  : 1;
    const size_t total_workers = VEHICLE_COUNT + 3; /* analyser, render, traffic_light */

    simulation->analyser = analyser_new();
    simulation->clock = clock_new(total_workers, total_ticks);
    simulation->map = map_new(PATH_MAP_DATA);
    simulation->render = render_new(simulation->map, tile_width, tile_height);

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        simulation->vehicles[i] = vehicle_new(simulation->map, i);
    }

    const TileType roads[9] = {
        TILE_ROAD_UP, TILE_ROAD_DOWN, TILE_ROAD_LEFT, TILE_ROAD_RIGHT,
        TILE_TURN_UP, TILE_TURN_DOWN, TILE_TURN_LEFT, TILE_TURN_RIGHT,
        TILE_ROAD
    };

    /* Carregamento condicional de assets baseado na escolha do usuário */
    if (render_mode == 'A') {
        load_vehicle_assets(simulation->render, AMBULANCE,  PATH_AMBULANCE_LEFT,  PATH_AMBULANCE_RIGHT);
        load_vehicle_assets(simulation->render, CAR_FAST,   PATH_CAR_FAST_LEFT,   PATH_CAR_FAST_RIGHT);
        load_vehicle_assets(simulation->render, CAR_MEDIUM, PATH_CAR_MEDIUM_LEFT, PATH_CAR_MEDIUM_RIGHT);
        load_vehicle_assets(simulation->render, CAR_SLOW,   PATH_CAR_SLOW_LEFT,   PATH_CAR_SLOW_RIGHT);

        render_load_tile_asset_multi(simulation->render, PATH_TILE_ROAD, roads, 9);
        render_load_tile_asset(simulation->render, TILE_BLOCKED, PATH_TILE_BLOCKED);

        render_load_traffic_light_asset(simulation->render, TRAFFIC_LIGHT_RED,    PATH_LIGHT_RED);
        render_load_traffic_light_asset(simulation->render, TRAFFIC_LIGHT_GREEN,  PATH_LIGHT_GREEN);
        render_load_traffic_light_asset(simulation->render, TRAFFIC_LIGHT_YELLOW, PATH_LIGHT_YELLOW);
    }
    else { // Modo 'C' - Single Characters
        render_load_vehicle_asset_all_directions_from_string(simulation->render, AMBULANCE, " A ");
        render_load_vehicle_asset_all_directions_from_string(simulation->render, CAR_FAST,  " F ");
        render_load_vehicle_asset_all_directions_from_string(simulation->render, CAR_MEDIUM," M ");
        render_load_vehicle_asset_all_directions_from_string(simulation->render, CAR_SLOW,  " S ");

        render_load_tile_asset_multi_from_string(simulation->render, " . ", roads, 9);
        render_load_tile_asset_from_string(simulation->render, TILE_BLOCKED, " # ");

        render_load_traffic_light_asset_from_string(simulation->render, TRAFFIC_LIGHT_RED,    " r ");
        render_load_traffic_light_asset_from_string(simulation->render, TRAFFIC_LIGHT_GREEN,  " g ");
        render_load_traffic_light_asset_from_string(simulation->render, TRAFFIC_LIGHT_YELLOW, " y ");
    }

    /* Mapeamento de Interseções via Compound Literals */
    Intersection intersections[8] = {
        {
            .count = 2,
            .wait_points = (WaitPoint[]) {
                {{5, 0}, DIRECTION_LEFT},
                {{4, 1}, DIRECTION_UP}
            }
        },
        {
            .count = 2,
            .wait_points = (WaitPoint[]) {
                {{0, 2}, DIRECTION_DOWN},
                {{1, 3}, DIRECTION_LEFT}
            }
        },
        {
            .count = 4,
            .wait_points = (WaitPoint[]) {
                {{3, 2}, DIRECTION_DOWN},
                {{5, 3}, DIRECTION_LEFT},
                {{2, 4}, DIRECTION_RIGHT},
                {{4, 5}, DIRECTION_UP}
            }
        },
        { .count = 2,
            .wait_points = (WaitPoint[]) {
                {{6, 4}, DIRECTION_LEFT},
                {{7, 5}, DIRECTION_UP} }
        },
        { .count = 2,
            .wait_points = (WaitPoint[]) {
                {{0, 6}, DIRECTION_DOWN},
                {{1, 7}, DIRECTION_LEFT}
            }
        },
        {
            .count = 4,
            .wait_points = (WaitPoint[]) {
                {{3, 6}, DIRECTION_DOWN},
                {{5, 7}, DIRECTION_LEFT},
                {{2, 8}, DIRECTION_RIGHT},
                {{4, 9}, DIRECTION_UP}
            }
        },
        {
            .count = 2,
            .wait_points = (WaitPoint[]) {
                {{6, 8}, DIRECTION_RIGHT},
                {{7, 9}, DIRECTION_UP}
            }
        },
        {
            .count = 2,
            .wait_points = (WaitPoint[]) {
                {{3, 10}, DIRECTION_DOWN},
                {{2, 11}, DIRECTION_LEFT}
            }
        },
    };

    simulation->traffic_light = traffic_light_new(simulation->map, 8, intersections);

    return simulation;
}

void simulation_run(Simulation *simulation) {
    if (!simulation || !simulation->analyser || !simulation->clock ||
        !simulation->map || !simulation->render || !simulation->traffic_light) {
        LOG("Error: critical simulation components are missing.");
        return;
    }

    SharedVehicleArgs shared = {
        .analyser = simulation->analyser,
        .clock = simulation->clock,
        .map = simulation->map,
        .traffic_light = simulation->traffic_light,
    };

    VehicleArgs vehicle_args[VEHICLE_COUNT];
    for (int i = 0; i < VEHICLE_COUNT; i++) {
        vehicle_args[i].shared = &shared;
        vehicle_args[i].vehicle = simulation->vehicles[i];
    }

    RenderArgs render_args = {
        .analyser = simulation->analyser,
        .clock = simulation->clock,
        .map = simulation->map,
        .render = simulation->render,
        .vehicles = simulation->vehicles,
        .vehicle_count = VEHICLE_COUNT,
        .traffic_light = simulation->traffic_light,
    };

    AnalyserArgs analyser_args = {
        .analyser = simulation->analyser,
        .clock = simulation->clock,
        .map = simulation->map,
    };

    ClockArgs clock_args = {
        .clock = simulation->clock,
        .analyser = simulation->analyser,
        .traffic_light = simulation->traffic_light,
    };

    TrafficLightArgs traffic_light_args = {
        .traffic_light = simulation->traffic_light,
        .clock = simulation->clock,
        .map = simulation->map,
    };

    system("clear");

    /* Disparo das Threads de Controle */
    TRY(pthread_create(&simulation->thread_clock, NULL, clock_update, &clock_args));
    TRY(pthread_create(&simulation->thread_traffic_light, NULL, traffic_light_update, &traffic_light_args));
    TRY(pthread_create(&simulation->thread_analyser, NULL, analyser_update, &analyser_args));
    TRY(pthread_create(&simulation->thread_render, NULL, render_update, &render_args));

    /* Disparo das Threads dos Veículos */
    for (int i = 0; i < VEHICLE_COUNT; i++) {
        TRY(pthread_create(&simulation->thread_vehicles[i], NULL, vehicle_update, &vehicle_args[i]));
    }

    /* Sincronização Final */
    TRY(pthread_join(simulation->thread_clock, NULL));
    TRY(pthread_join(simulation->thread_analyser, NULL));
    TRY(pthread_join(simulation->thread_traffic_light, NULL));
    TRY(pthread_join(simulation->thread_render, NULL));

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        TRY(pthread_join(simulation->thread_vehicles[i], NULL));
    }
}

void simulation_destroy(Simulation *simulation) {
    if (!simulation) return;

    clock_destroy(simulation->clock);
    analyser_destroy(simulation->analyser);
    map_destroy(simulation->map);
    render_destroy(simulation->render);
    traffic_light_destroy(simulation->traffic_light);

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        vehicle_destroy(simulation->vehicles[i]);
    }

    free(simulation);
    DEBUG_CLOSE;
}