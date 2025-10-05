#ifndef RASTERIZER_H
#define RASTERIZER_H

#include <stdint.h>
#include <stdlib.h>

#include "linalg.h"
#include "camera.h"
#include "loader.h"

typedef struct raster_ctx_t raster_ctx;

typedef struct {
    size_t width;
    size_t height;
    size_t channels;
    float aspect;
    uint8_t* pixels;
} frame;

frame*
rasterizer_frame_create(size_t width, size_t height, size_t channels);

void
rasterizer_frame_clear(frame *frame);

raster_ctx*
rasterizer_context_create(gsmodel* model, vec2f tile_lower_bound, vec2f tile_upper_bound);

void
rasterizer_preprocess(raster_ctx *ctx, camera *camera);

void
rasterizer_mark_visible(raster_ctx *ctx, camera *camera);

void
rasterizer_render(raster_ctx *ctx, camera *camera, frame *frame);


void 
rasterizer_frame_destroy(frame *frame);

void
rasterizer_context_destroy(raster_ctx *ctx);


#endif
