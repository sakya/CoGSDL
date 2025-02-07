#ifndef COGSDL_H__
#define COGSDL_H__

#include "locale.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"
#include <sys/stat.h>

#include "game.h"
#include "KeyMap.h"

struct CoGSDL {
  int quit;
  int32_t DISPLAY_WIDTH;
  int32_t DISPLAY_HEIGHT;
  int32_t WINDOW_WIDTH;
  int32_t WINDOW_HEIGHT;
  SDL_Window *window;
  SDL_Renderer *renderer;

  TTF_Font *defaultFont;
  int defaultFontSize;
} typedef CoGSDL;

extern CoGSDL _Globals;

/* init.c */
/// @brief Initialize SDL
/// @return
int init();

/* draw.c */
int draw();

/* events.c */
/// @brief Manage SDL events
/// @return 0 on success
int manageEvents();

/* scene.c */
/// @brief Loads a game from a folder (read the file scenes/startup.txt)
/// @param path Path to the folder containing the game
/// @param game The game
/// @return 0 on success
int loadGame(char* path, game* game);
/// @brief Load a game section
/// @param path The path to the section file
/// @param section The section
/// @return 0 on success
int loadSection(char* path, section* section);

/* utils.c */
/// @brief Get the position of a char in a string
/// @param string The string
/// @param character The character to find
/// @param startFrom The start position
/// @return The position or -1
int indexOf(const char *string, char character, int startFrom);
/// @brief Check if a string starts with the given string
/// @param prefix The prefix to check
/// @param string The string
/// @return 1 if string starts with prefix
int startsWith(const char *prefix, const char *string);
void subString(const char *source, char *target, int pos, int length);
void trim(char *string);

// Return 1 if the given file exists
int fileExists(char *filename);

#endif