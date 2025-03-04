#ifndef PLAYGAME_H__
#define PLAYGAME_H__
#include <stdio.h>
#include <SDL2/SDL_image.h>
#include "game.h"

struct if_info
{
  int indentation;
  long end_position;
} typedef if_info;

struct gosub_info
{
  FILE *fp;
  long position;
} typedef gosub_info;

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
    if_info ifInfo[32];
    int ifInfoCount;
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

#endif