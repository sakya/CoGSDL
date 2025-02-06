#include "CoGSDL.h"

CoGSDL _Globals = {
  0,
  0,
  0,
  0,
  0,
  NULL
};

int main(int argc, char* argv[])
{
  if (init() < 0) {
    printf("init failed\n");
    return -1;
  }

  Scene scene;
  loadScene("d:\\paolo.iommarini\\Downloads\\ChoiceOfGames\\Vampire - Night Road\\scenes\\startup.txt", &scene);
  draw();

  SDL_DestroyRenderer(_Globals.renderer);
  SDL_DestroyWindow(_Globals.window);
  SDL_Quit();

  return 0;
}