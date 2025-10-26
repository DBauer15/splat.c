#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <splatc/rasterizer.h>
#include <splatc/linalg.h>
#include <splatc/camera.h>
#include <splatc/loader.h>
#include <splatc/threadpool.h>

#define RASTERIZER_NUM_THREADS 16
#define RASTERIZER_TILE_BATCH_SIZE 8

#define LOG2E 1.4426950408889634f

struct render_kernel_args;

typedef struct {
    vec4f view;
    vec2f frame;
    size_t idx;
} transformed_point;

struct raster_ctx_t {
    /* model */
    gsmodel *model;

    /* rendering */
    transformed_point *trans_points;
    vec4f *ndc_points;
    size_t *visible;
    size_t *n_visible;
    float* radii;
    vec3f *inv_cov2d;
    vec2u tile_size;
    vec2u n_tiles;
    vec3f *throughputs;

    /* threading */
    tpool *tpool;
    size_t n_tile_batches;
    struct render_kernel_args *rargs;
};

typedef struct render_kernel_args {
    raster_ctx *ctx;
    camera *camera;
    frame *frame;
    size_t tile;
    size_t tile_start;
    size_t tile_end;
} render_batch_args;

static vec2f
frame_ndc_to_screen(vec4f p, frame *frame) {
   return (vec2f){ (0.5f * p.x + 0.5f) * frame->width, (0.5f * p.y + 0.5f) * frame->height };
}

static vec3f
compute_cov2d(const vec4f *mu_view, const mat4 *view, mat3 *cov_3d, float focal_x, float focal_y, float tan_fovx, float tan_fovy) {
    vec3f t = { 
        mu_view->x,
        mu_view->y,
        mu_view->z,
    };

	const float limx = 1.3f * tan_fovx;
	const float limy = 1.3f * tan_fovy;
	const float txtz = t.x / t.z;
	const float tytz = t.y / t.z;
	t.x = fminf(limx, fmaxf(-limx, txtz)) * t.z;
	t.y = fminf(limy, fmaxf(-limy, tytz)) * t.z;

    mat3 J = { 
        focal_x / t.z,  0.f,            -(focal_x * t.x) / (t.z * t.z),
        0.f,            focal_y / t.z,  -(focal_y * t.y) / (t.z * t.z),
        0.f,            0.f,            0.f
    };

    mat3 W = { 
    view->vv[0][0], view->vv[1][0], view->vv[2][0],
    view->vv[0][1], view->vv[1][1], view->vv[2][1],
    view->vv[0][2], view->vv[1][2], view->vv[2][2] };

	mat3 T = matmul3(J, W);
    mat3 Vrk = *cov_3d;

    mat3 cov = matmul3(T, matmul3(Vrk, transpose3(T)));

	return (vec3f){ cov.vv[0][0], cov.vv[0][1], cov.vv[1][1] };
}

static int
comp_points(const void *a, const void *b) {
    float z0 = ((transformed_point*)b)->view.z;
    float z1 = ((transformed_point*)a)->view.z;
    return (z0 < z1) - (z0 > z1);
}

frame*
rasterizer_frame_create(size_t width, size_t height){
    vec3f* pixels = calloc(width * height, sizeof(vec3f));
    float aspect = (float)width / height;

    frame *f = calloc(1, sizeof(frame));
    f->width = width;
    f->height = height;
    f->aspect = aspect;
    f->pixels = pixels;
    return f;
}

void
rasterizer_frame_clear(frame *f) {
    for (size_t i = 0; i < f->width * f->height; ++i) {
        f->pixels[i].x = 0.f;
        f->pixels[i].y = 0.f;
        f->pixels[i].z = 0.f;
    }
    // memset(f->pixels, 0, f->height * f->width * f->channels * sizeof(float));
}

