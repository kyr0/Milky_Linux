#include "tunnel.h"

static clock_t milky_tunnelLastPaletteInitTime = 0;


/**
 * Draws a pixel on the screen with specified RGBA values.
 *
 * @param screen The screen buffer.
 * @param width The width of the screen in pixels.
 * @param height The height of the screen in pixels.
 * @param x The x-coordinate of the pixel.
 * @param y The y-coordinate of the pixel.
 * @param r The red component (0-255).
 * @param g The green component (0-255).
 * @param b The blue component (0-255).
 * @param a The alpha component (0-255).
 */
void drawPixel(uint8_t *screen, size_t width, size_t height, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // Ensure coordinates are within bounds
    if (x < 0 || x >= (int)width || y < 0 || y >= (int)height)
        return;

    size_t index = (y * width + x) * 4; // Assuming RGBA format
    screen[index]     = r; // R
    screen[index + 1] = g; // G
    screen[index + 2] = b; // B
    screen[index + 3] = a; // A
}

/**
 Renders a tunnel circle (a circle that gets faded into a tunnely visualiztation) 

 @param timeFrame The current time frame for animation (used to create variation in chaser movement).
 @param screen    The screen buffer to render the chasers on.
 @param speed     The speed factor for chaser movement (higher values result in faster movement).
 @param count     The number of chasers to render.
 @param width     The width of the screen buffer in pixels.
 @param height    The height of the screen buffer in pixels.
 @param seed      The seed value for random number generation (used to ensure reproducibility).
*/
void renderTunnelCircle(size_t currentTime, float timeFrame, uint8_t *screen, int radius, unsigned int count, size_t width, size_t height, unsigned int seed, int thickness) {

    // check if it's time to regenerate the palette based on energy spikes and time elapsed
     if ((milky_energyEnergySpikeDetected && currentTime - milky_tunnelLastPaletteInitTime > 20 /** ms */) || milky_tunnelLastPaletteInitTime == 0) {
    // Calculate center coordinates
        float centerX = width / 2.0f;
        float centerY = height / 2.0f;

       // Validate thickness
        if (thickness <= 0) {
            fprintf(stderr, "Error: Thickness must be positive.\n");
            return;
        }

        // Validate radius and adjust if necessary to fit within the screen
        if (radius + thickness / 2 > (width > height ? width / 2 : height / 2)) {
            fprintf(stderr, "Warning: Circle with radius %d and thickness %d may exceed screen bounds. Adjusting radius.\n", radius, thickness);
            radius = (width > height ? width / 2 : height / 2) - thickness / 2;
            if (radius < 0) radius = 0;
        }

        // Precompute thickness boundaries
        float innerRadius = (float)radius - (float)thickness / 2.0f;
        float outerRadius = (float)radius + (float)thickness / 2.0f;

        // Define circle color (e.g., white)
        uint8_t circleR = MILKY_MAX_COLOR;
        uint8_t circleG = MILKY_MAX_COLOR;
        uint8_t circleB = MILKY_MAX_COLOR;
        uint8_t circleA = MILKY_MAX_COLOR;

        // Parallelize the loop over each pixel
        #pragma omp parallel for schedule(static) collapse(2)
        for (size_t y = 0; y < height; y++) {
            for (size_t x = 0; x < width; x++) {
                // Compute distance from the center
                float dx = (float)x - centerX;
                float dy = (float)y - centerY;
                float distance = sqrtf(dx * dx + dy * dy);

                // Check if the pixel is within the thickness range
                if (distance >= innerRadius && distance <= outerRadius) {
                    drawPixel(screen, width, height, (int)x, (int)y, circleR, circleG, circleB, circleA);
                }
            }
        }

        milky_tunnelLastPaletteInitTime = currentTime; // update the last initialization time
    }
}
