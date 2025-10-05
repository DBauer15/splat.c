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

#define WIDTH   1280
#define HEIGHT  720


#define INPUT_W (1<<0)
#define INPUT_A (1<<1)
#define INPUT_S (1<<2)
#define INPUT_D (1<<3)
#define INPUT_Q (1<<4)
#define INPUT_E (1<<5)

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
}

void 
update_view(camera *cam, uint32_t input_state) {
    float speed = 0.05f;
    vec3f offset = {};
    if (input_state & INPUT_W) {
        offset.z = speed;
    }
    if (input_state & INPUT_S) {
        offset.z = -speed;
    }
    if (input_state & INPUT_D) {
        offset.x = -speed;
    }
    if (input_state & INPUT_A) {
        offset.x = speed;
    }
    if (input_state & INPUT_Q) {
        offset.y = -speed;
    }
    if (input_state & INPUT_E) {
        offset.y = speed;
    }

    cam->pos.x += dot3(offset, cam->right);
    cam->pos.y += dot3(offset, cam->up);
    cam->pos.z += dot3(offset, cam->forward);
    cam->at.x += dot3(offset, cam->right);
    cam->at.y += dot3(offset, cam->up);
    cam->at.z += dot3(offset, cam->forward);
}



void
image_render(vec3f *points, gsmodel *model, frame *frame) {
    // for (size_t x = 0; x < frame->width; ++x) {
    //     for (size_t y = 0; y < frame->height; ++y) {
    //         for (size_t n = 0; n < 8; ++n) {
    //             int idx = (rand() % model->n_points);
    //             if (abs((0.5 + points[idx].x * 0.5) * frame->width - x) < 64 && abs((0.5 + points[idx].y * 0.5) * frame->height - y) < 64) {
    //                 frame_put_pixel(frame, x, y, model->colors[idx].v);
    //                 break;
    //             }
    //         }
    //     }
    // }

    // for (size_t i = 0; i < model->n_points; ++i) {
    //     if (points[i].x < -1.f || points[i].x > 1.f) continue;
    //     if (points[i].y < -1.f || points[i].y > 1.f) continue;
    //     if (points[i].z < -1.f || points[i].z > 1.f) continue;
    //     size_t x = frame->width * (0.5f + points[i].x * 0.5f);
    //     size_t y = frame->height * (0.5f + points[i].y * 0.5f);
    //     frame_put_pixel(frame, x, y, model->colors[i].v);
    // }

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
    if (!model) return -1;
    // vec3f* ndc_points = calloc(model->n_points, sizeof(vec3f));
    vec2f l = {-1.f, -1.f};
    vec2f u = {1.f, 1.f};
    raster_ctx *ctx = rasterizer_context_create(model, l, u);

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

    /* set key callback */
    glfwSetKeyCallback(window, key_callback);

    int frame_width, frame_height;
    glfwGetFramebufferSize(window, &frame_width, &frame_height);
    glViewport(0, 0, frame_width, frame_height);
    printf("Window(%d, %d)\n", frame_width, frame_height);


    /* set up camera */
    camera cam = {};
    cam.pos = (vec3f){ 0.f, 0.f, -1.f };
    cam.at = (vec3f){ 0.f, 0.f, 0.f };
    cam.up = (vec3f){ 0.f, 1.f, 0.f };
    cam.fovy = 0.35 * M_PI;
    cam.near = 0.0001f;
    cam.far = 100.f;
    cam.aspect = (float)frame_width / frame_height;
    glfwSetWindowUserPointer(window, &input_state);

    /* Render image */
    frame *image = rasterizer_frame_create(frame_width, frame_height, 3);


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
        rasterizer_preprocess(ctx, &cam);
        rasterizer_mark_visible(ctx, &cam);
        end = clock();
        trans_time = ((double)(end - start)) / CLOCKS_PER_SEC;

        /* Draw frame */
        start = clock();
        rasterizer_frame_clear(image);
        rasterizer_render(ctx, &cam, image);
        glDrawPixels(image->width, image->height, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
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
