#include "CoGSDL.h"

CoGSDL _G = {
    640,  // DISPLAY_WIDTH
    480,  // DISPLAY_HEIGHT
    NULL, // Fill the remaining fields with NULL
};

void quit(int exitcode) {
  dispose();
  exit(exitcode);
}

int main() {
  int ret = EXIT_SUCCESS;
  signal(SIGTERM, quit);
  signal(SIGSEGV, quit);

  if (init() < 0) {
    printf("init failed\n");
    ret = EXIT_FAILURE;
  }

  if (draw_loop() < 0) {
    printf("draw_loop failed\n");
    ret = EXIT_FAILURE;
  }

  dispose();
  return ret;
}