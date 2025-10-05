#ifndef LINALG_H
#define LINALG_H


/* type definitions */
typedef union {
    float v[2];
    struct { 
        float x, y;
    };

} vec2f;

typedef union {
    float v[3];
    struct { 
        float x, y, z;
    };

} vec3f;

typedef union {
    float v[4];
    struct { 
        float x, y, z, w;
    };

} vec4f;

typedef union {
    float v[16];
    float vv[4][4];
} mat4;
/* end type definitions */

/* vector ops */
float dot2(vec2f a, vec2f b);
float dot3(vec3f a, vec3f b);
float dot4(vec4f a, vec4f b);

vec2f sub2(vec2f a, vec2f b);
vec3f sub3(vec3f a, vec3f b);
vec4f sub4(vec4f a, vec4f b);

vec2f add2(vec2f a, vec2f b);
vec3f add3(vec3f a, vec3f b);
vec4f add4(vec4f a, vec4f b);

vec2f norm2(vec2f a);
vec3f norm3(vec3f a);
vec4f norm4(vec4f a);

vec3f cross3(vec3f a, vec3f b);
/* end vector ops */


/* matrix ops */
mat4 mat4_id();
void matmul_mat(mat4 *b, mat4 *a, mat4 *c);
vec3f matmul_v3(mat4 b, vec3f a);
vec4f matmul_v4(mat4 *b, vec4f *a);
/* end matrix ops */

#endif
