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
    logInfo("init failed\n");
    return -1;
  }

  _Globals.gamesCount = listGames("d:\\paolo.iommarini\\Downloads\\ChoiceOfGames\\", _Globals.games);

  //game game;
  //loadGame("d:\\paolo.iommarini\\Downloads\\ChoiceOfGames\\Vampire - Parliament of Knives", &game);

  draw();

  IMG_Quit();
  TTF_Quit();
  SDL_DestroyRenderer(_Globals.renderer);
  SDL_DestroyWindow(_Globals.window);
  SDL_Quit();

  return 0;
}