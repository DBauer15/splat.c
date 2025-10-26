#include <splatc/ppm.h>
#include <stdio.h>

void
ppm_write(uint8_t* pixels, size_t width, size_t height, char* fn) {
  FILE* ppm = fopen(fn, "w");
  if (!ppm) {
    printf("Unable to open file %s\n", fn);
    return;
  }

  fprintf(ppm, "P3\n%d %d\n%d\n", width, height, 255);

  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      fprintf(ppm, "%d %d %d  ", pixels[3 * row * width + 3 * col + 0],
              pixels[3 * row * width + 3 * col + 1],
              pixels[3 * row * width + 3 * col + 2]);
    }
    fprintf(ppm, "\n");
  }

  fclose(ppm);
}
