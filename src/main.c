#include <stdio.h>

#include "map.h"
#include "clock.h"
#include "vehicle.h"

#include <pthread.h>

int main() {
    pthread_t thread_clock;
    pthread_t thread_vehicle[VEHICLE_COUNT];

    Map *map = map_new("res/map.txt");
    Clock *clock = clock_new();
    Vehicle *vehicle = vehicle_new(map,0);

    VehicleArgs vehicle_args[VEHICLE_COUNT];

    pthread_create(&thread_clock,NULL,&clock_update, clock);

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        vehicle_args[i].vehicle = vehicle_new(map, i);
        vehicle_args[i].map = map;
        vehicle_args[i].clock = clock;
    }

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        pthread_create(&thread_vehicle[i], NULL, vehicle_update, &vehicle_args[i]);
    }

    for (int i = 0; i < VEHICLE_COUNT; i++) {
        pthread_join(thread_vehicle[i], NULL);
        vehicle_destroy(vehicle_args[i].vehicle);
    }

    pthread_join(thread_clock, NULL);

    map_destroy(map);
    clock_destroy(clock);

    return 0;
}