/**
 * @file simulation.c
 *
 * @date 2026-07-05
 */
#include <stdlib.h>
#include <pthread.h>

#include "simulation.h"
#include "analyser.h"
#include "render.h"
#include "map.h"
#include "vehicle.h"
#include "clock.h"
#include "debug.h"
#include "traffic_light.h"

#define TILE_WIDTH      15
#define TILE_HEIGHT     4

#define MAP_PATH        "res/map.txt"
#define AMBULANCE_PATH  "res/ambulance.txt"
#define CAR_FAST_PATH   "res/car-fast.txt"
#define CAR_MEDIUM_PATH "res/car-medium.txt"
#define CAR_SLOW_PATH   "res/car-slow.txt"

#define PATH_LIGHT_RED      "res/light-red.txt"
#define PATH_LIGHT_GREEN    "res/light-green.txt"
#define PATH_LIGHT_YELLOW   "res/light-yellow.txt"

#define TILE_ROAD_PATH    "res/tile-road.txt"
#define TILE_BLOCKED_PATH "res/tile-blocked.txt"

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

Simulation *simulation_new(void) {
    DEBUG_INIT("out/debug.log");

    Simulation *simulation = malloc(sizeof(Simulation));
    if (!simulation) {
        LOG("Error: failed to allocate memory for 'simulation'.");
        return NULL;
    }

    const size_t total_workers = VEHICLE_COUNT + 3; /* analyser, render, traffic_light*/

    simulation->analyser = analyser_new();
    simulation->clock = clock_new(total_workers);
    simulation->map = map_new(MAP_PATH);
    simulation->render = render_new(simulation->map, TILE_WIDTH, TILE_HEIGHT);

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        simulation->vehicles[i] = vehicle_new(simulation->map, i);
    }

    render_load_vehicle_asset_all_directions(simulation->render, AMBULANCE, AMBULANCE_PATH);
    render_load_vehicle_asset_all_directions(simulation->render, CAR_FAST, CAR_FAST_PATH);
    render_load_vehicle_asset_all_directions(simulation->render, CAR_SLOW, CAR_SLOW_PATH);
    render_load_vehicle_asset_all_directions(simulation->render, CAR_MEDIUM, CAR_MEDIUM_PATH);

    const TileType roads[6] = {
        TILE_ROAD,
        TILE_ROAD_UP,
        TILE_ROAD_DOWN,
        TILE_ROAD_LEFT,
        TILE_ROAD_RIGHT,
        TILE_WAIT,
    };

    render_load_tile_asset_multi(simulation->render, TILE_ROAD_PATH, roads, 6);
    render_load_tile_asset(simulation->render, TILE_BLOCKED, TILE_BLOCKED_PATH);

    render_load_traffic_light_asset(simulation->render, TRAFFIC_LIGHT_RED, PATH_LIGHT_RED);
    render_load_traffic_light_asset(simulation->render, TRAFFIC_LIGHT_GREEN, PATH_LIGHT_GREEN);
    render_load_traffic_light_asset(simulation->render, TRAFFIC_LIGHT_YELLOW, PATH_LIGHT_YELLOW);


    /* Inicialização Hardcoded para garantir a lógica dos semáforos. Implementar um algorítmo que
     * mapeasse adequadamente as interseções e pontos de espera seria complexo e levaria tempo */

    WaitPoint intersection1[2] = {
        { .position = {5, 0 }, .direction = DIRECTION_LEFT },
        { .position = {4, 1 }, .direction = DIRECTION_UP },
    };

    WaitPoint intersection2[2] = {
        { .position = {0, 2 }, .direction = DIRECTION_DOWN },
        { .position = {1, 3 }, .direction = DIRECTION_LEFT },
    };

    WaitPoint intersection3[4] = {
        { .position = {3, 2}, .direction = DIRECTION_DOWN },
        { .position = {5, 3 }, .direction = DIRECTION_LEFT },
        { .position = {2, 4 }, .direction = DIRECTION_RIGHT },
        { .position = {4, 5 }, .direction = DIRECTION_UP },
    };

    WaitPoint intersection4[2] = {
        { .position = {6, 4 }, .direction = DIRECTION_LEFT },
        { .position = {7, 5 }, .direction = DIRECTION_UP },
    };

    WaitPoint intersection5[2] = {
        { .position = {0, 6 }, .direction = DIRECTION_DOWN },
        { .position = {1, 7 }, .direction = DIRECTION_LEFT },
    };

    WaitPoint intersection6[4] = {
        { .position = {3, 6}, .direction = DIRECTION_DOWN },
        { .position = {5, 7}, .direction = DIRECTION_LEFT },
        { .position = {2, 8}, .direction = DIRECTION_RIGHT },
        { .position = {4, 9}, .direction = DIRECTION_UP },
    };

    WaitPoint intersection7[2] = {
        { .position = {6, 8}, .direction = DIRECTION_RIGHT },
        { .position = {7, 9}, .direction = DIRECTION_UP }
    };

    WaitPoint intersection8[4] = {
        { .position = {3, 10}, .direction = DIRECTION_DOWN },
        { .position = {2, 11}, .direction = DIRECTION_LEFT }
    };

    Intersection intersections[8] = {
        { .wait_points = intersection1, .count = 2 },
        { .wait_points = intersection2, .count = 2 },
        { .wait_points = intersection3, .count = 4 },
        { .wait_points = intersection4, .count = 2 },
        { .wait_points = intersection5, .count = 2 },
        { .wait_points = intersection6, .count = 4 },
        { .wait_points = intersection7, .count = 2 },
        { .wait_points = intersection8, .count = 2 },
    };

    simulation->traffic_light = traffic_light_new(simulation->map, 8, intersections);

    return simulation;
}

void simulation_run(Simulation *simulation) {
    if (!simulation) {
        LOG("Error: parameter 'simulation' is NULL.");
        return;
    }

    if (!simulation->analyser) {
        LOG("Error: field 'simulation->analyser' is NULL.");
        return;
    }

    if (!simulation->clock) {
        LOG("Error: field 'simulation->clock' is NULL.");
        return;
    }

    if (!simulation->map) {
        LOG("Error: field 'simulation->map' is NULL.");
        return;
    }

    if (!simulation->render) {
        LOG("Error: field 'simulation->render' is NULL.");
        return;
    }

    if (!simulation->traffic_light) {
        LOG("Error: field 'simulation->traffic_light' is NULL.");
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
        .render = simulation->render,
        .clock = simulation->clock,
        .map = simulation->map,
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

    TRY(pthread_create(&simulation->thread_clock, NULL, clock_update, &clock_args));
    TRY(pthread_create(&simulation->thread_traffic_light, NULL, traffic_light_update, &traffic_light_args));
    TRY(pthread_create(&simulation->thread_analyser, NULL, analyser_update, &analyser_args));
    TRY(pthread_create(&simulation->thread_render, NULL, render_update, &render_args));

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        TRY(pthread_create(&simulation->thread_vehicles[i], NULL, vehicle_update, &vehicle_args[i]));
    }


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