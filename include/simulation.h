/**
 * @file simulation.h
 *
 * @date 2026-07-05
 */
#ifndef URBAN_TRAFFIC_SIMULATION_H
#define URBAN_TRAFFIC_SIMULATION_H

typedef struct Simulation Simulation;

Simulation *simulation_new(void);

void simulation_run(Simulation *simulation);

void simulation_destroy(Simulation *simulation);

#endif //URBAN_TRAFFIC_SIMULATION_H