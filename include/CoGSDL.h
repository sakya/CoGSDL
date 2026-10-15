#ifndef COGSDL_H__
#define COGSDL_H__

#include <stdlib.h>
#include <locale.h>
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <stdarg.h>
#ifdef _WIN32
#include <direct.h>
#endif

#include "game.h"
#include "KeyMap.h"
#include "utf8.h"
#include "themes.h"

#define TAB_SIZE 2
#define TEXT_MARGIN 5

#ifdef MIYOO
  #define HEADER_HEIGHT 34
#else
  #define HEADER_HEIGHT 30
#endif

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

enum scroll_type
{
  scroll_line = 0,
  scroll_page = 1
};

enum status
{
  MainMenu = 0,
  About = 1,
  GameSelect = 2,
  Game = 3,
  GameMenu = 4,
  GameStats = 5,
  GameChoice = 6,
  Settings = 7,
  OnScreenKeyboard = 8
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
  enum scroll_type scroll;
  int justify_text;
  /// @brief Show the FPS in the header (settings.cfg: show_fps=true)
  int show_fps;
  int quit;
  int32_t displayWidth;
  int32_t displayHeight;
  int32_t windowWidth;
  int32_t windowHeight;
  SDL_Window *window;
  SDL_Renderer *renderer;
  /// @brief Miyoo: the frame is drawn by the software renderer on screenSurface
  /// and copied to the screen with the MI_GFX renderer (see presentFrame)
  SDL_Renderer *screenRenderer;
  SDL_Surface *screenSurface;
  SDL_Texture *screenTexture;

  int gamesCount;
  game games[255];

  enum status status;
  int input[256];

  TTF_Font *defaultFont;
  int defaultFontSize;
  int defaultFontHeight;
  int titleFontSize;
  TTF_Font *titleFont;
  TTF_Font *defaultSmallFont;
  int defaultSmallFontSize;

  TTF_Font *textFont;
  int textFontSize;
  /// @brief Index in textFontFamilies (settings.cfg: text_font_family)
  int textFontFamily;
  TTF_Font *textItalicFont;
  TTF_Font *textBoldFont;

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

  int KeyLeft;
  int KeyRight;
} typedef Keys;

extern Keys _Keys;
extern const theme* _theme;

/* init.c */
/// @brief Initialize SDL
/// @return
int init();

/* draw.c */
int draw();
/// @brief Show the frame drawn with _Globals.renderer
void presentFrame();
void free_images();
void free_sounds();
void free_fonts();

/* events.c */
/// @brief Manage SDL events
/// @return 0 on success
void manageEvents();

/* game.c */
/// @brief Loads a game from a directory (read the file scenes/startup.txt)
/// @param path Path to the directory containing the game
/// @param game The game
/// @param forListing If 1 only title, author, icon informations are loaded
/// @return 0 on success
int loadGame(const char *path, game *game, int forListing);
/// @brief Free memory allocated by a game
/// @param game The game
void freeGame(game *game);
/// @brief Free the memory allocated by a game, but not the game struct (e.g. the games of _Globals.games)
/// @param game The game
void freeGameData(game *game);
/// @brief List all games in a directory
/// @param path The directory path
/// @param games A pointer to an array of games
/// @return The number of games found
int listGames(const char *path, game *games);
/// @brief Get a variable value
/// @param varName The variable name
/// @param game The game
/// @return The variable value or NULL
char* getGameVarValue(const char *varName, game* game);
variable* getGameVar(const char *varName, game* game);
/// @brief Set a variable value
/// @param varName The variable name
/// @param value The variable value
/// @param game The game
int setGameVarValue(const char *varName, const char *value, game *game);
void setVarValue(variable *var, const char *value);
/// @brief Check if a line of text is a comment
/// @param line The line
int isComment(const char *line);
int isCommand(const char *line, const char command);
void addGameVar(game* game, enum var_types type, const char* name, const char* value);
enum var_types get_var_type(const char *value);

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
/// @brief Get the position of a char in a string, going backward
/// @param string The string
/// @param character The character to find
/// @param startFrom The start position
/// @return The position or -1
int lastIndexOf(const char *string, const char character, const int startFrom);
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
/// @brief Check if a string ends with the given string
/// @param suffix The suffix to check
/// @param string The string
/// @return 1 if string ends with suffix
int endsWith(const char *suffix, const char *string);
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
int is_number(const char *text, int *value);
/// @brief Compare two strings ignoring case
/// @param s1 The first string
/// @param s2 The second string
/// @return 0 if the strings are equals, 1 if s1 > s2, -1 if s1 < s2
int stricmp(const char *s1, const char *s2);

