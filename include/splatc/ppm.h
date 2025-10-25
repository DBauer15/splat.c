#ifndef PPM_H
#define PPM_H
#include <stdint.h>
#include <stdlib.h>

void ppm_write(uint8_t* pixels, size_t width, size_t height, char* fn);

#endif
