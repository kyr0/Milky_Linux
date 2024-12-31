#include "main.h"

// current rendered frame (buffer allocated globally)
//uint8_t *frame = NULL;

void process_audio_chunk(const uint8_t *waveform, size_t waveformLength, size_t spectrumLength, const uint8_t *spectrum) {

/*
    printf("First sample: %d\n", waveform[0]);
    printf("First spectrum sample: %d\n", spectrum[0]);
    
    printf("main.c: waveformLength: %zu\n", waveformLength);
    printf("main.c: spectrumLength: %zu\n", spectrumLength);

    // now we only need to call the VIDEO function
    // and render the framebuffer onto a surface that can handle
    // framebuffer data -- you'll then see the current playback sound
    // being beautifully visualized!
    // We'll use SDL to render the framebuffer on a new window that we're 
    // going to create.

    // simple demo: width * height * count of values we have to reserve 
    // memory for because a framebuffer wants Red, Green, Blue, Alpha 
    // cannel values per pixel.

    size_t canvasWidthPx = 1440;
    size_t canvasHeightPx = 900;
    size_t frameSize = canvasWidthPx * canvasHeightPx * 4; // RGBA

    // if frame buffer isn't allocated yet, do it once during initialization
    if (frame == NULL) {
        // allocate memory once
        frame = (uint8_t *)malloc(frameSize); 
        if (!frame) {
            fprintf(stderr, "Failed to allocate memory for the frame buffer.\n");
        }
        memset(frame, 0, frameSize);
    }

    float *presetsBuffer = NULL; 
    float speed = 0.03f;
    size_t sampleRate = 44100;
    size_t bitDepth = 16;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    // convert to milliseconds
    size_t currentTime = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    
    // rendering the audio 
    render(
        frame,
        canvasWidthPx,
        canvasHeightPx,
        waveform,
        spectrum,
        waveformLength,
        spectrumLength,
        bitDepth,
        presetsBuffer,
        speed,
        currentTime,
        sampleRate
    );

    printf("First pixel RGBA: %d %d %d %d\n", frame[0], frame[1], frame[2], frame[3]);
    */
} 

int main(int argc, char *argv[]) {
    omp_set_dynamic(0); // Disable dynamic thread adjustment
    omp_set_num_threads(8); // Set to desired number of threads
    omp_set_nested(8); // Disable nested parallelism to prevent thread oversubscription
    
    //setup_signal_handlers();

     //set_audio_chunk_callback(process_audio_chunk);

    // something weird is going on with the signal;
    // it has a value that was never assigned;
    // either I'm doing something terribly wrong 
    // and the memory is corrupted, or 
    // there is some undefined behaviour 

/*
    printf("Starting Milky...\n"); // works
    printf("Args: %d\n", argc); // works

    for (int i = 0; i < argc; i++) {
        printf("Argument %d: %s\n", i, argv[i]); // verified
    }
    */

    int ret = run(); // quickly testing creating a PulseAudio object
    
     printf("Press Ctrl+C to stop the program.\n");

    /*
    size_t x = 0;
    while (true) {
        x++; // Simulate work
        if (milky_sig_stop) { // Check the flag
            printf("\nCtrl+C detected. Exiting...\n");
            break; // Exit the loop
        }
        // Optionally, sleep briefly to reduce CPU usage
        usleep(10000); // 10ms
    }
    */
    

    return ret; // exit without error
}