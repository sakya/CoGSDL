#include "CoGSDL.h"

const int HEADER_HEIGHT = 30;

const SDL_Color colorBackground = {30, 30, 30};
const SDL_Color colorHeaderBackground = {60, 60, 60};
const SDL_Color colorText = {255, 255, 255};
const SDL_Color colorSelectedText = {95, 195, 248};
const SDL_Color colorTitleText = {242, 238, 143};

CoGSDL _Globals = {
  0,
  0,
  0,
  0,
  0,
  0,
  NULL
};

Keys _Keys = {
  SDL_SCANCODE_UP,
  SDL_SCANCODE_DOWN,

  SDL_SCANCODE_Z,
  SDL_SCANCODE_X
};

int main(int argc, char* argv[])
{
  if (init() < 0) {
    logError("Init failed\n");
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
  logDispose();

  return 0;
}