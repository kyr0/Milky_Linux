#include "signal.h"

volatile sig_atomic_t milky_sig_stop = 0;

void signal_handler(int signal)
{
  milky_sig_stop = 1;
  printf("Address of milky_sig_stop: %p\n", (void*)&milky_sig_stop);
}

void setup_signal_handlers(void) {
  signal(SIGINT, signal_handler);

  //SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
}