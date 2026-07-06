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

#define TILE_WIDTH      1
#define TILE_HEIGHT     1

#define MAP_PATH        "res/map.txt"
#define AMBULANCE_PATH  "res/ambulance.txt"
#define CAR_FAST_PATH   "res/car-fast.txt"
#define CAR_SLOW_PATH   "res/car-slow.txt"

#define TILE_ROAD_PATH    "res/tile-road.txt"
#define TILE_BLOCKED_PATH "res/tile-block.txt"

struct Simulation {
    Map      *map;
    Clock    *clock;
    Analyser *analyser;
    Render   *render;
    Vehicle  *vehicles[VEHICLE_COUNT];

    pthread_t thread_clock;
    pthread_t thread_analyser;
    pthread_t thread_render;
    pthread_t thread_vehicles[VEHICLE_COUNT];
};

Simulation *simulation_new(void) {

    Clock *clock = clock_new();
    Analyser *analyser = analyser_new();
    Map *map = map_new(MAP_PATH);
    Render *render = render_new(map, TILE_WIDTH, TILE_HEIGHT);

    render_load_vehicle_asset_all_directions(render, AMBULANCE, AMBULANCE_PATH);
    render_load_vehicle_asset_all_directions(render, CAR_FAST, CAR_FAST_PATH);
    render_load_vehicle_asset_all_directions(render, CAR_SLOW, CAR_SLOW_PATH);
    render_load_vehicle_asset_all_directions(render, CAR_MEDIUM, CAR_SLOW_PATH);


    const TileType roads[6] = {
        TILE_ROAD,
        TILE_ROAD_UP,
        TILE_ROAD_DOWN,
        TILE_ROAD_LEFT,
        TILE_ROAD_RIGHT,
        TILE_WAIT,
    };

    render_load_tile_asset_multi(render, TILE_ROAD_PATH, roads, 6);
    render_load_tile_asset(render, TILE_BLOCKED, TILE_BLOCKED_PATH);

    Simulation *simulation = malloc(sizeof(Simulation));

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        simulation->vehicles[i] = vehicle_new(map, i);
    }

    simulation->map = map;
    simulation->clock = clock;
    simulation->analyser = analyser;
    simulation->render = render;

    return simulation;
}

void simulation_run(Simulation *simulation) {
    VehicleArgs vehicle_args[VEHICLE_COUNT];
    for (int i = 0; i < VEHICLE_COUNT; i++) {
        vehicle_args[i].map = simulation->map;
        vehicle_args[i].clock = simulation->clock;
        vehicle_args[i].vehicle = simulation->vehicles[i];
    }

    RenderArgs render_args = {
        .analyser = simulation->analyser,
        .render = simulation->render,
        .clock = simulation->clock,
        .map = simulation->map,
        .vehicles = simulation->vehicles,
        .vehicle_count = VEHICLE_COUNT,
    };

    AnalyserArgs analyser_args = {
        .analyser = simulation->analyser,
        .clock = simulation->clock,
        .map = simulation->map,
    };

    ClockArgs clock_args = {
        .clock = simulation->clock,
    };

    TRY(pthread_create(&simulation->thread_clock, NULL, clock_update, &clock_args));
    TRY(pthread_create(&simulation->thread_analyser, NULL, analyser_update, &analyser_args));
    TRY(pthread_create(&simulation->thread_render, NULL, render_update, &render_args));

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        TRY(pthread_create(&simulation->thread_vehicles[i], NULL, vehicle_update, &vehicle_args[i]));
    }


    TRY(pthread_join(simulation->thread_clock, NULL));
    TRY(pthread_join(simulation->thread_analyser, NULL));
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

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        vehicle_destroy(simulation->vehicles[i]);
    }

    free(simulation);
}