raster_ctx*
rasterizer_context_create(gsmodel* model, frame *frame, vec2u tile_size) {
    assert(model != NULL);

    raster_ctx *ctx = calloc(1, sizeof(raster_ctx));

    ctx->model = model;

    ctx->n_tiles = (vec2u){ 
        (frame->width + tile_size.x-1) / tile_size.x, 
        (frame->height + tile_size.y-1) / tile_size.y 
    };
    ctx->tile_size = tile_size;

    size_t *visible = calloc(model->n_points * ctx->n_tiles.x * ctx->n_tiles.y, sizeof(size_t));
    ctx->visible = visible;

    size_t *n_visible = calloc(ctx->n_tiles.x * ctx->n_tiles.y, sizeof(size_t));
    ctx->n_visible = n_visible;

    vec4f *ndc_points = calloc(model->n_points, sizeof(vec4f));
    ctx->ndc_points = ndc_points;

    transformed_point *trans_points = calloc(model->n_points, sizeof(transformed_point));
    ctx->trans_points = trans_points;

    float *radii = calloc(model->n_points, sizeof(float));
    ctx->radii = radii;

    vec3f *inv_cov2d = calloc(model->n_points, sizeof(vec3f));
    ctx->inv_cov2d = inv_cov2d;

    vec3f *throughputs = calloc(frame->height * frame->width, sizeof(vec3f));
    ctx->throughputs = throughputs;

    tpool *tpool = tpool_create(RASTERIZER_NUM_THREADS);
    ctx->tpool = tpool;

    ctx->n_tile_batches = ((ctx->n_tiles.x * ctx->n_tiles.y) + RASTERIZER_TILE_BATCH_SIZE-1) / RASTERIZER_TILE_BATCH_SIZE;
    // render_batch_args *rargs = calloc(ctx->n_tile_batches, sizeof(render_batch_args));
    render_batch_args *rargs = calloc(ctx->n_tiles.x * ctx->n_tiles.y, sizeof(render_batch_args));
    ctx->rargs = rargs;

    return ctx;

}

static size_t
rasterizer_get_n_tiles(raster_ctx *ctx) {
    return ctx->n_tiles.x * ctx->n_tiles.y;
}

