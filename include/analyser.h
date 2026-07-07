/**
 * @file analyser.h
 *
 * @date 2026-07-04
 */

#ifndef URBAN_TRAFFIC_ANALYSER_H
#define URBAN_TRAFFIC_ANALYSER_H

#include "map.h"

// Forward declaration
typedef struct Clock Clock;

typedef enum {
    REQUEST_EMPTY,
    REQUEST_PENDING,
    REQUEST_APPROVED,
    REQUEST_DENIED,
} RequestStatus;

typedef struct {
    Coord from;
    Coord to;
    RequestStatus status;
} MovementRequest;

typedef struct Analyser Analyser;

typedef struct {
    Analyser *analyser;
    Clock *clock;
    Map *map;
}AnalyserArgs;

Analyser *analyser_new(void);

void analyser_destroy(Analyser *analyser);

void *analyser_update(void *analyser_args);

void analyser_request(Analyser *analyser, int id, MovementRequest request);

MovementRequest *analyser_get_previous_requests(Analyser *analyser);

void analyser_swap_buffers(Analyser *analyser);

#endif //URBAN_TRAFFIC_ANALYSER_H
