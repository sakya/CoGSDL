#ifndef PLAYGAME_H__
#define PLAYGAME_H__
#include <stdio.h>
#include <time.h>
#include <SDL2/SDL_image.h>
#include "game.h"

/// @brief Default reuse of the choice options in the current scene (*allow_reuse, *hide_reuse, *disable_reuse)
enum choice_reuse
{
  reuse_allow = 0,
  reuse_hide = 1,
  reuse_disable = 2
};

/// @brief Number of visits to a label in the current page (*looplimit)
struct label_visit
{
  unsigned int sceneHash;
  long position;
  int count;
} typedef label_visit;

/// @brief A line of a choice at the level of the options (#option, *if...): reaching it
/// at the end of an option body means jumping to the end of the choice (as ChoiceScript does)
struct choice_end
{
  unsigned int sceneHash;
  long position;
  long endPosition;
} typedef choice_end;

struct gosub_info
{
  /// @brief FILE to return (NULL for a *gosub in the same scene)
  FILE *fp;
  /// @brief Position to return
  long position;
  /// @brief Saved for *gosub_scene: size of the file to return
  long fpSize;
  /// @brief Saved for *gosub_scene: scene index to return
  int sceneIndex;
  /// @brief Saved for *gosub_scene: scene name to return
  char sceneName[256];
  /// @brief Saved for *gosub_scene: temporary variables of the calling scene
  variable *tempVariables;
  int tempVariablesCount;
  /// @brief Saved for *gosub_scene: default reuse of the calling scene
  enum choice_reuse choiceReuse;
  /// @brief Saved for *gosub_scene: loop limit of the calling scene
  int loopLimit;
} typedef gosub_info;

/// @brief The option of a choice being executed
struct choice_context
{
  /// @brief The position of the first line after the choice block
  long endPosition;
  /// @brief The indentation of the option line
  int indentation;
  /// @brief The gosub depth when the option was selected
  int gosubCount;
} typedef choice_context;

struct game_status
{
    /// @brief Startup or scene file
    FILE *fp;
    /// @brief Size of the file we are executing
    long fpSize;
    /// @brief Position of the file we have to execute
    long fpPosition;
    gosub_info gosub[32];
    int gosubCount;
    /// @brief The scene index in game.scenes we are executing
    int sceneIndex;
    /// @brief The name of the scene we are executing
    char sceneName[256];
    /// @brief Arguments of the last *gosub/*gosub_scene/*goto_scene (named with *params)
    char *params[32];
    int paramsCount;
    /// @brief Set to 1 when the game is ended (*ending or end of the last scene)
    int gameEnded;
    /// @brief Default reuse of the choice options in the current scene
    enum choice_reuse choiceReuse;
    /// @brief Maximum number of visits to a label in a page (*looplimit)
    int loopLimit;
    label_visit labelVisits[256];
    int labelVisitsCount;
    /// @brief The text to display
    char *text;
    /// @brief The text scroll y value
    int scrollY;
    /// @brief Set to 1 if the text was fully rendered
    int textFullyRendered;
    /// @brief The y value of the last rendered element
    int lastRenderedY;
    /// @brief Set to 1 if we need to show a "next" button
    int showPageBreak;
    /// @brief The message to show for the page break
    char pageBreakMessage[64];
    /// @brief Set to 1 if the text can be scrolled
    int canScrollDown;
    /// @brief Set to 1 if *input_text is waiting for the text
    int inputTextActive;
    /// @brief The variable of *input_text
    char inputTextVariable[256];
    /// @brief *input_text options
    int inputTextAllowBlank;
    int inputTextLong;
    /// @brief Set to 1 when the keyboard was opened automatically
    int inputTextOpened;
    /// @brief Set to 1 for *input_number (with the allowed range)
    int inputNumber;
    double inputNumberMin;
    double inputNumberMax;
    int inputNumberIntRequired;

    /// @brief The labels of the opened file
    label* labels;
    int labelsCount;

    /// @brief Temp variables
    variable* tempVariables;
    int tempVariablesCount;

    /// @brief The choices to display to the user
    choice *choices;
    int choicesCount;
    int selectedChoiceIndex;
    /// @brief The position of the first line after the last choice block
    long choiceEndPosition;

    /// @brief The options being executed (an option body ends continuing after its choice block)
    choice_context choiceContexts[32];
    int choiceContextsCount;

    /// @brief The option lines of the choices met in the game (see choice_end)
    choice_end choiceEnds[512];
    int choiceEndsCount;

    char used_choices[1024][64];
    int used_choices_count;
} typedef game_status;

struct image_cache_item
{
    char *name;
    SDL_Surface *surface;
    int w;
    int h;
} typedef image_cache_item;

struct parentheses
{
  int open;
  int close;
} typedef parentheses;

/// @brief Open the stats screen: execute scenes/choicescript_stats.txt with its own state
/// @return 0 if the game has no stats scene
int pg_statsOpen();
/// @brief Close the stats screen and restore the game state
void pg_statsClose();
/// @brief Check if the stats screen is open
int pg_isStatsActive();
/// @brief Manage the input and draw the current page (text, page break, choice button)
void pg_drawPage();

/// @brief Close the running game and free its memory
/// @brief Initialize the running game (runningGame) at the start of the first scene
void pg_init();
/// @brief Apply the changed settings (text font, size, theme) to the running game: the page is rendered again
void pg_refreshLayout();
void pg_dispose();

/// @brief Number of save slots for each game
#define SAVE_SLOTS 5

struct save_info
{
  /// @brief Set to 1 if the slot contains a save
  int used;
  /// @brief Name of the save (built from the position in the game)
  char name[256];
  /// @brief Time of the save
  time_t time;
} typedef save_info;

/// @brief Read the information of a save slot of the running game
/// @return 1 if the slot contains a save
int pg_getSaveInfo(int slot, save_info *info);
/// @brief Save the state at the start of the current page in a slot
/// @return 1 on success
int pg_saveGame(int slot);
/// @brief Load a saved game and execute the saved page
/// @return 1 on success
int pg_loadGame(int slot);
/// @brief Find the most recent save of all the games
/// @param gameIndex The index of the game in _Globals.games
/// @param slot The save slot
/// @return 1 if a save was found
int pg_findLastSave(int *gameIndex, int *slot);
/// @brief Start the game of the most recent save and load it
/// @return 1 on success
int pg_continueLastSave();

#endif