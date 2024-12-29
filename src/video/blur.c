#include "blur.h"

/*
// not in use yet
void boxBlurAndPerspective(
    uint8_t *frame, 
    uint8_t *prevFrame, 
    size_t canvasWidthPx, 
    size_t canvasHeightPx
) {
    float centerX = canvasWidthPx * 0.5f;
    float centerY = canvasHeightPx * 0.5f;
    float maxDistance = sqrtf(centerX * centerX + centerY * centerY);

    // precompute perspective factors for each possible distance
    float perspectiveFactors[canvasWidthPx / 2 + 1];
    for (size_t i = 0; i <= canvasWidthPx / 2; ++i) {
        perspectiveFactors[i] = 0.5f + 0.5f * (1.0f - ((float)i / (canvasWidthPx / 2)));
    }

    // apply a simple box blur for smoothing
    for (size_t y = 1; y < canvasHeightPx - 1; y++) {
        for (size_t x = 1; x < canvasWidthPx - 1; x++) {
            size_t index = (y * canvasWidthPx + x) * 4;
            uint8_t *pixel = &prevFrame[index];

            // calculate distance from the center
            float dx = (float)(x - centerX);
            float dy = (float)(y - centerY);
            float distanceSquared = dx * dx + dy * dy;

            // use precomputed perspective factor
            float perspectiveFactor = perspectiveFactors[(size_t)(sqrtf(distanceSquared))];

            // apply a softer perspective fade to R, G, B channels
            for (int channel = 0; channel < 3; ++channel) {
                pixel[channel] = (uint8_t)(pixel[channel] * perspectiveFactor);
            }

            // use SIMD to apply a simple box blur by averaging with immediate neighboring pixels
            v128_t sumR = wasm_i32x4_splat(0);
            v128_t sumG = wasm_i32x4_splat(0);
            v128_t sumB = wasm_i32x4_splat(0);

            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    size_t neighborIndex = ((y + i) * canvasWidthPx + (x + j)) * 4;
                    v128_t neighborPixel = wasm_v128_load(&frame[neighborIndex]);

                    sumR = wasm_i32x4_add(sumR, wasm_i32x4_splat(wasm_i8x16_extract_lane(neighborPixel, 0)));
                    sumG = wasm_i32x4_add(sumG, wasm_i32x4_splat(wasm_i8x16_extract_lane(neighborPixel, 1)));
                    sumB = wasm_i32x4_add(sumB, wasm_i32x4_splat(wasm_i8x16_extract_lane(neighborPixel, 2)));
                }
            }

            pixel[0] = wasm_i32x4_extract_lane(sumR, 0) / 9;
            pixel[1] = wasm_i32x4_extract_lane(sumG, 0) / 9;
            pixel[2] = wasm_i32x4_extract_lane(sumB, 0) / 9;
        }
    }
}

// not in use yet
void boxBlurAndPerspective2(
    uint8_t *frame, 
    uint8_t *prevFrame, 
    size_t canvasWidthPx, 
    size_t canvasHeightPx
) {
  float centerX = canvasWidthPx * 0.5f;
  float centerY = canvasHeightPx * 0.5f;
  float maxDistance = sqrtf(centerX * centerX + centerY * centerY);

  // precompute perspective factors for each possible distance
  float perspectiveFactors[canvasWidthPx / 2 + 1];
  for (size_t i = 0; i <= canvasWidthPx / 2; ++i) {
      perspectiveFactors[i] = 0.5f + 0.5f * (1.0f - ((float)i / (canvasWidthPx / 2)));
  }

  // combine perspective effect and anti-aliasing in a single loop
  for (size_t y = 1; y < canvasHeightPx - 1; y++) {
      for (size_t x = 1; x < canvasWidthPx - 1; x++) {
          size_t index = (y * canvasWidthPx + x) * 4;
          uint8_t *pixel = &frame[index];

          // calculate distance from the center
          float dx = (float)(x - centerX);
          float dy = (float)(y - centerY);
          float distanceSquared = dx * dx + dy * dy;

          // use precomputed perspective factor
          float perspectiveFactor = perspectiveFactors[(size_t)(sqrtf(distanceSquared))];

          // apply a softer perspective fade to R, G, B channels
          for (int channel = 0; channel < 3; ++channel) {
              pixel[channel] = (uint8_t)(pixel[channel] * perspectiveFactor);
          }

          // apply anti-aliasing by averaging with neighboring pixels
          uint8_t avgR = (pixel[0] +
                          frame[index - 4] +
                          frame[index + 4] +
                          frame[index - canvasWidthPx * 4] +
                          frame[index + canvasWidthPx * 4]) / 5;
          uint8_t avgG = (pixel[1] +
                          frame[index - 3] +
                          frame[index + 5] +
                          frame[index - canvasWidthPx * 4 + 1] +
                          frame[index + canvasWidthPx * 4 + 1]) / 5;
          uint8_t avgB = (pixel[2] +
                          frame[index - 2] +
                          frame[index + 6] +
                          frame[index - canvasWidthPx * 4 + 2] +
                          frame[index + canvasWidthPx * 4 + 2]) / 5;
          pixel[0] = avgR;
          pixel[1] = avgG;
          pixel[2] = avgB;
      }
  }
}
*/

/**
 * Iterates over each pixel in the given frame and applies a fade effect
 * to the red, green, and blue channels. The fade effect is achieved by multiplying each
 * channel value by 0.90, which slightly reduces the intensity of the colors. This results
 * in a smoother transition between frames, helping to create a more visually appealing
 * blur effect. The alpha channel is not modified to maintain the transparency level.
 *
 * @param prevFrame A pointer to the frame buffer containing pixel data in RGBA format.
 * @param frameSize The total size of the frame buffer in bytes.
 */
void blurFrame(uint8_t *prevFrame, size_t frameSize, size_t step, float factor) {
   size_t numPixels = frameSize / step;

    #pragma omp parallel for schedule(static) default(none) shared(prevFrame, numPixels, step, factor)
    for (size_t i = 0; i < numPixels; i++) {
        size_t idx = i * step;
        uint8_t *pixel = &prevFrame[idx];
        
        // Process RGB channels
        pixel[0] = (uint8_t)(pixel[0] * factor);  // Red
        pixel[1] = (uint8_t)(pixel[1] * factor);  // Green
        pixel[2] = (uint8_t)(pixel[2] * factor);  // Blue
        // Alpha channel (pixel[3]) remains unchanged
    }
}

void preserveMassFade(uint8_t *prevFrame, uint8_t *frame, size_t frameSize) {
  // Ensure frameSize is a multiple of 4
    size_t numPixels = frameSize / 4;

    #pragma omp parallel for schedule(static)
    for (long i = 0; i < numPixels; i++) {
        size_t idx = i * 4;
        // Unroll the inner loop for better performance
        uint8_t prevValue0 = prevFrame[idx + 0];
        frame[idx + 0] = (prevValue0 + (uint8_t)(prevValue0 * 85)) >> 1;

        uint8_t prevValue1 = prevFrame[idx + 1];
        frame[idx + 1] = (prevValue1 + (uint8_t)(prevValue1 * 85)) >> 1;

        uint8_t prevValue2 = prevFrame[idx + 2];
        frame[idx + 2] = (prevValue2 + (uint8_t)(prevValue2 * 85)) >> 1;
        // Skip idx + 3 (Alpha channel)
    }
}
