#include "video.h"

// flag to check if lastFrame is initialized
static int milky_videoIsLastFrameInitialized = 0;

// global variable to store the previous time
static size_t milky_videoPrevTime = 0;
static size_t milky_videoPrevFrameSize = 0;
static float milky_videoSpeedScalar = 0.01f;

// static buffer to be reused across render calls
static uint8_t *milky_videoTempBuffer = NULL;
static uint8_t *milky_videoPrevFrame = NULL;
static size_t milky_videoTempBufferSize = 0;
static size_t milky_videoLastCanvasWidthPx = 0;
static size_t milky_videoLastCanvasHeightPx = 0;

// remember the last time we reacted to energy spiked
static clock_t milky_energyLastChangeInitTime = 0;

/**
 * Renders one visual frame based on audio waveform and spectrum data.
 *
 * @param frame           Canvas frame buffer (RGBA format).
 * @param canvasWidthPx   Canvas width in pixels.
 * @param canvasHeightPx  Canvas height in pixels.
 * @param waveform        Waveform data array, 8-bit unsigned integers, 576 samples.
 * @param spectrum        Spectrum data array.
 * @param waveformLength  Length of the waveform data array.
 * @param spectrumLength  Length of the spectrum data array.
 * @param bitDepth        Bit depth of the rendering.
 * @param presetsBuffer   Preset data.
 * @param speed           Speed factor for the rendering.
 * @param currentTime     Current time in milliseconds.
 * @param sampleRate      Waveform sample rate (samples per second)
 */
void render(
               uint8_t *frame,
               size_t canvasWidthPx,
               size_t canvasHeightPx,
               const uint8_t *waveform,
               const uint8_t *spectrum,
               size_t waveformLength,
               size_t spectrumLength,
               uint8_t bitDepth,
               float *presetsBuffer,
               float speed,
               size_t currentTime,
               size_t sampleRate
           ) {
                // TODO: find out why the compiler corrupts the stack
                
                speed = 0.03323f; // NICE; great factor value found!

               if (waveformLength == 0 || spectrumLength == 0) {
                   fprintf(stderr, "No waveform or spectrum data provided\n");
                   return;
               }

               // Pre-calculate frame size and check memory requirements once
               const size_t frameSize = canvasWidthPx * canvasHeightPx * 4;

               if (milky_videoPrevFrameSize == 0) {
                   milky_videoPrevFrameSize = frameSize;
               }

               // Only update memory if canvas size changes
               reserveAndUpdateMemory(canvasWidthPx, canvasHeightPx, frame, frameSize);
             
             // Process emphasized waveform
             float emphasizedWaveform[waveformLength];
             smoothBassEmphasizedWaveform(waveform, waveformLength, emphasizedWaveform, canvasWidthPx, 0.65f);
     

                //fprintf(stdout, "milky_videoPrevTime: %zu, currentTime: %zu deltaTime: %f\n", milky_videoPrevTime, currentTime, (float)(currentTime - milky_videoPrevTime));
                float deltaTime = (float)(currentTime - milky_videoPrevTime);
                // Pre-calculate time frame and constants outside of per-pixel rendering for efficiency
                const float timeFrame = (milky_videoPrevTime == 0) ? 0.01f : deltaTime / 1000.0f;

               // Optimize buffer copies for NEON by vectorizing the copying operation
               // Copy the previous frame to a temporary buffer, minimizing redundant operations
               if (!milky_videoIsLastFrameInitialized) {
                   clearFrame(frame, frameSize);
                   clearFrame(milky_videoPrevFrame, milky_videoPrevFrameSize);
                   milky_videoIsLastFrameInitialized = 1;
               } else {
                   milky_videoSpeedScalar += speed * 2;

                   //fprintf(stdout, "speedscalar: %f  speed: %f  ", milky_videoSpeedScalar);
              // renderChasers(milky_videoSpeedScalar/4, frame, speed , 1, canvasWidthPx, canvasHeightPx, 88, 1);


                   blurFrame(milky_videoPrevFrame, frameSize, 2, 0.95f);

                   preserveMassFade(milky_videoPrevFrame, milky_videoTempBuffer, frameSize);
                   
                   #ifdef __ARM_NEON__
                   size_t i = 0;
                   for (; i + 16 <= frameSize; i += 16) {
                       uint8x16_t prevFrameData = vld1q_u8(&milky_videoPrevFrame[i]);
                       vst1q_u8(&milky_videoTempBuffer[i], prevFrameData);
                       vst1q_u8(&frame[i], prevFrameData);
                   }
                   for (; i < frameSize; i++) {
                       milky_videoTempBuffer[i] = milky_videoPrevFrame[i];
                       frame[i] = milky_videoTempBuffer[i];
                   }
                   #else
                   memcpy(milky_videoTempBuffer, milky_videoPrevFrame, frameSize);
                   memcpy(frame, milky_videoTempBuffer, frameSize);
                   #endif
               }

               milky_videoPrevTime = currentTime;
               // Apply color palette for visual effects
               applyPaletteToCanvas(currentTime, frame, canvasWidthPx, canvasHeightPx);
               //renderChasers(milky_videoSpeedScalar, frame, speed  * 20, 1, canvasWidthPx, canvasHeightPx, 44, 2);

               //renderTunnelCircle(currentTime, milky_videoSpeedScalar, frame, 50, 1, canvasWidthPx, canvasHeightPx, 42, 2);

               // Render waveform with multiple emphasis levels
               //renderWaveformSimple(timeFrame, frame, canvasWidthPx, canvasHeightPx, emphasizedWaveform, waveformLength, 0.85f, 1, 1);
               renderWaveformSimple(timeFrame, frame, canvasWidthPx, canvasHeightPx, emphasizedWaveform, waveformLength, 5.0f, 0, 0);
               renderWaveformSimple(timeFrame, frame, canvasWidthPx, canvasHeightPx, emphasizedWaveform, waveformLength, 0.0f, 1, 0);
     
               detectEnergySpike(waveform, spectrum, waveformLength, spectrumLength, sampleRate);

               renderChasers(milky_videoSpeedScalar, frame, speed  * 20, 2, canvasWidthPx, canvasHeightPx, 42, 2);
               

               if (bitDepth < 32) {
                   reduceBitDepth(frame, frameSize, bitDepth);
               }
                    
                // Seed the random number generator with the current time
                srand((unsigned int)time(NULL));

                // Generate a random float between 0.2 and 0.5
                float rotationAngle = ((float)rand() / (float)RAND_MAX) * (0.5f - 0.2f) + 0.2f;

                if (rotationAngle > 0 && rotationAngle < 0.05) rotationAngle = 0.05f;
                if (rotationAngle < 0 && rotationAngle > -0.05) rotationAngle = -0.05f;

                float zoomFactor = ((float)rand() / (float)RAND_MAX) * (0.2f - 0.1f) + 0.1f;

                if (zoomFactor > 0 && zoomFactor < 0.05) zoomFactor = 0.05f;
                if (zoomFactor < 0 && zoomFactor > -0.05) zoomFactor = -0.05f;

               // check if it's time to regenerate the palette based on energy spikes and time elapsed
                if ((milky_energyEnergySpikeDetected && currentTime - milky_energyLastChangeInitTime > 10 * 1000) || milky_energyLastChangeInitTime == 0) {
                    rotationAngle = -rotationAngle;
                    zoomFactor = -zoomFactor;
                    // invert (make it minus, rotate in different direction)
                    milky_energyLastChangeInitTime = currentTime; // update the last initialization time
                }
     
               
               // Rotate and scale effects with NEON-optimized copy
               rotate(timeFrame, milky_videoTempBuffer, frame, 0.02 * currentTime, rotationAngle, canvasWidthPx, canvasHeightPx);
               scale(frame, milky_videoTempBuffer, 1.15f, canvasWidthPx, canvasHeightPx);
     
               // Copy the final frame to the previous frame buffer
               #ifdef __ARM_NEON__
               size_t j = 0;
               for (; j + 16 <= frameSize; j += 16) {
                   vst1q_u8(&milky_videoPrevFrame[j], vld1q_u8(&frame[j]));
               }
               for (; j < frameSize; j++) {
                   milky_videoPrevFrame[j] = frame[j];
               }
               #else
               memcpy(milky_videoPrevFrame, frame, frameSize);
               #endif

               // Update frame size to match current frame
               milky_videoPrevFrameSize = frameSize;
           }


