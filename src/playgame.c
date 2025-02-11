#include "CoGSDL.h"

void pg_init();
void pg_dispose();
void pg_exec();
void pg_renderText(char* text);

static FILE *pg_fp = NULL;
static char *pg_text = NULL;
static char *pg_scene = NULL;
static long pg_position = -1;

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
    pg_exec();
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

  pg_fp = fopen(filePath, "rb");
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

void pg_exec()
{
  char line[4096];
  int indentation = -1;
  int readingText = 0;

  if(pg_text != NULL) {
    free(pg_text);
    pg_text = NULL;
  }

  while (fgets(line, sizeof(line), pg_fp)) {
    int lineIndentation = getIndentation(line);
    if (indentation < 0)
      indentation = lineIndentation;

    if (lineIndentation != indentation) {
      break;
    }

    if (isComment(line))
      continue;

    trim(line);
    if (line[0] == '*') {
      // Command
      readingText = 0;
    } else {
      // Read text
      readingText = 1;
      if (pg_text == NULL) {
        pg_text = malloc(strlen(line) + 1);
        strcpy(pg_text, line);
      } else {
        if (strlen(line)) {
          char *tmp = realloc(pg_text, strlen(pg_text) + strlen(line) + 2);
          if (tmp != NULL) {
            strcat(tmp, " ");
            strcat(tmp, line);
            pg_text = tmp;
            tmp = NULL;
          }
        } else {
          char *tmp = realloc(pg_text, strlen(pg_text) + 1);
          if (tmp != NULL) {
            strcat(tmp, "\n");
            pg_text = tmp;
            tmp = NULL;
          }
        }
      }
    }
  }

  if (pg_text != NULL) {
    logDebug("Text to render: %s\n", pg_text);
  }
}