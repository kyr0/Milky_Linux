#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <unistd.h> // nanosleep() for ctrl+c to work (POSIX signals don't come through otherwise)

// include the PulseAudio library 
#include <pulse/pulseaudio.h>

// for rendering using GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// include KISS FFT
#include "./kiss_fft/kiss_fft.h"

typedef struct {
    uint8_t r; // Red channel
    uint8_t g; // Green channel
    uint8_t b; // Blue channel
} Color;

// callback (set from main.c)
typedef void (*process_audio_chunk_callback_t)(
    const uint8_t *waveform,
    size_t sampleLength,
    size_t spectrumLength,
    const uint8_t *spectrum
);

// we need to typedef a struct because we want to get hold of some references
// (aka. pointers into memory...) -- but we don't want to get lost in a million
// variables..
typedef struct {
    pa_mainloop *mainloop;
    pa_mainloop_api *mainloop_api;
    pa_context *context;
    pa_signal_event *signal;
    pa_stream *stream; 
} PulseAudio;

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

// FFT related
void calculate_spectrum(const uint8_t *waveform, size_t sample_count, uint8_t *spectrum);

typedef struct {
    Color color;
    int count;
} ColorCount;


#define MAX_COLORS 256 // Adjust based on expected unique colors
#define MAX_PIXELS 1000 // Max number of pixels to analyze in the first row


void initialize_glfw();
void cleanup_glfw();
static void render_frame(uint8_t *frame);

int color_equals(Color c1, Color c2);
void add_color(Color c);
Color find_most_frequent_color();
Color analyze_first_line(uint8_t *frame, int width);

void initialize_timer();
float performance_now();

// Rendering relates
//int initialize_sdl_resources(size_t canvasWidthPx, size_t canvasHeightPx);
//void cleanup_sdl_resources();

#endif // CAPTURE_H