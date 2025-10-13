#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <splatc/rasterizer.h>
#include <splatc/linalg.h>
#include <splatc/camera.h>
#include <splatc/loader.h>

struct raster_ctx_t {
    gsmodel *model;
    vec2f tile_lower_bound;
    vec2f tile_upper_bound;

    vec4f *ndc_points;
    size_t *visible;
    size_t n_visible;
    float* radii;
    vec3f *inv_cov2d;
};

static vec2f
frame_ndc_to_screen(vec2f p, frame *frame) {
    vec2f screen = { (0.5f + p.x * 0.5f) * frame->width, (0.5f + p.y * 0.5f) * frame->height };
    return screen;
}

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
        focal_x / t.z, 0.0f, -(focal_x * t.x) / (t.z * t.z),
        0.0f, focal_y / t.z, -(focal_y * t.y) / (t.z * t.z),
        0, 0, 0 };

    mat3 W = { 
        view->vv[0][0], view->vv[1][0], view->vv[2][0],
        view->vv[0][1], view->vv[1][1], view->vv[2][1],
        view->vv[0][2], view->vv[1][2], view->vv[2][2] };

	mat3 T = matmul3(W, J);

    mat3 Vrk = { cov_3d->v[0], cov_3d->v[1], cov_3d->v[2],
        cov_3d->v[1], cov_3d->v[3], cov_3d->v[4],
        cov_3d->v[2], cov_3d->v[4], cov_3d->v[5] };

	// mat3 cov = glm::transpose(T) * glm::transpose(Vrk) * T;
    mat3 VrkT = transpose3(Vrk);
    mat3 TT = transpose3(T);
    mat3 cov = matmul3(TT, VrkT);
	cov = matmul3(cov, T);

	return (vec3f){ cov.vv[0][0], cov.vv[0][1], cov.vv[1][1] };
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
    memset(f->pixels, 255, f->height * f->width * f->channels * sizeof(uint8_t));
}

raster_ctx*
rasterizer_context_create(gsmodel* model, vec2f tile_lower_bound, vec2f tile_upper_bound) {
    assert(model != NULL);
    raster_ctx *ctx = calloc(1, sizeof(raster_ctx));

    ctx->model = model;
    ctx->tile_lower_bound = tile_lower_bound;
    ctx->tile_upper_bound = tile_upper_bound;

    size_t *visible = calloc(model->n_points, sizeof(size_t));
    ctx->visible = visible;

    vec4f *ndc_points = calloc(model->n_points, sizeof(vec4f));
    ctx->ndc_points = ndc_points;

    float *radii = calloc(model->n_points, sizeof(float));
    ctx->radii = radii;

    vec3f *inv_cov2d = calloc(model->n_points, sizeof(vec3f));
    ctx->inv_cov2d = inv_cov2d;

    return ctx;
}

void
rasterizer_set_tile(raster_ctx *ctx, vec2f tile_lower_bound, vec2f tile_upper_bound) {
    ctx->tile_upper_bound = tile_upper_bound;
    ctx->tile_lower_bound = tile_lower_bound;
}

void
rasterizer_preprocess(raster_ctx *ctx, camera *camera, frame *frame) {
    mat4 proj = camera_get_projection(camera);
    mat4 view = camera_get_view(camera);
    mat4 pv = matmul4(view, proj);

    float tan_fovy = tanf(camera->fovy);
    float tan_fovx = tan_fovy * camera->aspect;
    float focal_y = frame->height / (2.0f * tan_fovy);
	float focal_x = frame->width / (2.0f * tan_fovx);

    for (int i = 0; i < ctx->model->n_points; ++i) {
        vec4f vertex = (vec4f){
            ctx->model->positions[i].x,
            ctx->model->positions[i].y,
            ctx->model->positions[i].z,
            1.f
        };
        vec4f vview = matmulv4(view, vertex);
        vec4f vproj = matmulv4(proj, vview);

        float rw = 1.f / (vproj.w + 1e-5f);
        ctx->ndc_points[i].v[0] = (vproj.x * rw);
        ctx->ndc_points[i].v[1] = (vproj.y * rw);
        ctx->ndc_points[i].v[2] = (vproj.z * rw);
        
        vec3f cov = compute_cov2d(&vview, &view, &ctx->model->cov3d[i], focal_x, focal_y, tan_fovx, tan_fovy);

        const float h_var = 0.3f;
        const float det_cov = cov.x * cov.z - cov.y * cov.y;
        cov.x += h_var;
        cov.z += h_var;
        const float det_cov_plus_h_cov = cov.x * cov.z - cov.y * cov.y;
        float h_convolution_scaling = 1.0f;

        // Invert covariance (EWA algorithm)
        const float det = det_cov_plus_h_cov;

        if (det == 0.0f)
            continue;

        float det_inv = 1.f / det;
        vec3f inv_cov2d = { cov.z * det_inv, -cov.y * det_inv, cov.x * det_inv };

        // Compute extent in screen space (by finding eigenvalues of
        // 2D covariance matrix). Use extent to compute a bounding rectangle
        // of screen-space tiles that this Gaussian overlaps with. Quit if
        // rectangle covers 0 tiles. 
        float mid = 0.5f * (cov.x + cov.z);
        float lambda1 = mid + sqrtf(fmaxf(0.1f, mid * mid - det));
        float lambda2 = mid - sqrtf(fmaxf(0.1f, mid * mid - det));
        float radius = ceilf(3.f * sqrtf(fmaxf(lambda1, lambda2)));
        // float2 point_image = { ndc2Pix(p_proj.x, W), ndc2Pix(p_proj.y, H) };
        // uint2 rect_min, rect_max;
        // getRect(point_image, my_radius, rect_min, rect_max, grid);
        // if ((rect_max.x - rect_min.x) * (rect_max.y - rect_min.y) == 0)
            // return;

        ctx->inv_cov2d[i] = inv_cov2d;
        ctx->radii[i] = radius;
    }
}

