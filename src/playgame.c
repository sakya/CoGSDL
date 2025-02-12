#include "CoGSDL.h"

void pg_init();
void pg_dispose();
void pg_exec();
void pg_renderText(char* text);
int pg_indexOfWordEnd(const char *string, const int startFrom);

static FILE *pg_fp = NULL;
static char *pg_text = NULL;
static char *pg_scene = NULL;
static long pg_position = -1;
static int lineHeight = 0;
static int scrollY = 0;

void drawGame()
{
  if (_Globals.input[_Keys.KeyDown] == 1) {
    _Globals.input[_Keys.KeyDown]= 0;
    scrollY += lineHeight;
  }
  if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp]= 0;
    scrollY -= lineHeight;
    if (scrollY < 0)
      scrollY = 0;
  }

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

  SDL_Surface* surface = TTF_RenderUTF8_Blended(_Globals.defaultFont, "H", colorText);
  lineHeight = surface->h;
  logDebug("Line height %li\n", lineHeight);
  SDL_FreeSurface(surface);
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
  if (text == NULL || strlen(text) == 0)
    return;

  int end = strlen(text);
  int idx = 0;
  int x = TEXT_MARGIN;
  int y = HEADER_HEIGHT + TEXT_MARGIN;
  int width;
  int chars;
  char test[2];
  char word[256];
  TTF_Font *font = _Globals.defaultFont;
  while(1) {
    // Find the next word to print
    int wordEnd = pg_indexOfWordEnd(text, idx);
    subString(text, word, idx, wordEnd - idx + 1);
    TTF_MeasureUTF8(_Globals.defaultFont, word, _Globals.WINDOW_WIDTH, &width, &chars);
    if (x + width + TEXT_MARGIN >= _Globals.WINDOW_WIDTH) {
      y += lineHeight;
      x = TEXT_MARGIN;
      if (y + lineHeight > _Globals.WINDOW_HEIGHT - 60) {
        break;
      }
    }

    int wordLen = strlen(word);
    for (int i = 0; i < wordLen; i++) {
      if (word[i] == '\n') {
        y += lineHeight;
        x = TEXT_MARGIN;
        if (y + lineHeight > _Globals.WINDOW_HEIGHT - 60) {
          break;
        }
        continue;
      }

      // Check font change:
      if (word[i] == '[')  {
        // Italic
        if (i + 2 < wordLen && word[i + 1] == 'i' && word[i + 2] == ']') {
          font = _Globals.italicFont;
          i += 2;
          continue;
        } else if (i + 3 < wordLen && word[i + 1] == '/' && word[i + 2] == 'i' && word[i + 3] == ']') {
          font = _Globals.defaultFont;
          i += 3;
          continue;
        }

        // Bold
        if (i + 2 < wordLen && word[i + 1] == 'b' && word[i + 2] == ']') {
          font = _Globals.boldFont;
          i += 2;
          continue;
        } else if (i + 3 < wordLen && word[i + 1] == '/' && word[i + 2] == 'b' && word[i + 3] == ']') {
          font = _Globals.defaultFont;
          i += 3;
          continue;
        }
      }

      int charWidth;
      test[0] = word[i];
      test[1] = '\0';

      TTF_MeasureUTF8(font, test, _Globals.WINDOW_WIDTH, &charWidth, &chars);
      if (y >= scrollY + HEADER_HEIGHT + TEXT_MARGIN)
        sdlWriteText(font, colorText, test, x, y - scrollY, 0, 0);
      x += charWidth;
    }

    idx += strlen(word);
    if (idx >= end)
      break;
  }
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
          int needsSpace = endsWith(".", line) ? 0 : 1;
          char *tmp = needsSpace
            ? realloc(pg_text, strlen(pg_text) + strlen(line) + 2)
            : realloc(pg_text, strlen(pg_text) + strlen(line) + 1);
          if (tmp != NULL) {
            if (needsSpace) {
              memset(tmp + strlen(tmp), 0, strlen(line) + 2);
              strcat(tmp, " ");
            } else {
              memset(tmp + strlen(tmp), 0, strlen(line) + 1);
            }
            strcat(tmp, line);
            pg_text = tmp;
            tmp = NULL;
          } else {
            logWarning("realloc failed");
            return;
          }
        } else {
          char *tmp = realloc(pg_text, strlen(pg_text) + 3);
          if (tmp != NULL) {
            memset(tmp + strlen(tmp), 0, 2);
            strcat(tmp, "\n\n");
            pg_text = tmp;
            tmp = NULL;
          } else {
            logWarning("realloc failed");
            return;
          }
        }
      }
    }
  }

  if (pg_text != NULL) {
    logDebug("Text to render: %s\n", pg_text);
  }
}

int pg_indexOfWordEnd(const char *string, const int startFrom)
{
  if (!string)
      return -1;

  int idx = startFrom;
  int len = strlen(string);
  while (idx < len) {
    if (string[idx] == ' ' || string[idx] == '\t' || string[idx] == '\n' || string[idx] == ',' ||
        string[idx] == '.' || string[idx] == ';' || string[idx] == ';')
      return idx;
    idx++;
  }

  return -1;
}