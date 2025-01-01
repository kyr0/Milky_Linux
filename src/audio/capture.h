#ifndef CAPTURE_H
#define CAPTURE_H

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <time.h>

// Include the PulseAudio library 
#include <pulse/pulseaudio.h>

// For rendering using GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Include KISS FFT
#include "./kiss_fft/kiss_fft.h"

// Constants
#define BUFFER_COUNT 2
#define WIDTH 1920
#define HEIGHT 1080

// Structures
typedef struct {
    uint8_t *data;
    int ready; // 0: not ready, 1: ready
} FrameBuffer;

// Callback (set from main.c)
typedef void (*process_audio_chunk_callback_t)(
    const uint8_t *waveform,
    size_t sampleLength,
    size_t spectrumLength,
    const uint8_t *spectrum
);

// PulseAudio struct
typedef struct {
    pa_mainloop *mainloop;
    pa_mainloop_api *mainloop_api;
    pa_context *context;
    pa_signal_event *signal;
    pa_stream *stream; 
} PulseAudio;

// Function Declarations
void exit_signal_callback(pa_mainloop_api *m, pa_signal_event *e, int sig, void *userdata);
void context_state_callback(pa_context *c, void *userdata);
void server_info_callback(pa_context *c, const pa_server_info *i, void *userdata);
void sink_info_callback(pa_context *c, const pa_sink_info *i, int eol, void *userdata);
void subscribe_callback(pa_context *c, pa_subscription_event_type_t type, uint32_t idx, void *userdata);
void stream_read_callback(pa_stream *s, size_t length, void *userdata);

int pulse_initialize(PulseAudio *pa);
int pulse_run(PulseAudio *pa);
void pulse_destroy(PulseAudio *pa);
void pulse_quit(PulseAudio *pa, int ret);

int run();

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
static void window_close_callback(GLFWwindow* window);

// FFT related
void calculate_spectrum(const uint8_t *waveform, size_t sample_count, uint8_t *spectrum);

// Timer functions
void initialize_timer();
float performance_now();

// OpenGL and Rendering
void initialize_glfw();
void cleanup_glfw();

// Rendering thread function
void* render_thread_func(void *arg);

// PulseAudio thread function
void* pulse_thread_func(void *arg);

#endif // CAPTURE_H