void
rasterizer_mark_visible(raster_ctx *ctx, camera *camera) {
    ctx->n_visible = 0;
    for (size_t i = 0; i < ctx->model->n_points; ++i) {
        int visible = !(ctx->ndc_points[i].x < ctx->tile_lower_bound.x || 
                          ctx->ndc_points[i].x > ctx->tile_upper_bound.x ||
                          ctx->ndc_points[i].y < ctx->tile_lower_bound.y || 
                          ctx->ndc_points[i].y > ctx->tile_upper_bound.y ||
                          ctx->ndc_points[i].z < -1.f ||
                          ctx->ndc_points[i].z > 1.f ||
                          ctx->model->opacities[i] < 1e-2f);

        if (visible) {
            ctx->visible[ctx->n_visible++] = i;
        }
    }
}

void
rasterizer_render(raster_ctx *ctx, camera *camera, frame *frame) {
    if (ctx->n_visible == 0) return;

    size_t x_start = frame->width * (0.5 + 0.5 * ctx->tile_lower_bound.x);
    size_t y_start = frame->height * (0.5 + 0.5 * ctx->tile_lower_bound.y);
    size_t x_end = frame->width * (0.5 + 0.5 * ctx->tile_upper_bound.x);
    size_t y_end = frame->height * (0.5 + 0.5 * ctx->tile_upper_bound.y);

    for (size_t y = y_start; y < y_end; ++y) {
        for (size_t x = x_start; x < x_end; ++x) {

            vec3f color = {0, 0, 0};
            vec3f one = { 1.f, 1.f, 1.f };
            vec3f throughput = {1.f, 1.f, 1.f};
            for (size_t z = 0; z < fmin(20000, ctx->n_visible); ++z) {
                size_t i = ctx->visible[z];

                vec2f pix = {(float) x, (float) y};
                vec2f p = { ctx->ndc_points[i].x, ctx->ndc_points[i].y };
                p = frame_ndc_to_screen(p, frame);

                vec2f d = sub2(p, pix);
                vec3f con_o = ctx->inv_cov2d[i];
                float opacity = ctx->model->opacities[i];
                float power = -0.5f * (con_o.x * d.x * d.x + con_o.z * d.y * d.y) - con_o.y * d.x * d.y;
                if (power > 0.0f)
                    continue;

                float alpha = fminf(0.99f, opacity * expf(power));
                if (alpha < 1.0f / 255.0f)
                    continue;

                // if (dist < ctx->radii[i]) {
                vec3f o3 = {alpha, alpha, alpha};
                color = add3(color, mul3(throughput, mul3(ctx->model->colors[i], o3)));
                throughput.x *= (1.f - opacity);
                throughput.y *= (1.f - opacity);
                throughput.z *= (1.f - opacity);
                // }

                if (dot3(throughput, throughput) < 0.01f) {
                    break;
                }
                if (color.x > 1.f || color.y > 1.f || color.z > 1.f) {
                    break;
                }
            }
            frame_put_pixel(frame, x, y, color.v);
        }
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
        free(ctx->radii);
        free(ctx->inv_cov2d);
    }
    free(ctx);
}