void
rasterizer_preprocess(raster_ctx *ctx, camera *camera, frame *frame) {

    memset(ctx->n_visible, 0, ctx->n_tiles.x * ctx->n_tiles.y * sizeof(size_t));

    mat4 proj = camera_get_projection(camera);
    mat4 view = camera_get_view(camera);

    float tan_fovy = tanf(camera->fovy * 0.5f);
    float tan_fovx = tanf(camera->fovy * 0.5f) * camera->aspect; // float focal_y = frame->height / (2.0f * tan_fovy);
    float focal_y = (frame->height * 0.5f) / tan_fovy;
	float focal_x = (frame->width * 0.5f) / tan_fovx;

    size_t n_valid_points = 0;
    for (int i = 0; i < ctx->model->n_points; ++i) {
        vec4f vertex = (vec4f){
            ctx->model->positions[i].x,
            ctx->model->positions[i].y,
            ctx->model->positions[i].z,
            1.f
        };
        vec4f vview = matmulv4(view, vertex);
        if (vview.z < 0.f)
            continue;
        vec4f vproj = matmulv4(proj, vview);

        float rw = 1.f / (vproj.w + 1e-5f);
        ctx->ndc_points[i].v[0] = (vproj.x * rw);
        ctx->ndc_points[i].v[1] = (vproj.y * rw);
        ctx->ndc_points[i].v[2] = (vproj.z * rw);

        if (
            ctx->ndc_points[i].x < -1.f || ctx->ndc_points[i].x > 1.f ||
            ctx->ndc_points[i].y < -1.f || ctx->ndc_points[i].y > 1.f ||
            ctx->ndc_points[i].z < -1.f || ctx->ndc_points[i].z > 1.f
        ) {
            continue;
        }

        ctx->trans_points[n_valid_points].view = vview;
        ctx->trans_points[n_valid_points].frame = frame_ndc_to_screen(ctx->ndc_points[i], frame);
        ctx->trans_points[n_valid_points].idx = i;

        n_valid_points += 1;
    }

    /* sort valid points */
    qsort(ctx->trans_points, n_valid_points, sizeof(transformed_point), comp_points);
        
    for (int i = 0; i < n_valid_points; ++i) {
        size_t idx = ctx->trans_points[i].idx;
        vec4f vview = ctx->trans_points[i].view;
        vec3f cov = compute_cov2d(&vview, &view, &ctx->model->cov3d[idx], focal_x, focal_y, tan_fovx, tan_fovy);

        const float h_var = 0.3f;
        const float det_cov = cov.x * cov.z - cov.y * cov.y;
        cov.x += h_var;
        cov.z += h_var;
        const float det_cov_plus_h_cov = cov.x * cov.z - cov.y * cov.y;
        const float det = det_cov_plus_h_cov;

        if (det == 0.0f)
            continue;

        float det_inv = 1.f / det;
        vec3f inv_cov2d = { cov.z * det_inv, -cov.y * det_inv, cov.x * det_inv };

        float mid = 0.5f * (cov.x + cov.z);
        float lambda1 = mid + sqrtf(fmaxf(0.1f, mid * mid - det));
        float lambda2 = mid - sqrtf(fmaxf(0.1f, mid * mid - det));
        float radius = ceilf(3.f * sqrtf(fmaxf(lambda1, lambda2)));
        if (radius < 1.f)
            continue;
        vec2f point_screen = ctx->trans_points[i].frame;
        vec2u rect_min, rect_max;
        rect_min.x = fmaxf(floor(point_screen.x - radius), 0);
        rect_min.y = fmaxf(floor(point_screen.y - radius), 0);
        rect_max.x = fminf(point_screen.x + radius, frame->width);
        rect_max.y = fminf(point_screen.y + radius, frame->height);
        if ((rect_max.x - rect_min.x) * (rect_max.y - rect_min.y) == 0)
            continue;


        for (size_t tile_y = rect_min.y / ctx->tile_size.y; tile_y < (rect_max.y + ctx->tile_size.y-1) / ctx->tile_size.y; ++tile_y) {
            for (size_t tile_x = rect_min.x / ctx->tile_size.x; tile_x < (rect_max.x + ctx->tile_size.x-1) / ctx->tile_size.x; ++tile_x) {
                if (tile_y < 0) continue;
                if (tile_x < 0) continue;
                if (tile_y >= ctx->n_tiles.y) continue;
                if (tile_x >= ctx->n_tiles.x) continue;

                size_t tile = tile_y * ctx->n_tiles.x + tile_x;
                size_t *visible = ctx->visible + (tile * ctx->model->n_points);
                visible[ctx->n_visible[tile]++] = idx;
            }
        }

        ctx->inv_cov2d[idx] = inv_cov2d;
        ctx->radii[idx] = radius;
        ctx->ndc_points[idx].x = point_screen.x;
        ctx->ndc_points[idx].y = point_screen.y;
    }
}