/**
 * Reserves and updates memory dynamically for rendering based on canvas size.
 *
 * @param canvasWidthPx  Canvas width in pixels.
 * @param canvasHeightPx Canvas height in pixels.
 * @param frame          Frame buffer to be updated.
 * @param frameSize      Size of the frame buffer.
 */
void reserveAndUpdateMemory(size_t canvasWidthPx, size_t canvasHeightPx, uint8_t *frame, size_t frameSize) {
    // check if the canvas size has changed and reinitialize buffers if necessary
    if (canvasWidthPx != milky_videoLastCanvasWidthPx || canvasHeightPx != milky_videoLastCanvasHeightPx) {
        clearFrame(frame, frameSize);
        if (milky_videoPrevFrame) {
            free(milky_videoPrevFrame);
        }
        milky_videoPrevFrame = (uint8_t *)malloc(frameSize);
        if (!milky_videoPrevFrame) {
            fprintf(stderr, "Failed to allocate prevFrame buffer\n");
            return;
        }
        milky_videoLastCanvasWidthPx = canvasWidthPx;
        milky_videoLastCanvasHeightPx = canvasHeightPx;

        // free and reallocate the temporary buffer if canvas size changes
        if (milky_videoTempBuffer) {
            free(milky_videoTempBuffer);
        }
        milky_videoTempBuffer = (uint8_t *)malloc(frameSize);
        if (!milky_videoTempBuffer) {
            fprintf(stderr, "Failed to allocate temporary buffer\n");
            return;
        }
        milky_videoTempBufferSize = frameSize;
    }

    // allocate or reuse the prevFrame buffer
    if (!milky_videoPrevFrame || milky_videoTempBufferSize < frameSize) {
        if (milky_videoPrevFrame) {
            free(milky_videoPrevFrame);
        }
        milky_videoPrevFrame = (uint8_t *)malloc(frameSize);
        if (!milky_videoPrevFrame) {
            fprintf(stderr, "Failed to allocate prevFrame buffer\n");
            return;
        }
    }

    // allocate or reuse the temporary buffer
    if (!milky_videoTempBuffer || milky_videoTempBufferSize < frameSize) {
        if (milky_videoTempBuffer) {
            free(milky_videoTempBuffer);
        }
        milky_videoTempBuffer = (uint8_t *)malloc(frameSize);
        if (!milky_videoTempBuffer) {
            fprintf(stderr, "Failed to allocate temporary buffer\n");
            return;
        }
        milky_videoTempBufferSize = frameSize;
    }
}
