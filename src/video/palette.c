#include "palette.h"

// Define a palette as an array of 256 RGB color values
uint8_t milky_palettePalette[MILKY_PALETTE_SIZE][3];
static clock_t milky_paletteLastPaletteInitTime = 0;

// **New Variables for Palette Transition**
static RGB currentPalette[MILKY_PALETTE_SIZE];
static RGB targetPalette[MILKY_PALETTE_SIZE];
static RGB oldPalette[MILKY_PALETTE_SIZE];
static int isTransitioning = 0;
static int transitionStep = 0;
static int totalTransitionSteps = 650; // Default transition steps

// Helper function for HSL to RGB conversion
float hue2rgb(float p, float q, float t) {
    if(t < 0.0f) t += 1.0f;
    if(t > 1.0f) t -= 1.0f;
    if(t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
    if(t < 1.0f/2.0f) return q;
    if(t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
    return p;
}

// Function to convert RGB to HSL
HSL rgbToHsl(uint8_t r, uint8_t g, uint8_t b) {
    float r_norm = r / 255.0f;
    float g_norm = g / 255.0f;
    float b_norm = b / 255.0f;
    
    float max = r_norm > g_norm ? (r_norm > b_norm ? r_norm : b_norm) : (g_norm > b_norm ? g_norm : b_norm);
    float min = r_norm < g_norm ? (r_norm < b_norm ? r_norm : b_norm) : (g_norm < b_norm ? g_norm : b_norm);
    float delta = max - min;
    
    HSL hsl;
    hsl.l = (max + min) / 2.0f;
    
    if(delta == 0.0f) {
        hsl.h = 0.0f;
        hsl.s = 0.0f;
    }
    else {
        hsl.s = hsl.l > 0.5f ? delta / (2.0f - max - min) : delta / (max + min);
        
        if(max == r_norm) {
            hsl.h = (g_norm - b_norm) / delta + (g_norm < b_norm ? 6.0f : 0.0f);
        }
        else if(max == g_norm) {
            hsl.h = (b_norm - r_norm) / delta + 2.0f;
        }
        else {
            hsl.h = (r_norm - g_norm) / delta + 4.0f;
        }
        
        hsl.h *= 60.0f; // Convert to degrees
        if(hsl.h < 0.0f) hsl.h += 360.0f;
    }
    
    return hsl;
}

// Function to convert HSL to RGB
RGB hslToRgb(HSL hsl) {
    float r, g, b;
    
    if(hsl.s == 0.0f) {
        r = g = b = hsl.l; // Achromatic
    }
    else {
        float q = hsl.l < 0.5f ? hsl.l * (1.0f + hsl.s) : hsl.l + hsl.s - hsl.l * hsl.s;
        float p = 2.0f * hsl.l - q;
        float h_norm = hsl.h / 360.0f;
        
        float t_r = h_norm + 1.0f/3.0f;
        float t_g = h_norm;
        float t_b = h_norm - 1.0f/3.0f;
        
        r = hue2rgb(p, q, t_r);
        g = hue2rgb(p, q, t_g);
        b = hue2rgb(p, q, t_b);
    }
    
    RGB rgb;
    rgb.r = (uint8_t)(r * 255.0f);
    rgb.g = (uint8_t)(g * 255.0f);
    rgb.b = (uint8_t)(b * 255.0f);
    
    return rgb;
}

RGB changeHue(RGB originalRgb, float newHue) {
    HSL hsl = rgbToHsl(originalRgb.r, originalRgb.g, originalRgb.b);
    hsl.h = fmodf(newHue, 360.0f); // Ensure hue stays within [0, 360)
    return hslToRgb(hsl);
}

/**
 * Sets the RGB values for a specific index in the palette.
 *
 * @param index The index in the palette to set the RGB values.
 * @param r The red component value.
 * @param g The green component value.
 * @param b The blue component value.
 */
void setRGB(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    milky_palettePalette[index][0] = r; // Set red component
    milky_palettePalette[index][1] = g; // Set green component
    milky_palettePalette[index][2] = b; // Set blue component

    // Update currentPalette for transition purposes
    currentPalette[index].r = r;
    currentPalette[index].g = g;
    currentPalette[index].b = b;
}

// Function to apply brightness factor safely and selectively
uint8_t applyBrightness(float colorValue, float brightnessFactor) {
    if (colorValue < MILKY_BRIGHTNESS_THRESHOLD) {
        float brightValue = colorValue * brightnessFactor;
        if (brightValue > MILKY_MAX_COLOR) {
            brightValue = MILKY_MAX_COLOR;
        }
        return (uint8_t)(brightValue);
    }
    return (uint8_t)(colorValue); // Return original value if above threshold
}

// Function to calculate the hue rotation matrix
void calculateHueRotationMatrix(float hue_deg, float matrix[3][3]) {
    float hue_rad = hue_deg * (M_PI / 180.0f); // Convert degrees to radians
    float cos_theta = cosf(hue_rad);
    float sin_theta = sinf(hue_rad);
    
    // Calculate the rotation matrix components
    matrix[0][0] = 0.213f + cos_theta * 0.787f - sin_theta * 0.213f;
    matrix[0][1] = 0.715f - cos_theta * 0.715f - sin_theta * 0.715f;
    matrix[0][2] = 0.072f - cos_theta * 0.072f + sin_theta * 0.928f;

    matrix[1][0] = 0.213f - cos_theta * 0.213f + sin_theta * 0.143f;
    matrix[1][1] = 0.715f + cos_theta * 0.285f + sin_theta * 0.140f;
    matrix[1][2] = 0.072f - cos_theta * 0.072f - sin_theta * 0.283f;

    matrix[2][0] = 0.213f - cos_theta * 0.213f - sin_theta * 0.787f;
    matrix[2][1] = 0.715f - cos_theta * 0.715f + sin_theta * 0.715f;
    matrix[2][2] = 0.072f + cos_theta * 0.928f + sin_theta * 0.072f;
}

// Function to shift hue using rotation matrix
void shiftHue(uint8_t r, uint8_t g, uint8_t b, float matrix[3][3], uint8_t* r_out, uint8_t* g_out, uint8_t* b_out) {
    float new_r = r * matrix[0][0] + g * matrix[0][1] + b * matrix[0][2];
    float new_g = r * matrix[1][0] + g * matrix[1][1] + b * matrix[1][2];
    float new_b = r * matrix[2][0] + g * matrix[2][1] + b * matrix[2][2];
    
    // Clamp the results to [0, 255]
    *r_out = (uint8_t)(fminf(fmaxf(new_r, 0.0f), 255.0f));
    *g_out = (uint8_t)(fminf(fmaxf(new_g, 0.0f), 255.0f));
    *b_out = (uint8_t)(fminf(fmaxf(new_b, 0.0f), 255.0f));
}

// Light Purple palette generation with hue adjustment using rotation matrix
void generateHuePalette(float hue_shift, float brightness_purple) {
    float rotationMatrix[3][3];
    calculateHueRotationMatrix(hue_shift, rotationMatrix);
    
    // Fill the first GRADIENT_SIZE colors with a gradient effect
    #pragma omp parallel for
    for (int a = 0; a < GRADIENT_SIZE; a++) {
        // Original RGB components before brightness adjustment
        float originalRed = (float)(a);                // Red increases linearly (0-63)
        float originalGreen = (float)(a * a) / 64.0f;  // Green increases quadratically (0-63)
        float originalBlue = (float)(sqrtf(a) * 8.0f); // Blue based on sqrt(a)*8 (0-56)
        
        // Apply brightness factor to each channel
        float brightRed = originalRed * brightness_purple;
        if (brightRed > MILKY_MAX_COLOR) brightRed = MILKY_MAX_COLOR;
        
        float brightGreen = originalGreen * brightness_purple;
        if (brightGreen > MILKY_MAX_COLOR) brightGreen = MILKY_MAX_COLOR;
        
        float brightBlue = originalBlue * brightness_purple;
        if (brightBlue > MILKY_MAX_COLOR) brightBlue = MILKY_MAX_COLOR;
        
        // Convert to uint8_t
        uint8_t r = (uint8_t)(brightRed);
        uint8_t g = (uint8_t)(brightGreen);
        uint8_t b = (uint8_t)(brightBlue);
        
        // Shift hue using rotation matrix
        uint8_t shifted_r, shifted_g, shifted_b;
        shiftHue(r, g, b, rotationMatrix, &shifted_r, &shifted_g, &shifted_b);
        
        // Set the shifted RGB values in the palette
        setRGB(a, shifted_r, shifted_g, shifted_b);
    }
    
    // Set the remaining colors to maximum intensity white
    #pragma omp parallel for
    for (int a = GRADIENT_SIZE; a < MILKY_PALETTE_SIZE; a++) {
        setRGB(a, MILKY_MAX_COLOR, MILKY_MAX_COLOR, MILKY_MAX_COLOR); // Pure white
    }
}

/**
 * Generates a random color palette based on predefined types.
 * The palette is filled with different gradient effects depending on the selected type.
 */
void generatePalette(void) {
    // Seed the random number generator with the current time to ensure different results each time
    srand((unsigned int)time(NULL));

    // Randomly select a palette type from 0 to 6
    int paletteType = rand() % 5;

    // Generate the palette based on the selected type
    switch (paletteType) {
        case 0: // "purple majik"
            #pragma omp parallel
            {
                // Fill the first 64 colors with a gradient effect
                #pragma omp for
                for (int a = 0; a < 64; a++) {
                    setRGB(a, a, a * a / 64, (uint8_t)(sqrtf(a) * 8));
                }

                // Set the remaining colors to maximum intensity white
                #pragma omp for
                for (int a = 64; a < MILKY_PALETTE_SIZE; a++) {
                    setRGB(a, MILKY_MAX_COLOR, MILKY_MAX_COLOR, MILKY_MAX_COLOR);
                }
            }
            break;

        case 1: // "golden sunburst"
            {
                const float brightness = 1.08f;

                // Fill the first 64 colors with a yellow gradient effect
                #pragma omp parallel
                {
                    #pragma omp for
                    for (int a = 0; a < 64; a++) {
                        // Red increases linearly
                        uint8_t red = a;

                        // Green increases quadratically for a smooth ramp-up
                        uint8_t green = (uint8_t)(a * a / 64);

                        // Blue remains minimal to maintain yellow (can set to 0 or a small value for variation)
                        uint8_t blue = 0; // Alternatively, use a small value like (uint8_t)(sqrtf(a) * 2) for slight variation

                        setRGB(a, applyBrightness(red, brightness), applyBrightness(green, brightness), blue);
                    }

                    // Set the remaining colors to maximum intensity yellow
                    #pragma omp for
                    for (int a = 64; a < MILKY_PALETTE_SIZE; a++) {
                        setRGB(a, MILKY_MAX_COLOR, MILKY_MAX_COLOR, 0); // Pure yellow
                    }
                }
            }
            break;

        case 2: // "green lantern ultima"
            #pragma omp parallel
            {
                // Fill the first 64 colors with yet another gradient effect
                #pragma omp for
                for (int a = 0; a < 64; a++) {
                    setRGB(a, fminf((uint8_t)(sqrtf(a) * 8), a), fmaxf(a, a+10), fminf(a * a / 64, a));
                }
                // Gradually fade the remaining colors to darkness
                #pragma omp for
                for (int a = 64; a < MILKY_PALETTE_SIZE; a++) {
                    uint8_t fadeValue = (uint8_t)((MILKY_PALETTE_SIZE - a) * MILKY_MAX_COLOR / (MILKY_PALETTE_SIZE - 92));
                    setRGB(a, fadeValue, fadeValue, fadeValue);
                }
            }
            break;

        /*
        case 3: // "christmas red/green" tunnel effect
            #pragma omp parallel
            {
                const float base_brightness = 0.8f; // Base brightness for green channel
                const float brightness_variation = 0.8f;   // Variation amplitude for green brightness
                const float frequency = 8.0f;               // Frequency of brightness variation

                // Precompute constant brightness adjustments for the second loop (fixed for red and blue)
                const uint8_t brightness_milky_r = applyBrightness(MILKY_MAX_COLOR / 9, base_brightness); // ~7 * 0.8 = 5
                const uint8_t brightness_milky_g = applyBrightness(MILKY_MAX_COLOR, base_brightness);     // 63 * 0.8 = 50

                // Execute the first loop serially as it has only 64 iterations

                #pragma omp for
                for (int a = 0; a < 64; a++) {
                    // Calculate the proportion of the current index (0.0 to 1.0)
                    float proportion = (float)a / 63.0f; // 0.0 - 1.0

                    // Compute dynamic brightness for the green channel using a sine wave
                    // This introduces smooth variations in green brightness across the palette
                    float dynamic_brightness = base_brightness + brightness_variation * sinf(proportion * frequency * M_PI);
                    
                    // Clamp the dynamic brightness to ensure it stays within [base_brightness_green - brightness_variation, base_brightness_green + brightness_variation]
                    if (dynamic_brightness < (base_brightness - brightness_variation)) {
                        dynamic_brightness = base_brightness - brightness_variation;
                    }
                    if (dynamic_brightness > (base_brightness + brightness_variation)) {
                        dynamic_brightness = base_brightness + brightness_variation;
                    }

                    // Compute red component with clamping between 0 and 255
                    uint8_t red = (uint8_t)(sqrtf((float)a) * 4.5f);
                    red = (red > 255) ? 255 : red;

                    // Apply base brightness to red and blue channels
                    uint8_t final_red = applyBrightness(red, dynamic_brightness);

                    // Set the RGB values in the palette
                    setRGB(a, final_red, 0, 0);
                }

                // Parallelize the second loop which has MILKY_PALETTE_SIZE - 64 iterations
                #pragma omp for
                for (int a = 64; a < MILKY_PALETTE_SIZE; a++) {
                    // Calculate the distance from the center of the palette
                    int center = MILKY_PALETTE_SIZE / 2;
                    int distance = abs(a - center);

                    // Normalize the distance to [0, 1]
                    float norm_distance = (float)distance / (float)(center);

                    // Calculate brightness factor: brighter at edges, darker towards center
                    // Ranges from 1.0 (edges) to 0.2 (center)
                    float brightness_factor = 0.2f + 0.8f * (1.0f - norm_distance);

                    // Apply brightness to precomputed milky colors
                    uint8_t r = applyBrightness(brightness_milky_r, brightness_factor);
                    uint8_t g = applyBrightness(brightness_milky_g, brightness_factor);

                    // Set the RGB values in the palette
                    setRGB(a, r, g, 0);
                }
            }
        break;
        */

        case 3:
            {
                // Precompute constants if possible
                const int lower_bound = 0;
                const int upper_bound = 64;
                const int remaining_start = 64;
                
                #pragma omp parallel
                {
                    // Handle the first loop (small loop) serially to avoid parallel overhead
                    #pragma omp single
                    {
                        for (int a = lower_bound; a < upper_bound; a++) {
                            // Precompute expressions
                            int a_squared_div_64 = (a * a) / 64;
                            float sqrt_a_times_8 = sqrtf((float)a) * 8.0f;
                            uint8_t value = (uint8_t)((sqrt_a_times_8 > 1.0f) ? sqrt_a_times_8 : 1.0f);
                            
                            setRGB(a, a_squared_div_64, a, value);
                        }
                    }

                    // Parallelize the second loop (larger loop)
                    #pragma omp for schedule(static)
                    for (int a = remaining_start; a < MILKY_PALETTE_SIZE; a++) {
                        // Optimize fminf and fmaxf usage
                        // Assuming MILKY_MAX_COLOR, 200, and 255 are constants
                        uint8_t r = (a < 100) ? a : 100;
                        uint8_t g = (MILKY_MAX_COLOR > 180) ? MILKY_MAX_COLOR : 180;
                        uint8_t b = (MILKY_MAX_COLOR > 255) ? MILKY_MAX_COLOR : 255;
                        
                        setRGB(a, r, g, b);
                    }
                }
            }
            break;

        default:
        case 4:
            {
                const float brightness_2 = 1.18f;

                // Fill the first 64 colors with a yellow gradient effect
                #pragma omp parallel for
                for (int a = 0; a < 64; a++) {
                    // Red increases linearly
                    uint8_t red = sqrtf(a);

                    // Green increases quadratically for a smooth ramp-up
                    uint8_t green = sqrtf(a);

                    // Blue remains minimal to maintain yellow (can set to 0 or a small value for variation)
                    uint8_t blue = (uint8_t)(a * a / 64) + a; // Alternatively, use a small value like (uint8_t)(sqrtf(a) * 2) for slight variation

                    setRGB(a, applyBrightness(fminf(a, 45), 1.1), applyBrightness(green, brightness_2*2.5), applyBrightness(blue, brightness_2*1.125));
                }

                // Set the remaining colors to maximum intensity yellow
                #pragma omp parallel for
                for (int a = 64; a < MILKY_PALETTE_SIZE; a++) {
                    setRGB(a, fminf(a-20, 5), MILKY_MAX_COLOR, MILKY_MAX_COLOR ); // Pure yellow
                }
            }
            break;
        /*

        case 6: // "green lantern I"
            {
                float hue_shift = 355.0f; 
                generateHuePalette(hue_shift, 1.10f);
            }
            break;

        default:
            // Fallback to a default palette if an unknown type is selected
            #pragma omp parallel for
            for (int a = 0; a < MILKY_PALETTE_SIZE; a++) {
                setRGB(a, 0, 0, 0);
            }
            break;
            */
    }

    // If a transition is ongoing, start transitioning to the new palette
    if (isTransitioning) {
        // Copy the new palette to targetPalette
        #pragma omp for schedule(static)
        for (int i = 0; i < MILKY_PALETTE_SIZE; i++) {
            targetPalette[i].r = milky_palettePalette[i][0];
            targetPalette[i].g = milky_palettePalette[i][1];
            targetPalette[i].b = milky_palettePalette[i][2];
        }
    }
}

/**
 * Initializes the palette transition system.
 * Must be called before any palette transitions occur.
 */
void initializePaletteTransition(int transitionSteps) {
    // Initialize currentPalette with the initial palette
    for (int i = 0; i < MILKY_PALETTE_SIZE; i++) {
        currentPalette[i].r = milky_palettePalette[i][0];
        currentPalette[i].g = milky_palettePalette[i][1];
        currentPalette[i].b = milky_palettePalette[i][2];
    }

    // Initialize transition parameters
    isTransitioning = 0;
    transitionStep = 0;
    totalTransitionSteps = (transitionSteps > 0) ? transitionSteps : 450; // Default to 300 if invalid
}

/**
 * Sets the number of steps for palette transitions.
 *
 * @param steps The number of steps over which the transition should occur.
 */
void setTransitionSteps(int steps) {
    if (steps > 0) {
        totalTransitionSteps = steps;
    }
}

/**
 * Starts a palette transition.
 * This function should be called when a palette change is triggered.
 */
void startPaletteTransition(void) {
    if (!isTransitioning) {
        // Copy currentPalette to oldPalette
        memcpy(oldPalette, currentPalette, sizeof(oldPalette));

        // Set targetPalette to the newly generated palette
        for (int i = 0; i < MILKY_PALETTE_SIZE; i++) {
            targetPalette[i].r = milky_palettePalette[i][0];
            targetPalette[i].g = milky_palettePalette[i][1];
            targetPalette[i].b = milky_palettePalette[i][2];
        }

        // Reset transition variables
        transitionStep = 0;
        isTransitioning = 1;
    }
}

/**
 * Applies the current palette to the canvas, updating each pixel's color.
 * Regenerates the palette if an energy spike is detected and sufficient time has elapsed.
 * Implements a smooth fade-in transition over specified steps when the palette changes.
 *
 * @param currentTime The current time in milliseconds.
 * @param canvas The canvas buffer to apply the palette to.
 * @param width The width of the canvas in pixels.
 * @param height The height of the canvas in pixels.
 */
void applyPaletteToCanvas(size_t currentTime, uint8_t *canvas, size_t width, size_t height) {
    size_t frameSize = width * height;

    // Check if it's time to regenerate the palette based on energy spikes and time elapsed
    if ((milky_energyEnergySpikeDetected && currentTime - milky_paletteLastPaletteInitTime > 10000) || milky_paletteLastPaletteInitTime == 0) {
        generatePalette();             // Reinitialize the palette
        startPaletteTransition();     // Start transitioning to the new palette
        milky_paletteLastPaletteInitTime = currentTime; // Update the last initialization time
    }
    
    // If transitioning, interpolate and apply the palette
    if (isTransitioning) {
        // Calculate blending factor 't' and '1 - t' outside the loop
        float t = (float)transitionStep / (float)totalTransitionSteps;
        if (t > 1.0f) t = 1.0f;
        float one_minus_t = 1.0f - t;

        // Parallelize the application using OpenMP
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < frameSize; i++) {
            uint8_t colorIndex = canvas[i * 4]; // Use the red channel as the intensity index

            // Branchless clamping using ternary operators
            colorIndex = (colorIndex >= MILKY_PALETTE_SIZE) ? (MILKY_PALETTE_SIZE - 1) : colorIndex;

            // Blend the colors on-the-fly without using a temporary blendedPalette
            uint8_t blended_r = (uint8_t)((one_minus_t * oldPalette[colorIndex].r) + (t * targetPalette[colorIndex].r));
            uint8_t blended_g = (uint8_t)((one_minus_t * oldPalette[colorIndex].g) + (t * targetPalette[colorIndex].g));
            uint8_t blended_b = (uint8_t)((one_minus_t * oldPalette[colorIndex].b) + (t * targetPalette[colorIndex].b));

            // Map the blended palette colors to the RGBA format in the canvas
            canvas[i * 4]     = blended_r; // R
            canvas[i * 4 + 1] = blended_g; // G
            canvas[i * 4 + 2] = blended_b; // B
            canvas[i * 4 + 3] = 255;       // A (fully opaque)
        }

        // Increment the transition step
        transitionStep++;
        if (transitionStep > totalTransitionSteps) {
            // Transition complete
            isTransitioning = 0;

            // Update the currentPalette to the targetPalette
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < MILKY_PALETTE_SIZE; i++) {
                currentPalette[i] = targetPalette[i];
            }
        }
    }
    else {
        // No transition, apply the current palette directly
        #pragma omp parallel for schedule(static)
        for (size_t i = 0; i < frameSize; i++) {
            uint8_t colorIndex = canvas[i * 4]; // Use the red channel as the intensity index

            // Branchless clamping using ternary operators
            colorIndex = (colorIndex >= MILKY_PALETTE_SIZE) ? (MILKY_PALETTE_SIZE - 1) : colorIndex;

            // Fetch the current palette colors directly
            uint8_t palette_r = currentPalette[colorIndex].r;
            uint8_t palette_g = currentPalette[colorIndex].g;
            uint8_t palette_b = currentPalette[colorIndex].b;

            // Map the current palette colors to the RGBA format in the canvas
            canvas[i * 4]     = palette_r; // R
            canvas[i * 4 + 1] = palette_g; // G
            canvas[i * 4 + 2] = palette_b; // B
            canvas[i * 4 + 3] = 255;       // A (fully opaque)
        }
    }
}