#ifndef COGSDL_H__
#define COGSDL_H__

#include "locale.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#include "game.h"
#include "KeyMap.h"

enum status
{
  MainMenu = 0,
  Game = 1
};

struct CoGSDL {
  int quit;
  int32_t DISPLAY_WIDTH;
  int32_t DISPLAY_HEIGHT;
  int32_t WINDOW_WIDTH;
  int32_t WINDOW_HEIGHT;
  SDL_Window *window;
  SDL_Renderer *renderer;

  int gamesCount;
  game games[255];

  enum status status;

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

/* game.c */
/// @brief Loads a game from a directory (read the file scenes/startup.txt)
/// @param path Path to the directory containing the game
/// @param game The game
/// @return 0 on success
int loadGame(const char* path, game* game);
/// @brief Load a game section
/// @param path The path to the section file
/// @param section The section
/// @return 0 on success
int loadSection(const char* path, section* section);
/// @brief List all games in a directory
/// @param path The directory path
/// @param games A pointer to an array of games
/// @return The number of games found
int listGames(const char* path, game *games);
/// @brief Get a variable value
/// @param varName The variable name
/// @param game The game
/// @return The variable value or NULL
char* getVarValue(const char *varName, game* game);

/* utils.c */
/// @brief Check if a string is empty or all whitespaces
/// @param string The string
/// @return 1 if the string is empty
int isEmpty(const char *string);
/// @brief Get the position of a char in a string
/// @param string The string
/// @param character The character to find
/// @param startFrom The start position
/// @return The position or -1
int indexOf(const char *string, const char character, const int startFrom);
/// @brief Get the position of a word break char (space or tab) in a string
/// @param string The string
/// @param startFrom The start position
/// @return The position or -1
int indexOfWordBreak(const char *string, const int startFrom);
/// @brief Check if a string starts with the given string
/// @param prefix The prefix to check
/// @param string The string
/// @return 1 if string starts with prefix
int startsWith(const char *prefix, const char *string);
void subString(const char *source, char *target, const int pos, const int length);
void trim(char *string);
/// @brief Check if a file exists
/// @param filename The path to the file
/// @return 1 if the file exists
int fileExists(const char *filename);

void sdlWriteText(char* text, int x, int y, int maxWidth, int maxHeight);
void sdlBlitImage(char* image, int x, int y, int maxWidth, int maxHeight);

/* log.c */
int logInit(const char* filePath);
void logInfo(const char *format, ...);

/* mainmenu.c */
void drawMainMenu();
#endif