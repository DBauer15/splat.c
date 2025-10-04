#include <splatc/loader.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <rply.h>

#define C0 0.28209

static void
loader_nomalize_positions(gsmodel *model) {
    float min = 1e31f;
    float max = -1e31f;

    for (int i = 0; i < model->n_points; ++i) {
        min = fminf(min, model->positions[i].x);
        min = fminf(min, model->positions[i].y);
        min = fminf(min, model->positions[i].z);
        max = fmaxf(max, model->positions[i].x);
        max = fmaxf(max, model->positions[i].y);
        max = fmaxf(max, model->positions[i].z);
    }

    float extent = max - min;
    printf("[loader] scene scale %.2f, normalizing to [0, 1)\n", extent);
    for (int i = 0; i < model->n_points; ++i) {
        for (int j = 0; j < 3; ++j) {
            model->positions[i].v[j] = 
                (model->positions[i].v[j] - min) / extent;
        }
    }
}

static int
loader_ply_read_callback(p_ply_argument arg) {
    // long idx = -1;
    // if (!ply_get_argument_property(arg, NULL, NULL, &idx)) return 0;

    long dim = -1;
    gsmodel* model = NULL;
    if (!ply_get_argument_user_data(arg, &model, &dim)) return 0;
    if (!model) return 0;


    double v = ply_get_argument_value(arg);

    if (dim > 2) { /* color */
        model->colors[model->n_colors++ / 3].v[dim-3] = (float)v * C0 + 0.5f;
    }
    else { /* position */
        model->positions[model->n_points++ / 3].v[dim] = (float)v;
    }

    return 1;
}

gsmodel*
loader_gsmodel_from_sogs(const char *fn) {
    return NULL;
}

gsmodel*
loader_gsmodel_from_ply(const char *fn) {

    /* load file */
    p_ply ply = ply_open(fn, NULL, 0L, NULL);
    if (!ply) {
        printf("[loader] unable to open file %s\n", fn);
        return NULL;
    }
    if (!ply_read_header(ply)) {
        printf("[loader] unable to parse PLY header\n");
        return NULL;
    }

    gsmodel* model = calloc(1, sizeof(gsmodel));

    long num_x = ply_set_read_cb(ply, "vertex", "x", &loader_ply_read_callback, model, 0L);
    long num_y = ply_set_read_cb(ply, "vertex", "y", &loader_ply_read_callback, model, 1L);
    long num_z = ply_set_read_cb(ply, "vertex", "z", &loader_ply_read_callback, model, 2L);
    ply_set_read_cb(ply, "vertex", "f_dc_0", &loader_ply_read_callback, model, 3L);
    ply_set_read_cb(ply, "vertex", "f_dc_1", &loader_ply_read_callback, model, 4L);
    ply_set_read_cb(ply, "vertex", "f_dc_2", &loader_ply_read_callback, model, 5L);
    assert(num_x == num_y && num_y == num_z);

    model->positions = calloc(num_x, sizeof(vec3f));
    model->colors = calloc(num_x, sizeof(vec3f));
    model->n_points = 0;
    model->n_colors = 0;

    int ok = ply_read(ply);

    model->n_points /= 3;
    model->n_colors /= 3;

    if (!ok) {
        free(model->colors);
        free(model->positions);
        free(model);
        printf("[loader] unable to load PLY elements\n");
        return NULL;
    }

    // loader_nomalize_positions(model);
    printf("[loader] loaded PLY file %s\n", fn);
    return model;
}

void
loader_gsmodel_destroy(gsmodel *model) {
    free(model->colors);
    free(model->positions);
    free(model);
} 
