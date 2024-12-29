#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <omp.h>

#include "../audio/energy.h"

// Define HSL structure
typedef struct {
    float h; // Hue angle in degrees [0, 360)
    float s; // Saturation [0, 1]
    float l; // Lightness [0, 1]
} HSL;

// Define RGB structure
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB;

// Define Palette Size and Constants
#define MILKY_PALETTE_SIZE 256
#define MILKY_MAX_COLOR 63
#define MILKY_BRIGHTNESS_THRESHOLD 150 // Threshold for selective brightening
#define GRADIENT_SIZE 64                // Number of gradient colors

// Function Prototypes
void calculateHueRotationMatrix(float hue_deg, float matrix[3][3]);
void generatePalette(void);
void applyPaletteToCanvas(size_t currentTime, uint8_t *canvas, size_t width, size_t height);
uint8_t applyBrightness(float colorValue, float brightnessFactor);
RGB hslToRgb(HSL hsl);
HSL rgbToHsl(uint8_t r, uint8_t g, uint8_t b);
float hue2rgb(float p, float q, float t);
RGB changeHue(RGB originalRgb, float newHue);
void generateHuePalette(float hue_shift, float brightness);

// **New Functions for Palette Transition**
/**
 * Initializes the palette transition system.
 * Must be called before any palette transitions occur.
 */
void initializePaletteTransition(int transitionSteps);

/**
 * Sets the number of steps for palette transitions.
 *
 * @param steps The number of steps over which the transition should occur.
 */
void setTransitionSteps(int steps);

/**
 * Starts a palette transition.
 * This function should be called when a palette change is triggered.
 */
void startPaletteTransition(void);

#endif // PALETTE_H
