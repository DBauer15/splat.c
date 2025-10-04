#ifndef LOADER_H
#define LOADER_H

#include <splatc/linalg.h>

typedef struct {
    long n_points;
    long n_colors;
    vec3f* positions; 
    vec3f* colors; 
} gsmodel;


gsmodel*
loader_gsmodel_from_sogs(const char *fn);

gsmodel*
loader_gsmodel_from_ply(const char *fn);

void
loader_gsmodel_destroy(gsmodel *model);

#endif
