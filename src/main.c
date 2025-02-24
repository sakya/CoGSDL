#include "CoGSDL.h"

SDL_Surface *app_icon = NULL;
SDL_Surface *button_x = NULL;
SDL_Surface *button_y = NULL;
SDL_Surface *button_a = NULL;
SDL_Surface *button_b = NULL;
SDL_Surface *dpad = NULL;
SDL_Surface *dpad_up_down = NULL;

Mix_Chunk* sound_menu_change = NULL;

game *runningGame = NULL;
SDL_Surface *runningGameIcon = NULL;

CoGSDL _Globals = {
  0,
  scroll_page,
  1,
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
  SDL_SCANCODE_SPACE,

  SDL_SCANCODE_Q,
  SDL_SCANCODE_W,
  SDL_SCANCODE_R,
  SDL_SCANCODE_E,
};

const theme darkTheme = {
  {30, 30, 30},
  {60, 60, 60},
  {255, 255, 255},
  {95, 195, 248},
  {242, 238, 143}
};

const theme sepiaTheme = {
  {225, 228, 196},
  {139, 90, 43},
  {44, 44, 44},
  {107, 66, 38},
  {225, 228, 196},
};

const theme *_theme = &sepiaTheme;

int main(int argc, char* argv[])
{
  if (init() < 0) {
    logError("Init failed\n");
    return -1;
  }

  draw();

  free_images();
  free_sounds();
  IMG_Quit();
  TTF_Quit();
  SDL_DestroyRenderer(_Globals.renderer);
  SDL_DestroyWindow(_Globals.window);
  SDL_Quit();
  logDispose();

  return 0;
}