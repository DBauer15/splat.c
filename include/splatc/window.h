#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>

typedef struct {
  size_t width;
  size_t height;
  size_t channels;
  float aspect;
  uint8_t* pixels;
} frame;

typedef struct window;

window* window_create(size_t width, size_t height, const char* title);

void window_set_title(window* w, const char* title);

void window_update(window* w, frame* f);

void window_destroy(window* w);

#endif
