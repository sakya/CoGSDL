#include "CoGSDL.h"

void pg_init();
void pg_dispose();
void pg_renderText(char* text);

char *pg_text = NULL;

FILE *pg_fp = NULL;
char *pg_scene = NULL;
long pg_position = -1;

void drawGame()
{
  if (_Globals.input[_Keys.KeyStart] == 1) {
    _Globals.input[_Keys.KeyStart] = 0;
    pg_dispose();

    _Globals.status = GameSelect;
    return;
  }

  // Open the file
  if (pg_fp == NULL) {
    pg_init();
  }

  if (pg_text != NULL) {
    // Render text
    pg_renderText(pg_text);
  }
}

void pg_init()
{
  char filePath[255];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/scenes/");
  if (pg_scene != NULL) {
    strcat(filePath, pg_scene);
  } else {
    strcat(filePath, "startup.txt");
  }

  pg_fp = fopen(filePath, "r");
  logDebug("Opened file %s\n", filePath);

  // Check initial position
  if (pg_position < 0) {
    pg_position = runningGame->startPosition;
    logDebug("Position set to %li\n", pg_position);
  }
  fseek(pg_fp, pg_position, SEEK_SET);
}

void pg_dispose()
{
  pg_position = -1;

  free(pg_scene);
  pg_scene = NULL;

  free(pg_text);
  pg_text = NULL;

  fclose(pg_fp);
  pg_fp = NULL;

  freeGame(runningGame);
  runningGame = NULL;

  SDL_FreeSurface(runningGameIcon);
  runningGameIcon = NULL;
}

void pg_renderText(char* text)
{

}