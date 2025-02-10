#include "CoGSDL.h"

const int HEADER_HEIGHT = 30;

const SDL_Color colorBackground = {30, 30, 30};
const SDL_Color colorHeaderBackground = {60, 60, 60};
const SDL_Color colorText = {255, 255, 255};
const SDL_Color colorSelectedText = {95, 195, 248};
const SDL_Color colorTitleText = {242, 238, 143};

SDL_Surface *app_icon;
SDL_Surface *button_x;
SDL_Surface *button_y;
SDL_Surface *button_a;
SDL_Surface *button_b;
SDL_Surface *dpad;
SDL_Surface *dpad_up_down;

game *runningGame = NULL;
SDL_Surface *runningGameIcon = NULL;

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
  SDL_SCANCODE_X,
  SDL_SCANCODE_A,
  SDL_SCANCODE_S,

  SDL_SCANCODE_RETURN,
  SDL_SCANCODE_SPACE
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

  free_images();
  IMG_Quit();
  TTF_Quit();
  SDL_DestroyRenderer(_Globals.renderer);
  SDL_DestroyWindow(_Globals.window);
  SDL_Quit();
  logDispose();

  return 0;
}