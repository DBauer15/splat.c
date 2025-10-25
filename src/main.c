#include <time.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLFW/glfw3.h>

#include <splatc/camera.h>
#include <splatc/linalg.h>
#include <splatc/loader.h>
#include <splatc/ppm.h>
#include <splatc/rasterizer.h>

#ifndef M_PI
#define M_PI 3.14159265359
#endif

#define WIDTH   1280
#define HEIGHT  720

#define TILESIZE 8

#define INPUT_W (1<<0)
#define INPUT_A (1<<1)
#define INPUT_S (1<<2)
#define INPUT_D (1<<3)
#define INPUT_Q (1<<4)
#define INPUT_E (1<<5)
#define INPUT_I (1<<6)
#define INPUT_J (1<<7)
#define INPUT_K (1<<8)
#define INPUT_L (1<<9)

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    uint32_t *input_state = (uint32_t*)glfwGetWindowUserPointer(window);
    if(!input_state) return;
    if(action != GLFW_PRESS && action != GLFW_RELEASE) return;

    if (key == GLFW_KEY_W) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_W) : ((*input_state) & ~INPUT_W);
    }
    if (key == GLFW_KEY_S) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_S) : ((*input_state) & ~INPUT_S);
    }
    if (key == GLFW_KEY_A) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_A) : ((*input_state) & ~INPUT_A);
    }
    if (key == GLFW_KEY_D) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_D) : ((*input_state) & ~INPUT_D);
    }
    if (key == GLFW_KEY_Q) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_Q) : ((*input_state) & ~INPUT_Q);
    }
    if (key == GLFW_KEY_E) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_E) : ((*input_state) & ~INPUT_E);
    }
    if (key == GLFW_KEY_I) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_I) : ((*input_state) & ~INPUT_I);
    }
    if (key == GLFW_KEY_J) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_J) : ((*input_state) & ~INPUT_J);
    }
    if (key == GLFW_KEY_K) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_K) : ((*input_state) & ~INPUT_K);
    }
    if (key == GLFW_KEY_L) {
        *input_state = action == GLFW_PRESS ? ((*input_state) | INPUT_L) : ((*input_state) & ~INPUT_L);
    }
}

void 
update_view(camera *cam, uint32_t input_state) {
    float speed = 0.15f;
    vec3f offset_pos = {};
    vec2f offset_at = {};

    if (input_state & INPUT_W) {
        offset_pos.z = -speed;
    }
    if (input_state & INPUT_S) {
        offset_pos.z = speed;
    }
    if (input_state & INPUT_D) {
        offset_pos.x = -speed;
    }
    if (input_state & INPUT_A) {
        offset_pos.x = speed;
    }
    if (input_state & INPUT_Q) {
        offset_pos.y = -speed;
    }
    if (input_state & INPUT_E) {
        offset_pos.y = speed;
    }
    if (input_state & INPUT_I) {
        offset_at.y = -speed;
    }
    if (input_state & INPUT_K) {
        offset_at.y = speed;
    }
    if (input_state & INPUT_L) {
        offset_at.x = -speed;
    }
    if (input_state & INPUT_J) {
        offset_at.x = speed;
    }

    vec3f dir = { sin(offset_at.x), 0.f, cos(offset_at.x) };
    cam->pos.x += dot3(offset_pos, cam->right);
    cam->pos.y += dot3(offset_pos, cam->up);
    cam->pos.z += dot3(offset_pos, cam->forward);
    cam->at.x += dot3(offset_pos, cam->right);
    cam->at.y += dot3(offset_pos, cam->up);
    cam->at.z += dot3(offset_pos, cam->forward);
}


void
image_save(frame *frame) {
    ppm_write(frame->pixels, WIDTH, HEIGHT, "render.ppm");
}

int 
main(int ac, const char** av) {
    if (ac < 2) {
        return -1;
    }

    /* load gsmodel */
    gsmodel *model = loader_gsmodel_from_ply(av[1]);
    // gsmodel *model = loader_gsmodel_debug();
    if (!model) return -1;

    /* create window */
    GLFWwindow* window;
    uint32_t input_state = 0;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    char window_title[128] = "splat.c";
    window = glfwCreateWindow(WIDTH, HEIGHT, window_title, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    /* set key callback */
    glfwSetKeyCallback(window, key_callback);

    int frame_width, frame_height;
    glfwGetFramebufferSize(window, &frame_width, &frame_height);
    glViewport(0, 0, frame_width, frame_height);
    printf("Window(%d, %d)\n", frame_width, frame_height);


    /* set up camera */
    camera cam = {};
    cam.pos = (vec3f){ 0.0f, 0.0f, -10.f };
    cam.at = (vec3f){ 0.f, 0.f, 0.f };
    cam.up = (vec3f){ 0.f, 1.f, 0.f };
    cam.fovy = 0.35 * M_PI;
    cam.near = 0.1f;
    cam.far = 100.f;
    cam.aspect = (float)frame_width / frame_height;
    glfwSetWindowUserPointer(window, &input_state);

    /* Render image */
    frame *image = rasterizer_frame_create(frame_width, frame_height);

    /* Create rasterizer context */
    vec2u tile_size = { TILESIZE, TILESIZE };
    raster_ctx *ctx = rasterizer_context_create(model, image, tile_size);

    size_t frame_no = 0;    
    clock_t start, end, frame_start, frame_end;
    double trans_time, render_time, frame_time;
    double fps = 0;

    while (!glfwWindowShouldClose(window)) {
        frame_start = clock();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* Update view */
        update_view(&cam, input_state);
        start = clock();
        rasterizer_preprocess(ctx, &cam, image);
        end = clock();
        trans_time = ((double)(end - start)) / CLOCKS_PER_SEC;

        /* Draw frame */
        start = clock();
        rasterizer_frame_clear(image);
        rasterizer_render(ctx, &cam, image);
        glDrawPixels(image->width, image->height, GL_RGB, GL_FLOAT, image->pixels);
        end = clock();
        render_time = ((double)(end - start)) / CLOCKS_PER_SEC;

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        /* Update FPS */
        frame_end = clock();
        frame_time = ((double)(frame_end - frame_start)) / CLOCKS_PER_SEC;
        fps = fps * 0.5 + (1.f/frame_time) * 0.5;
        if (frame_no % 10 == 0) {
            snprintf(window_title, 128, "splat.c | %.1f (%.3f / %.3f)", fps, trans_time, render_time);
            glfwSetWindowTitle(window, window_title);
        }

        frame_no = (frame_no+1)%1200;
    }

    glfwTerminate();

    /* cleanup */
    rasterizer_context_destroy(ctx);
    rasterizer_frame_destroy(image);
    loader_gsmodel_destroy(model);

    return 0;
}
