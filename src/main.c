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

#define WIDTH   640
#define HEIGHT  360

#define TILESIZE 8

typedef enum {
    INPUT_W = (1<<0),
    INPUT_A = (1<<1),
    INPUT_S = (1<<2),
    INPUT_D = (1<<3),
    INPUT_Q = (1<<4),
    INPUT_E = (1<<5),
    INPUT_MB0 = (1<<30),
    INPUT_MB1 = (1<<31),
} key_state;

typedef struct {
    uint32_t key_input;
    uint32_t mouse_input;
    double mouse_x, mouse_y;
} window_state;

void size_callback(GLFWwindow *window, int width, int height) {
    glPixelZoom((float)width / WIDTH, (float)height / HEIGHT);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    window_state *ws = (window_state*) glfwGetWindowUserPointer(window);
    if(!ws) return;

    if(action != GLFW_PRESS && action != GLFW_RELEASE) return;
    switch(key) {
        case GLFW_KEY_W:
            ws->key_input = action == GLFW_PRESS ? (ws->key_input | INPUT_W) : (ws->key_input & ~INPUT_W);
        break;
        case GLFW_KEY_S:
            ws->key_input = action == GLFW_PRESS ? (ws->key_input | INPUT_S) : (ws->key_input & ~INPUT_S);
        break;
        case GLFW_KEY_A:
            ws->key_input = action == GLFW_PRESS ? (ws->key_input | INPUT_A) : (ws->key_input & ~INPUT_A);
        break;
        case GLFW_KEY_D:
            ws->key_input = action == GLFW_PRESS ? (ws->key_input | INPUT_D) : (ws->key_input & ~INPUT_D);
        break;
        case GLFW_KEY_Q:
            ws->key_input = action == GLFW_PRESS ? (ws->key_input | INPUT_Q) : (ws->key_input & ~INPUT_Q);
        break;
        case GLFW_KEY_E:
            ws->key_input = action == GLFW_PRESS ? (ws->key_input | INPUT_E) : (ws->key_input & ~INPUT_E);
        break;
    }
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    window_state *ws = (window_state*) glfwGetWindowUserPointer(window);
    if(!ws) return;

    if(action != GLFW_PRESS && action != GLFW_RELEASE) return;
    switch(button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            ws->mouse_input = action == GLFW_PRESS ? (ws->mouse_input | INPUT_MB0) : (ws->mouse_input & ~INPUT_MB0);
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            ws->mouse_input = action == GLFW_PRESS ? (ws->mouse_input | INPUT_MB1) : (ws->mouse_input & ~INPUT_MB1);
            break;
    }
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    window_state *ws = (window_state*)glfwGetWindowUserPointer(window);
    if(!ws) return;

    double prev_mouse_x = ws->mouse_x;
    double prev_mouse_y = ws->mouse_y;

    double dx = xpos - prev_mouse_x;
    double dy = ypos - prev_mouse_y;

    ws->mouse_x = xpos;
    ws->mouse_y = ypos;
}

void 
update_view(camera *cam, uint32_t input_state) {
    float speed = 0.15f;
    vec3f offset_pos = {};

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

    /* set callbacks */
    glfwSetKeyCallback(window, key_callback);
    glfwSetWindowSizeCallback(window, size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);

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
