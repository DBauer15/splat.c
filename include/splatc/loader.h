#ifndef LOADER_H
#define LOADER_H

#include <splatc/linalg.h>

typedef struct {
    long n_points;
    vec3f* positions; 
    vec3f* colors; 
    float* opacities;
    mat3* cov3d
} gsmodel;

gsmodel*
loader_gsmodel_from_splat(const char *fn);

gsmodel*
loader_gsmodel_from_sogs(const char *fn);

gsmodel*
loader_gsmodel_from_ply(const char *fn);

void
loader_gsmodel_destroy(gsmodel *model);

#endif
