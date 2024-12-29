#ifndef TUNNEL_H
#define TUNNEL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include "../../audio/energy.h"

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

// intensity of the chaser's trail on the screen
#define MILKY_MAX_COLOR 255

void drawPixel(uint8_t *screen, size_t width, size_t height, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void renderTunnelCircle(size_t currentTime, float timeFrame, uint8_t *screen, int radius, unsigned int count, size_t width, size_t height, unsigned int seed, int thickness);

#endif // TUNNEL_H
