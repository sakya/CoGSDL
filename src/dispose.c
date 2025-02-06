#include "CoGSDL.h"

void dispose() {
  printf("##Start dispose\n");

  if (_G.renderer != NULL) {
    printf("\tdisposing renderer...\n");
    SDL_DestroyRenderer(_G.renderer);
  }

  if (_G.window != NULL) {
    printf("\tdisposing window...\n");
    SDL_DestroyWindow(_G.window);
  }

  if (_G.defaultFont != NULL) {
    printf("\tClosing font...\n");
    TTF_CloseFont(_G.defaultFont);
  }

  TTF_Quit();
  SDL_Quit();
  printf("##Finish destory\n\n");
}