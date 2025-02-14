#ifndef PLAYGAME_H__
#define PLAYGAME_H__
#include <stdio.h>

struct game_status
{
    /// @brief Startup or scene file
    FILE *fp;
    /// @brief Position of the file we have to execute
    long fpPosition;
    /// @brief The scene index in game.scenes we are executing
    int sceneIndex;
    /// @brief The text to display
    char *text;
    /// @brief The text scroll y value
    int scrollY;
    /// @brief Set to 1 if we need to show a "next" button
    int showPageBreak;
} typedef game_status;

#endif