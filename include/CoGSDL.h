#ifndef COGSDL_H__
#define COGSDL_H__

#include "locale.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_ttf.h"
#include "SDL2/SDL_image.h"
#include "SDL2/SDL_mixer.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>

#include "game.h"
#include "KeyMap.h"

#define TAB_SIZE 2

extern const int HEADER_HEIGHT;

extern const SDL_Color colorBackground;
extern const SDL_Color colorHeaderBackground;
extern const SDL_Color colorText;
extern const SDL_Color colorSelectedText;
extern const SDL_Color colorTitleText;

extern SDL_Surface* app_icon;
extern SDL_Surface* button_x;
extern SDL_Surface* button_y;
extern SDL_Surface* button_a;
extern SDL_Surface* button_b;
extern SDL_Surface* dpad;
extern SDL_Surface* dpad_up_down;

extern Mix_Chunk* sound_menu_change;

extern game* runningGame;
extern SDL_Surface *runningGameIcon;

enum status
{
  MainMenu = 0,
  GameSelect = 1,
  Game = 2
};

enum logLevel
{
  Debug = 0,
  Info = 1,
  Warning = 2,
  Error = 3
};

struct CoGSDL {
  enum logLevel logLevel;
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
  int input[256];

  TTF_Font *defaultFont;
  int defaultFontSize;
  TTF_Font *defaultSmallFont;
  int defaultSmallFontSize;
} typedef CoGSDL;

extern CoGSDL _Globals;

struct Keys
{
  int KeyUp;
  int KeyDown;
  int KeyA;
  int KeyB;
  int KeyX;
  int KeyY;

  int KeyStart;
  int KeySelect;

  int KeyL1;
  int KeyL2;
  int KeyR1;
  int KeyR2;
} typedef Keys;

extern Keys _Keys;

/* init.c */
/// @brief Initialize SDL
/// @return
int init();

/* draw.c */
int draw();
void free_images();
void free_sounds();

/* events.c */
/// @brief Manage SDL events
/// @return 0 on success
void manageEvents();

/* game.c */
/// @brief Loads a game from a directory (read the file scenes/startup.txt)
/// @param path Path to the directory containing the game
/// @param game The game
/// @return 0 on success
int loadGame(const char* path, game* game);
/// @brief Free memory allocated by a game
/// @param game The game
void freeGame(game *game);
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
/// @brief Check if a string starts with the given word (use space and tab as word separator)
/// @param prefix The prefix to check
/// @param string The string
/// @return 1 if string starts with prefix
int startsWithWord(const char *prefix, const char *string);
/// @brief Get the number of indentation characters
/// @param string The string
/// @return The number of indentation characters
int getIndentation(const char *string);
/// @brief Get a substring of a string
/// @param source The source string
/// @param target The destination string
/// @param pos The starting position of the substring
/// @param length The substring length
void subString(const char *source, char *target, const int pos, const int length);
void trim(char *string);
/// @brief Check if a file exists
/// @param filename The path to the file
/// @return 1 if the file exists
int fileExists(const char *filename);

void sdlWriteText(TTF_Font* font, SDL_Color color, char* text, int x, int y, int maxWidth, int maxHeight);
void sdlBlitImage(SDL_Surface *surface, int x, int y, int maxWidth, int maxHeight);
void sdlBlitImageFile(char* image, int x, int y, int maxWidth, int maxHeight);

/* log.c */
/// @brief Initialize log
/// @param filePath The log file
/// @return 1 on success
int logInit(const char* filePath);
/// @brief Log a debug message
/// @param format The format
/// @param ... The arguments
void logDebug(const char *format, ...);
/// @brief Log a info message
/// @param format The format
/// @param ... The arguments
void logInfo(const char *format, ...);
/// @brief Log a warning message
/// @param format The format
/// @param ... The arguments
void logWarning(const char *format, ...);
/// @brief Log an error message
/// @param format The format
/// @param ... The arguments
void logError(const char *format, ...);
/// @brief Dispose the log
void logDispose();

/* mainmenu.c*/
void drawMainMenu();

/* gameselectmenu.c */
void drawGameSelectMenu();

/* playgame.c */
void drawGame();

#endif