static void 
render_kernel(void *args) {
    render_batch_args *rargs = (render_batch_args*)args;
    raster_ctx *ctx = rargs->ctx;
    camera *camera = rargs->camera;
    frame *frame = rargs->frame;
    size_t tile = rargs->tile;

    if (ctx->n_visible[tile] == 0) return;

    size_t x_start = (tile % ctx->n_tiles.x) * ctx->tile_size.x;
    size_t y_start = (tile / ctx->n_tiles.x) * ctx->tile_size.y;
    size_t x_end = x_start + ctx->tile_size.x;
    size_t y_end = y_start + ctx->tile_size.y;
    if (x_end > frame->width)
        x_end = frame->width;
    if (y_end > frame->height)
        y_end = frame->height;

    size_t *visible = ctx->visible + (tile * ctx->model->n_points);
    size_t itercnt = ctx->n_visible[tile];

    vec3f *throughputs = ctx->throughputs + (tile * ctx->tile_size.x * ctx->tile_size.y);

    for (size_t i = 0; i < ctx->tile_size.x * ctx->tile_size.y; ++i) {
        throughputs[i].x = 1.f;
        throughputs[i].y = 1.f;
        throughputs[i].z = 1.f;
    }

    uint8_t *done = calloc(ctx->tile_size.x * ctx->tile_size.y, sizeof(uint8_t));
    size_t n_done = 0;

    for (size_t z = 0; z < itercnt; ++z) {
        // if (n_done == ctx->tile_size.x * ctx->tile_size.y) break;
        size_t i = visible[z];
        vec3f color = ctx->model->colors[i];
        float opacity = ctx->model->opacities[i];
        vec3f con_o = ctx->inv_cov2d[i];
        float radius = ctx->radii[i];
        vec2f p = {ctx->ndc_points[i].x, ctx->ndc_points[i].y};

        for (size_t y = y_start; y < y_end; ++y) {
            vec3f *row_colors = (frame->pixels + y*frame->width);
            for (size_t x = x_start; x < x_end; ++x) {
                size_t tile_idx = (y-y_start) * ctx->tile_size.x + (x-x_start);
                if (done[tile_idx]) continue;

                vec2f pix = {(float) x, (float) y};
                vec2f d = { p.x - pix.x, p.y - pix.y };
                if (fabsf(d.x) > radius || fabsf(d.y) > radius)
                    continue;

                float power = -0.5f * (con_o.x * d.x * d.x + con_o.z * d.y * d.y) - con_o.y * d.x * d.y;
                if (power > 0.0f)
                    continue;

                // float alpha = fminf(0.99f, opacity * fastExp(power));
                float alpha = fminf(0.99f, opacity * exp2f(power * LOG2E));
                // float alpha = fminf(0.99f, opacity * expf(power));
                if (alpha < 0.004f) /* 1/255 ~= 0.004 */
                    continue;

                vec3f co = { 
                    row_colors[x].x + color.x * alpha * throughputs[tile_idx].x, 
                    row_colors[x].y + color.y * alpha * throughputs[tile_idx].y,  
                    row_colors[x].z + color.z * alpha * throughputs[tile_idx].z
                };

                row_colors[x] = co;
                throughputs[tile_idx].x *= (1.f - alpha);
                throughputs[tile_idx].y *= (1.f - alpha);
                throughputs[tile_idx].z *= (1.f - alpha);

                if (fminf(throughputs[tile_idx].x, fminf(throughputs[tile_idx].y, throughputs[tile_idx].z)) < 0.001f) {
                    done[tile_idx] = 1;
                    n_done++;
                    break;
                }
            }
        }
    }
    free(done);
}

static void
render_batch(void *args) {
    render_batch_args *rargs = (render_batch_args*)args;
    for (size_t i = rargs->tile_start; i < rargs->tile_end; ++i) {
        if (i >= rargs->ctx->n_tiles.x * rargs->ctx->n_tiles.y) break;

        rargs->tile = i;
        render_kernel(rargs);
    }
}

void
rasterizer_render(raster_ctx *ctx, camera *camera, frame *frame) {
    for (size_t b = 0; b < ctx->n_tile_batches; ++b) {
        ctx->rargs[b] = (render_batch_args){
            ctx, camera, frame, 0, b * RASTERIZER_TILE_BATCH_SIZE, (b+1) * RASTERIZER_TILE_BATCH_SIZE
        };
        tpool_add_work(ctx->tpool, render_batch, &ctx->rargs[b]);
    }
    tpool_wait(ctx->tpool);
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
        free(ctx->n_visible);
        free(ctx->ndc_points);
        free(ctx->trans_points);
        free(ctx->radii);
        free(ctx->inv_cov2d);
        free(ctx->throughputs);
        free(ctx->rargs);
        tpool_destroy(ctx->tpool);
    }
    free(ctx);
}
