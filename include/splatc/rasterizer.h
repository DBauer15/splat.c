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
    float aspect;
    vec3f *pixels;
} frame;

frame*
rasterizer_frame_create(size_t width, size_t height);

void
rasterizer_frame_clear(frame *frame);

raster_ctx*
rasterizer_context_create(gsmodel *model, frame *frame, vec2u tile_size);

void
rasterizer_preprocess(raster_ctx *ctx, camera *camera, frame *frame);

void
rasterizer_render(raster_ctx *ctx, camera *camera, frame *frame);

void 
rasterizer_frame_destroy(frame *frame);

void
rasterizer_context_destroy(raster_ctx *ctx);


#endif
