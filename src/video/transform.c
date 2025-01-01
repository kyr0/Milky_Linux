#include "transform.h"

// global variable to store the last theta value
static float milky_transformLastTheta = 0.0f;
static float milky_transformTargetTheta = 0.0f;

/**
 * Rotates the given frame buffer by a calculated angle and blends the result back into the frame.
 * Therefore, applies a smooth rotation transformation to the frame buffer using a temporary buffer.
 * The rotation angle is determined by the speed and angle parameters, and it smoothly transitions
 * towards a randomly set target angle. The rotated image is then blended back into the original frame
 * with a specified alpha value for smooth visual effects.
 *
 * @param timeFrame The time frame for the current rendering cycle.
 * @param tempBuffer A temporary buffer used for storing the rotated image.
 * @param frame The frame buffer (RGBA format) to be rotated.
 * @param speed The speed factor influencing the rotation angle.
 * @param angle The base angle for rotation.
 * @param width The width of the frame buffer in pixels.
 * @param height The height of the frame buffer in pixels.
 */
void rotate(float timeFrame, uint8_t *tempBuffer, uint8_t *frame, float speed, float angle, size_t width, size_t height) {
    // calculate the initial rotation angle based on speed and angle
    float theta = speed * angle;

    // if the difference between lastTheta and targetTheta is small, update targetTheta
    // this ensures that the rotation direction changes smoothly and randomly
    if (fabs(milky_transformLastTheta - milky_transformTargetTheta) < 0.01f) {
        // set a new targetTheta randomly between -45 and 45 degrees
        milky_transformTargetTheta = (rand() % 90 - 45) * (M_PI / 180.0f);
    }

    // interpolate theta towards targetTheta for smooth transition
    theta = milky_transformLastTheta + (milky_transformTargetTheta - milky_transformLastTheta) * 0.005f;
    milky_transformLastTheta = theta; // update lastTheta for the next frame

    // precompute sine and cosine of the current theta for rotation
    float sin_theta = sinf(theta), cos_theta = cosf(theta);
    // calculate the center of the frame for rotation
    float cx = width * 0.5f, cy = height * 0.5f;

    // clear the temporary buffer to prepare for the new rotated frame
    memset(tempBuffer, 0, width * height * 4);

    // iterate over each pixel in the frame
    #pragma omp parallel for collapse(2) schedule(static) default(none) shared(frame, tempBuffer, width, height, sin_theta, cos_theta, cx, cy)   
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            // translate coordinates to the center for rotation
            float xt = x - cx, yt = y - cy;
            // apply rotation transformation
            int src_xi = (int)(cos_theta * xt - sin_theta * yt + cx);
            int src_yi = (int)(sin_theta * xt + cos_theta * yt + cy);

            // check if the source pixel is within bounds
            if (src_xi >= 0 && src_xi < (int)width && src_yi >= 0 && src_yi < (int)height) {
                // calculate source and destination indices in the buffer
                size_t src_index = (src_yi * width + src_xi) * 4;
                size_t dst_index = (y * width + x) * 4;
                
                // copy the pixel from the source to the destination in the temp buffer

                // Fallback for non-NEON platforms
                memcpy(&tempBuffer[dst_index], &frame[src_index], 4);
            }
        }
    }

    float alpha = 0.7f;
    #pragma omp parallel for
    for (size_t i = 0; i < width * height * 4; i++) {
        frame[i] = (uint8_t)(frame[i] * (1 - alpha) + tempBuffer[i] * alpha);
    }
}

/**
 * Scales the image in the frame buffer by the specified `scale` factor,
 * using the `tempBuffer` as a temporary storage for the scaled image.
 *
 * @param frame      The frame buffer containing the image to be scaled (RGBA format).
 * @param tempBuffer A temporary buffer used to store the scaled image.
 * @param scale      The scale factor to apply to the image.
 * @param width      The width of the frame in pixels.
 * @param height     The height of the frame in pixels.
 */
void scale(
    unsigned char *frame,      // Frame buffer (RGBA format)
    unsigned char *tempBuffer, // Temporary buffer for scaled image
    float scale,               // Scale factor
    size_t width,              // Frame width
    size_t height              // Frame height
) {
    size_t frameSize = width * height * 4;
    memset(tempBuffer, 0, frameSize);

    // Calculate center and inverse scale for optimization
    float centerX = width * 0.5f;
    float centerY = height * 0.5f;
    float inv_scale = 1.0f / scale;

    // Loop through each pixel in the target buffer
    #pragma omp parallel for collapse(2) schedule(static) default(none) shared(frame, tempBuffer, width, height, inv_scale, centerX, centerY)
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            // Apply inverse scaling to find original positions
            float originalX = (x - centerX) * inv_scale + centerX;
            float originalY = (y - centerY) * inv_scale + centerY;

            // Round to nearest pixel in the source image
            int srcX = (int)roundf(originalX);
            int srcY = (int)roundf(originalY);

            // Only copy if source coordinates are within bounds
            if (srcX >= 0 && srcX < (int)width && srcY >= 0 && srcY < (int)height) {
                size_t srcIndex = (srcY * width + srcX) * 4;
                size_t dstIndex = (y * width + x) * 4;

                // Copy the RGBA values from the source to the destination

                // Non-NEON fallback
                tempBuffer[dstIndex + 0] = frame[srcIndex + 0]; // Red
                tempBuffer[dstIndex + 1] = frame[srcIndex + 1]; // Green
                tempBuffer[dstIndex + 2] = frame[srcIndex + 2]; // Blue
                tempBuffer[dstIndex + 3] = frame[srcIndex + 3]; // Alpha
            }
        }
    }

    // copy the scaled image back to the original frame buffer
    memcpy(frame, tempBuffer, frameSize);
}
