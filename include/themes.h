#ifndef THEMES_H__
#define THEMES_H__

#include <SDL2/SDL_image.h>

struct theme
{
    SDL_Color backgroundColor;
    SDL_Color headerBackgroundColor;
    SDL_Color textColor;
    SDL_Color selectedTextColor;
    SDL_Color titleTextColor;
    SDL_Color disabledTextColor;
} typedef theme;

extern const theme defaultTheme;
#endif