#include <assert.h>
#include <string.h>

#include <splatc/rasterizer.h>
#include <splatc/linalg.h>
#include <splatc/camera.h>
#include <splatc/loader.h>

struct raster_ctx_t {
    gsmodel *model;
    vec2f tile_lower_bound;
    vec2f tile_upper_bound;

    vec4f *ndc_points;
    uint8_t *visible;
};


static void
frame_put_pixel(frame *f, size_t x, size_t y, float* c) {
    if (x < 0 || y < 0 || x >= f->width || y >= f->height) return;
    y = f->height - 1 - y;

    size_t coord = f->channels * f->width * y + f->channels * x;

    for (size_t i = 0; i < f->channels; ++i) {
        if (c) {
            f->pixels[coord + i] = 255 * c[i];
        }
        else {
            f->pixels[coord + i] = 255;
        }
    }
}

frame*
rasterizer_frame_create(size_t width, size_t height, size_t channels){
    uint8_t* pixels = calloc(width * height * channels, sizeof(uint8_t));
    float aspect = (float)width / height;

    frame *f = calloc(1, sizeof(frame));
    f->width = width;
    f->height = height;
    f->channels = channels;
    f->aspect = aspect;
    f->pixels = pixels;
    return f;
}

void
rasterizer_frame_clear(frame *f) {
    memset(f->pixels, 0, f->height * f->width * f->channels * sizeof(uint8_t));
}

raster_ctx*
rasterizer_context_create(gsmodel* model, vec2f tile_lower_bound, vec2f tile_upper_bound) {
    assert(model != NULL);
    raster_ctx *ctx = calloc(1, sizeof(raster_ctx));

    ctx->model = model;
    ctx->tile_lower_bound = tile_lower_bound;
    ctx->tile_upper_bound = tile_upper_bound;

    uint8_t *visible = calloc(model->n_points, sizeof(uint8_t));
    ctx->visible = visible;

    vec4f *ndc_points = calloc(model->n_points, sizeof(vec4f));
    ctx->ndc_points = ndc_points;

    return ctx;
}

void
rasterizer_preprocess(raster_ctx *ctx, camera *camera) {
    mat4 proj = camera_get_projection(camera);
    mat4 view = camera_get_view(camera);
    mat4 pv;
    matmul_mat(&view, &proj, &pv);

    for (int i = 0; i < ctx->model->n_points; ++i) {
        vec4f vertex = (vec4f){
            ctx->model->positions[i].x,
            ctx->model->positions[i].y,
            ctx->model->positions[i].z,
            1.f
        };
        // vertex = matmul_v4(&view, &vertex);
        vertex = matmul_v4(&pv, &vertex);

        float rw = 1.f / (vertex.w + 1e-5f);
        ctx->ndc_points[i].v[0] = (vertex.x * rw);
        ctx->ndc_points[i].v[1] = (vertex.y * rw);
        ctx->ndc_points[i].v[2] = (vertex.z * rw);
    }
}

void
rasterizer_mark_visible(raster_ctx *ctx, camera *camera) {
    for (size_t i = 0; i < ctx->model->n_points; ++i) {
        ctx->visible[i] = !(ctx->ndc_points[i].x < ctx->tile_lower_bound.x || 
                          ctx->ndc_points[i].x > ctx->tile_upper_bound.x ||
                          ctx->ndc_points[i].y < ctx->tile_lower_bound.y || 
                          ctx->ndc_points[i].y > ctx->tile_upper_bound.y ||
                          ctx->ndc_points[i].z < -1.f ||
                          ctx->ndc_points[i].z > 1.f);
    }
}

void
rasterizer_render(raster_ctx *ctx, camera *camera, frame *frame) {

    for (size_t i = 0; i < ctx->model->n_points; ++i) {
        if (!ctx->visible[i]) continue;

        size_t x = frame->width * (0.5f + ctx->ndc_points[i].x * 0.5f);
        size_t y = frame->height * (0.5f + ctx->ndc_points[i].y * 0.5f);
        frame_put_pixel(frame, x, y, ctx->model->colors[i].v);
    }
}


void 
rasterizer_frame_destroy(frame *frame) {
    if (frame) {
        free(frame->pixels);
    }
    free(frame);
}

void
rasterizer_context_destroy(raster_ctx *ctx) {
    if (ctx) {
        free(ctx->visible);
        free(ctx->ndc_points);
    }
    free(ctx);
}
