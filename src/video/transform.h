#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#include "../audio/energy.h"

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

// Define maximum and minimum rotation in degrees
#define MAX_ROTATION_DEGREES 15.0f
#define MIN_ROTATION_DEGREES 2.0f

// Convert degrees to radians
#define DEG_TO_RAD(degrees) ((degrees) * (M_PI / 180.0f))

void rotate(float timeFrame, uint8_t *tempBuffer, uint8_t *screen, float speed, float angle, size_t width, size_t height);

void scale(
    unsigned char *screen,     // Frame buffer (RGBA format)
    unsigned char *tempBuffer, // Temporary buffer for scaled image
    float scale,               // Scale factor
    size_t width,              // Frame width
    size_t height              // Frame height
);

#endif // TRANSFORM_H
