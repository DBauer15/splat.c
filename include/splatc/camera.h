#ifndef CAMERA_H
#define CAMERA_H

#include <math.h>
#include "linalg.h"

typedef struct {
    vec3f pos;
    vec3f at;
    vec3f up;
    vec3f right;
    vec3f forward;
    float fovy;
    float near;
    float far;
    float aspect;
} camera;

mat4
camera_get_view(camera *camera);

mat4
camera_get_projection(camera *camera);

#endif
