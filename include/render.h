/**
 * @file render.h
 * @brief
 *
 * @date 2026-06-11
 */
#ifndef URBAN_TRAFFIC_RENDER_H
#define URBAN_TRAFFIC_RENDER_H

#include <stddef.h>
#include "map.h"

typedef struct Render Render;

typedef struct {
    Render *render;
} RenderArgs;

void render_config(size_t tile_width, size_t tile_height);

void render_load_asset(TileType tile_type, const char *file_name);

Render *render_new(void);

void render_destroy(Render *render);

void *render_update(void *render);

#endif //URBAN_TRAFFIC_RENDER_H