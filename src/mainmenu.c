#include "CoGSDL.h"

double mmAngle = 0;
int mmIndex = 0;

void drawMainMenu()
{
  SDL_Texture* texture = SDL_CreateTextureFromSurface(_Globals.renderer, app_icon);
  SDL_Rect rect = {30, 50, 100, 100};
  SDL_Rect crop = {0, 0, app_icon->w, app_icon->h};
  SDL_Point center = { 50, 50};
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  SDL_RenderCopyEx(_Globals.renderer, texture, &crop , &rect, mmAngle, &center, flip);
  SDL_DestroyTexture(texture);

  mmAngle += 0.5;
  if (mmAngle >= 360)
    mmAngle = 0;

  sdlWriteText(_Globals.defaultFont, colorText, "CoGSDL", 140, 75, 0, 0);
  sdlWriteText(_Globals.defaultSmallFont, colorText, "A Choice of Games interpreter", 140, 100, 0, 0);
}