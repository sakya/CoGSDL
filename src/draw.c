#include "CoGSDL.h"

int draw_loop() {
  printf("##Start rendering loop\n");

  while (_G.quit == 0) {
      pollevent();
      handle_key();
  }
  return 0;
}