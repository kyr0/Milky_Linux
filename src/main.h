#ifndef MAIN_H
#define MAIN_H

#include <stdio.h>
#include <omp.h>

#include "video.h"
#include "audio/capture.h"

void process_audio_chunk(const uint8_t *waveform, size_t waveformLength, size_t spectrumLength, const uint8_t *spectrum);

int main(int argc, char *argv[]);

#endif // MAIN_H