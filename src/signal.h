#ifndef SIGNAL_H
#define SIGNAL_H

#include <signal.h>
#include <stdio.h>
#include <SDL2/SDL.h>

extern volatile sig_atomic_t milky_sig_stop;

void signal_handler(int signal);

void setup_signal_handlers(void);

#endif // SIGNAL_H