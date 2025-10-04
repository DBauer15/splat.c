#include <splatc/linalg.h>
#include <math.h>

#define NORM_OP(N) \
    vec##N##f norm##N (vec##N##f a) {   \
        vec##N##f result;                           \
        float mag = sqrtf(dot##N (a, a));           \
        for (int i = 0; i < N; ++i) {               \
            result.v[i] = a.v[i] / mag;             \
        }                                           \
        return result;                              \
    }

#define DOT_OP(N) \
    float dot##N (vec##N##f a, vec##N##f b) {   \
        float result = 0.f;                     \
        for (int i = 0; i < N; ++i) {           \
            result += a.v[i] * b.v[i];          \
        }                                       \
        return result;                          \
    }

#define ADD_OP(N) \
    vec##N##f add##N (vec##N##f a, vec##N##f b) {   \
        vec##N##f result;                           \
        for (int i = 0; i < N; ++i) {               \
            result.v[i] = a.v[i] + b.v[i];          \
        }                                           \
        return result;                              \
    }

#define SUB_OP(N) \
    vec##N##f sub##N (vec##N##f a, vec##N##f b) {   \
        vec##N##f result;                           \
        for (int i = 0; i < N; ++i) {               \
            result.v[i] = a.v[i] - b.v[i];          \
        }                                           \
        return result;                              \
    }


DOT_OP(2)
DOT_OP(3)
DOT_OP(4)

SUB_OP(2)
SUB_OP(3)
SUB_OP(4)

ADD_OP(2)
ADD_OP(3)
ADD_OP(4)

NORM_OP(2)
NORM_OP(3)
NORM_OP(4)

vec3f
cross3(vec3f a, vec3f b) {
    return (vec3f){
        a.v[1] * b.v[2] - a.v[2] * b.v[1],
        a.v[2] * b.v[0] - a.v[0] * b.v[2],
        a.v[0] * b.v[1] - a.v[1] * b.v[0],
    };
}

#define MAT_ID(N) \
    mat ## N                     \
    mat ## N ## _id() {          \
        mat ## N a = {};         \
        for (int i = 0; i < N; ++i) {   \
            a.vv[i][i] = 1.f;         \
        }                               \
        return a;                       \
    }

MAT_ID(4)


mat4
matmul_mat(mat4 b, mat4 a) {
    mat4 c = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            c.vv[i][j] = 0.f;
            for (int k = 0; k < 4; ++k) {
                c.vv[i][j] += b.vv[i][k] * a.vv[k][j];
            }
        }
    }
    return c;
}

vec3f
matmul_v3(mat4 b, vec3f a) {
    vec3f c = {};
    for (int i = 0; i < 3; ++i) {
        c.v[i] = 0.f;
        for (int j = 0; j < 4; ++j) {
            c.v[i] += (j == 3 ? 1.f : a.v[j]) * b.vv[j][i];
        }
    }
    return c;
}

vec4f
matmul_v4(mat4 *b, vec4f *a) {
    vec4f c = {};
    for (int i = 0; i < 4; ++i) {
        c.v[i] = 0.f;
        for (int j = 0; j < 4; ++j) {
            c.v[i] += a->v[j] * b->vv[j][i];
        }
    }
    return c;
}
