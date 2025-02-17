#ifndef PLAYGAME_H__
#define PLAYGAME_H__
#include <stdio.h>
#include <SDL2/SDL_image.h>

struct game_status
{
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
    /// @brief The labels of the opened file
    label* labels;
    int labelsCount;
} typedef game_status;

struct image_cache_item
{
    char *name;
    SDL_Surface *surface;
} typedef image_cache_item;

#endif