int sdlWriteText(TTF_Font* font, SDL_Color color, char* text, int x, int y, int maxWidth, int maxHeight);
void sdlBlitImage(SDL_Surface *surface, int x, int y, int maxWidth, int maxHeight);

// Game text font size (text_font_size in settings.cfg)
#define TEXT_FONT_SIZE_MIN 16
#define TEXT_FONT_SIZE_MAX 36
int loadTextFonts();

// Game text font families (text_font_family in settings.cfg)
typedef struct {
  const char *name; // Name shown in the settings and saved in settings.cfg
  const char *file; // Files in ./fonts: <file>-Regular.ttf, <file>-Italic.ttf, <file>-Bold.ttf
} text_font_family;
extern const text_font_family textFontFamilies[];
extern const int textFontFamiliesCount;
void sdlImageSize(SDL_Surface *surface, int maxWidth, int maxHeight, int *width, int *height);
/// @brief Get the (cached) texture of a surface scaled to the given size
/// The textures are kept between the frames: free the surfaces with sdlFreeSurface
SDL_Texture *sdlGetImageTexture(SDL_Surface *surface, int width, int height);
/// @brief Remove the cached textures of a surface and free it
void sdlFreeSurface(SDL_Surface *surface);
/// @brief Called at the end of each frame: release the cached textures not used for a while
void sdlCacheEndFrame();
/// @brief Release all the cached textures
void sdlCacheClear();
/// @brief Draw the background of the selected item of a menu (almost as wide as the screen)
/// @param y The top of the item
/// @param height The height of the item
void sdlDrawSelection(int y, int height);
/// @brief The height of the selection of a menu item with a single line of text
int sdlSelectionHeight();
void sdlBlitImageFile(char* image, int x, int y, int maxWidth, int maxHeight);
int get_line_height(TTF_Font *font);

/* log.c */
/// @brief Initialize log
/// @param filePath The log file
/// @return 1 on success
int logInit(const char *filePath);
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
/// @brief Free the resources of the game selection menu (on exit)
void gameSelectDispose();

/* playgame.c */
void drawGame();

/* about.c */
void drawAbout();

/* gui_gamemenu.c */
void drawGameMenu();

/* gui_gamestats.c */
void drawGameStats();
/* gui_gamechoice.c */
void drawGameChoice();
/* gui_settings.c */
void drawSettings();
/// @brief Load the settings (theme) from settings.cfg
void settingsLoad();
/// @brief Save the settings (theme) to settings.cfg
void settingsSave();
/* gui_osk.c */
/// @brief Open the on screen keyboard (the current status is restored when it closes)
/// @param title The text shown above the text box
/// @param text The initial text
/// @param maxLength The maximum number of characters
/// @param allowEmpty 0 if an empty text cannot be confirmed
void osk_open(const char *title, const char *text, int maxLength, int allowEmpty);
/// @brief Check the text before closing the keyboard
/// @return NULL if the text is valid, otherwise the error message to show
typedef const char *(*osk_validator)(const char *text);
/// @brief Open the on screen keyboard to type a number (digits, minus and decimal point)
/// @param validate The function that checks the number (can be NULL)
void osk_openNumeric(const char *title, const char *text, int maxLength, osk_validator validate);
/// @brief Get the text confirmed in the on screen keyboard
/// @param text The buffer for the text
/// @param size The buffer size
/// @return 1 if a text was confirmed (only once for each confirmation)
int osk_getResult(char *text, size_t size);
void drawOsk();

#endif