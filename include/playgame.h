#ifndef PLAYGAME_H__
#define PLAYGAME_H__
#include <stdio.h>

enum gui_status
{
    play = 0,
    stats = 1,
    menu = 2
};

struct game_status
{
    enum gui_status status;
    /// @brief Startup or scene file
    FILE *fp;
    /// @brief Size of the file we are executing
    long fpSize;
    /// @brief Position of the file we have to execute
    long fpPosition;
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
    /// @brief Set to 1 if the text can be scrolled
    int canScrollDown;
} typedef game_status;

#endif