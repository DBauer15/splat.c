#include <splatc/ppm.h>
#include <stdio.h>

static float clamp(float i, float min, float max) {
    return (i > max) ? max : ((i < min) ? min : i);
}

static uint8_t f2c(float f) {
    return (uint8_t)(255.f * clamp(f, 0.f, 1.f));
}

void
ppm_write(float* pixels, size_t width, size_t height, char* fn) {
  FILE* ppm = fopen(fn, "w");
  if (!ppm) {
    printf("Unable to open file %s\n", fn);
    return;
  }

  fprintf(ppm, "P3\n%d %d\n%d\n", width, height, 255);

  for (int row = height-1; row >= 0; --row) {
    for (int col = 0; col < width; ++col) {
      fprintf(ppm, "%d %d %d  ", 
              f2c(pixels[3 * row * width + 3 * col + 0]),
              f2c(pixels[3 * row * width + 3 * col + 1]),
              f2c(pixels[3 * row * width + 3 * col + 2]));
    }
    fprintf(ppm, "\n");
  }

  fclose(ppm);
}
