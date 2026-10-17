#ifndef SAVES_H
#define SAVES_H

// Saves of the running game: slots, checkpoints, episodes (*save_game/*restore_game), achievements

#include "CoGSDL.h"
#include "gui_playgame.h"

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
  /// @brief Play time (seconds): the sum of the time of all the game sessions
  unsigned long long playTime;
} typedef save_info;

/// @brief Read the information of a save slot of the running game
/// @return 1 if the slot contains a save
int sv_getSaveInfo(int slot, save_info *info);
/// @brief Save the state at the start of the current page in a slot
/// @return 1 on success
int sv_saveGame(int slot);
/// @brief Load a saved game and execute the saved page
/// @return 1 on success
int sv_loadGame(int slot);
/// @brief Find the most recent save of all the games
/// @param gameIndex The index of the game in _Globals.games
/// @param slot The save slot
/// @return 1 if a save was found
int sv_findLastSave(int *gameIndex, int *slot);
/// @brief Start the game of the most recent save and load it
/// @return 1 on success
int sv_continueLastSave();
/// @brief Start a game session (the game is started or loaded)
/// @param playTime The play time of the previous sessions (seconds)
void sv_startPlayTime(unsigned long long playTime);
/// @brief Get the play time (seconds): the previous sessions and the current one
unsigned long long sv_getPlayTime();
/// @brief Format the description of a save slot: play time and date (e.g. "2h 30m - 2026-10-17 18:30")
void sv_formatSaveDetails(const save_info *info, char *text, int size);

/* Used by the interpreter (choicescript.c) */

/// @brief Close the *restore_game menu and add the pending message (if any) at the start of the new page
void sv_restoreGameNewPage();
/// @brief Save the state at the start of the page (used by sv_saveGame)
void sv_takeSnapshot();
/// @brief Manage *save_checkpoint [slot]
void sv_manageSaveCheckpointCommand(const char *text);
/// @brief Manage *restore_checkpoint [slot]
/// @return 1 if the checkpoint was restored (the execution continues from the checkpoint)
int sv_restoreCheckpoint(const char *text);
/// @brief Manage the achieve command: mark the achievement as earned and show it
void sv_manageAchieveCommand(const char *text);
/// @brief Manage the save_game command: save the variables for the next episode (./saves/episodes)
void sv_manageSaveGameCommand(const char *text);
/// @brief Manage *restore_game: show the saves of ./saves/episodes as a choice
/// @return The position after the command (and its block)
long sv_manageRestoreGameCommand(const char *text, int indentation);
int sv_selectRestoreOption(choice *c);

/* Used by gui_playgame.c */

void sv_freeSnapshot();
void sv_clearCheckpoints();
/// @brief Load the achievements earned in all the games played (./saves/<game folder>/achievements.txt)
void sv_loadAchievements();

#endif
