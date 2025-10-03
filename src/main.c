#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GLFW/glfw3.h>

#include <splatc/camera.h>
#include <splatc/linalg.h>
#include <splatc/ppm.h>

#define WIDTH   1280
#define HEIGHT  720

typedef struct {
    size_t width;
    size_t height;
    size_t channels;
    float aspect;
    uint8_t* pixels;
} frame;

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

frame 
frame_create(size_t width, size_t height, size_t channels) {
    uint8_t* pixels = calloc(width * height * channels, sizeof(uint8_t));

    float aspect = (float)width / height;
    return (frame){
        width, height, channels, aspect, pixels 
    };
}

void
frame_put_pixel(frame *f, size_t x, size_t y, uint8_t* c) {
    if (x < 0 || y < 0 || x >= f->width || y >= f->height) return;

    size_t coord = f->channels * f->width * y + f->channels * x;

    for (size_t i = 0; i < f->channels; ++i) {
        if (c) {
            f->pixels[coord + i] = c[i];
        }
        else {
            f->pixels[coord + i] = 255;
        }
    }
}

void
image_clear(frame* f) {
    memset(f->pixels, 0, f->height * f->width * f->channels * sizeof(uint8_t));
}


void
image_render(frame *frame) {
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

    GLFWwindow* window;
    uint32_t input_state = 0;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(WIDTH, HEIGHT, "splat.c", NULL, NULL);
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


    /* Apply transformation */
    camera cam = {};
    cam.pos = (vec3f){ 0.f, 0.f, 5.f };
    cam.at = (vec3f){ 0.f, 0.f, 0.f };
    cam.up = (vec3f){ 0.f, 1.f, 0.f };
    cam.fovy = 0.35 * M_PI;
    cam.near = 0.01f;
    cam.far = 100.f;
    cam.aspect = (float)frame_width / frame_height;
    glfwSetWindowUserPointer(window, &input_state);

    mat4 proj = camera_get_projection(&cam);
    mat4 view = mat4_id();

    /* Render image */
    frame image = frame_create(frame_width, frame_height, 3);

    size_t frame_no = 0;

    while (!glfwWindowShouldClose(window)) {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        image_clear(&image);

        /* Update view */
        update_view(&cam, input_state);
        view = camera_get_view(&cam);

        /* Draw frame */
        glDrawPixels(image.width, image.height, GL_RGB, GL_UNSIGNED_BYTE, image.pixels);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        frame_no = (frame_no+1)%1200;
    }

    glfwTerminate();

    return 0;
}
