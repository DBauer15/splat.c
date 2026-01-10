#include <assert.h>
#include <math.h>
#include <rply.h>
#include <splatc/loader.h>
#include <stdio.h>
#include <stdlib.h>

#define C0 0.28209

typedef struct {
  gsmodel* model;
  size_t points_read;
  size_t colors_read;
  size_t scales_read;
  size_t rotations_read;
  size_t opacities_read;

  vec3f* scales;
  vec4f* rotations;

} ply_read_payload;

static void
loader_nomalize_positions(gsmodel* model) {
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
      model->positions[i].v[j] = (model->positions[i].v[j] - min) / extent;
    }
  }
}

static void
loader_compute_cov3d(ply_read_payload* payload, gsmodel* model) {
  for (size_t i = 0; i < model->n_points; ++i) {
    mat3 S = mat3_id();
    S.vv[0][0] = payload->scales[i].x;
    S.vv[1][1] = payload->scales[i].y;
    S.vv[2][2] = payload->scales[i].z;

    vec4f q = payload->rotations[i];
    float r = q.x;
    float x = q.y;
    float y = q.z;
    float z = q.w;

    mat3 R = {1.f - 2.f * (y * y + z * z), 2.f * (x * y - r * z),
              2.f * (x * z + r * y),       2.f * (x * y + r * z),
              1.f - 2.f * (x * x + z * z), 2.f * (y * z - r * x),
              2.f * (x * z - r * y),       2.f * (y * z + r * x),
              1.f - 2.f * (x * x + y * y)};

    mat3 M = matmul3(R, S);
    mat3 sigma = matmul3(M, transpose3(M));

    model->cov3d[i] = sigma;
  }
}

static int
loader_ply_read_callback(p_ply_argument arg) {
  long dim = -1;
  ply_read_payload* payload = NULL;
  if (!ply_get_argument_user_data(arg, (void**)&payload, &dim)) return 0;
  if (!payload) return 0;

  gsmodel* model = payload->model;
  if (!model) return 0;

  double v = ply_get_argument_value(arg);

  if (dim > 12) { /* opacity */
    model->opacities[payload->opacities_read++] = 1.f / (1.f + expf(-(float)v));
  } else if (dim > 8) { /* rotation */
    payload->rotations[payload->rotations_read++ / 4].v[dim - 9] = (float)v;
  } else if (dim > 5) { /* scale */
    payload->scales[payload->scales_read++ / 3].v[dim - 6] = expf((float)v);
  } else if (dim > 2) { /* color */
    model->colors[payload->colors_read++ / 3].v[dim - 3] = (float)v * C0 + 0.5f;
  } else { /* position */
    model->positions[payload->points_read++ / 3].v[dim] = (float)v;
  }

  return 1;
}

gsmodel*
loader_gsmodel_debug() {
  gsmodel* model = calloc(1, sizeof(gsmodel));

  model->positions = calloc(1, sizeof(vec3f));
  model->colors = calloc(1, sizeof(vec3f));
  model->opacities = calloc(1, sizeof(float));
  model->cov3d = calloc(1, sizeof(mat3));
  model->n_points = 1;

  model->positions[0] = (vec3f){0.f, 0.f, 0.f};
  model->colors[0] = (vec3f){1.f, 0.f, 0.f};
  model->opacities[0] = 1.f;
  model->cov3d[0] = mat3_id();
  model->cov3d[0].vv[0][0] = 0.005f;

  printf("[loader] loaded debug model\n");
  return model;
}

gsmodel*
loader_gsmodel_from_sogs(const char* fn) {
  return NULL;
}

gsmodel*
loader_gsmodel_from_ply(const char* fn) {
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
  ply_read_payload read_payload = {0};
  read_payload.model = model;

  long num_x = ply_set_read_cb(ply, "vertex", "x", &loader_ply_read_callback,
                               &read_payload, 0L);
  long num_y = ply_set_read_cb(ply, "vertex", "y", &loader_ply_read_callback,
                               &read_payload, 1L);
  long num_z = ply_set_read_cb(ply, "vertex", "z", &loader_ply_read_callback,
                               &read_payload, 2L);
  ply_set_read_cb(ply, "vertex", "f_dc_0", &loader_ply_read_callback,
                  &read_payload, 3L);
  ply_set_read_cb(ply, "vertex", "f_dc_1", &loader_ply_read_callback,
                  &read_payload, 4L);
  ply_set_read_cb(ply, "vertex", "f_dc_2", &loader_ply_read_callback,
                  &read_payload, 5L);
  ply_set_read_cb(ply, "vertex", "scale_0", &loader_ply_read_callback,
                  &read_payload, 6L);
  ply_set_read_cb(ply, "vertex", "scale_1", &loader_ply_read_callback,
                  &read_payload, 7L);
  ply_set_read_cb(ply, "vertex", "scale_2", &loader_ply_read_callback,
                  &read_payload, 8L);
  ply_set_read_cb(ply, "vertex", "rot_0", &loader_ply_read_callback,
                  &read_payload, 9L);
  ply_set_read_cb(ply, "vertex", "rot_1", &loader_ply_read_callback,
                  &read_payload, 10L);
  ply_set_read_cb(ply, "vertex", "rot_2", &loader_ply_read_callback,
                  &read_payload, 11L);
  ply_set_read_cb(ply, "vertex", "rot_3", &loader_ply_read_callback,
                  &read_payload, 12L);
  ply_set_read_cb(ply, "vertex", "opacity", &loader_ply_read_callback,
                  &read_payload, 13L);
  assert(num_x == num_y && num_y == num_z);

  model->positions = calloc(num_x, sizeof(vec3f));
  model->colors = calloc(num_x, sizeof(vec3f));
  model->opacities = calloc(num_x, sizeof(float));
  model->cov3d = calloc(num_x, sizeof(mat3));

  read_payload.scales = calloc(num_x, sizeof(vec3f));
  read_payload.rotations = calloc(num_x, sizeof(vec4f));
  model->n_points = 0;

  int ok = ply_read(ply);

  model->n_points = read_payload.points_read / 3;
  loader_compute_cov3d(&read_payload, model);

  free(read_payload.scales);
  free(read_payload.rotations);

  if (!ok) {
    free(model->colors);
    free(model->positions);
    free(model->opacities);
    free(model->cov3d);
    free(model);
    printf("[loader] unable to load PLY elements\n");
    return NULL;
  }

  printf("[loader] loaded PLY file %s\n", fn);
  return model;
}

void
loader_gsmodel_destroy(gsmodel* model) {
  free(model->colors);
  free(model->positions);
  free(model->opacities);
  free(model->cov3d);
  free(model);
}
