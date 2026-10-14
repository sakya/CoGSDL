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

// No initializer: the struct is large (games[255]) and an explicit
// initializer would store it entirely inside the executable.
// The fields are set in init().
CoGSDL _Globals;

#ifdef MIYOO
/* Miyoo Mini (Onion): i tasti arrivano come i keycode di KeyMap.h */
Keys _Keys = {
  SDL_SCANCODE_UP,        /* Up */
  SDL_SCANCODE_DOWN,      /* Down */

  SDL_SCANCODE_SPACE,     /* A */
  SDL_SCANCODE_LCTRL,     /* B */
  SDL_SCANCODE_LSHIFT,    /* X */
  SDL_SCANCODE_LALT,      /* Y */

  SDL_SCANCODE_RETURN,    /* Start */
  SDL_SCANCODE_RCTRL,     /* Select */

  SDL_SCANCODE_E,         /* L1 */
  SDL_SCANCODE_TAB,       /* L2 */
  SDL_SCANCODE_T,         /* R1 */
  SDL_SCANCODE_BACKSPACE, /* R2 */

  SDL_SCANCODE_LEFT,      /* Left */
  SDL_SCANCODE_RIGHT,     /* Right */
};
#else
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

  SDL_SCANCODE_LEFT,
  SDL_SCANCODE_RIGHT,
};
#endif

const theme darkTheme = {
  {30, 30, 30},
  {60, 60, 60},
  {255, 255, 255},
  {95, 195, 248},
  {242, 238, 143},
  {109, 109, 109}
};

const theme sepiaTheme = {
  {225, 228, 196},
  {139, 90, 43},
  {44, 44, 44},
  {107, 66, 38},
  {225, 228, 196},
  {161, 163, 142}
};

const theme lightTheme = {
  {210, 210, 210},
  {33, 97, 160},
  {33, 33, 33},
  {21, 101, 192},
  {255, 255, 255},
  {170, 170, 170}
};

const theme *_theme = &sepiaTheme;

int main(int argc, char* argv[])
{
  if (init() < 0) {
    logError("Init failed");
    return -1;
  }

  draw();

  free_images();
  free_sounds();
  free_fonts();
  IMG_Quit();
  TTF_Quit();
  SDL_DestroyRenderer(_Globals.renderer);
  SDL_DestroyWindow(_Globals.window);
  SDL_Quit();
  logDispose();

  return 0;
}