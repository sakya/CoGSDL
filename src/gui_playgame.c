#include "CoGSDL.h"
#include "gui_playgame.h"
#include "choicescript.h"

void pg_init();
void pg_dispose();
static void pg_freeSnapshot();
static void pg_clearCheckpoints();
static void pg_setCheckpoint(const char *name, char *state);
variable* getGameVarPrimitive(const char *varName, game* game);
static int pg_splitLines(char *content, char ***lines);
static int pg_validateState(char **lines, int start, int end);
static long pg_applyState(char **lines, int start, int end);
void pg_renderText(const char* text, int x, int y, int width, int height, int scrollY);
int pg_renderPageBreak();
static void pg_computeLineHeight();
/// @brief Load the achievements earned in all the games played (./saves/<game folder>/achievements.txt)
static void pg_loadAchievements();
/// @brief Save the earned achievements
static int pg_saveAchievements();
static int pg_sceneExists(const char *sceneName);
static int pg_parseSavedVariable(char *data, int *type, char **name, char **value);
static void pg_unescape(char *value);
/// @brief Open the on screen keyboard for *input_text
static void pg_openInputText();
/// @brief Draw a stat bar (*stat_bar value|label[|opposed label])
/// @return The height of the bar
static int pg_drawStatBar(SDL_Surface *surface, const char *line, int y);
/// @brief Get the next line from text
/// @param text The string
/// @param position The starting position. Updated to the starting position of the next line.
/// @param maxWidth The maximum width in pixels of the rendered line
/// @param wordWrap Set to 1 if the line was wrapped
/// @param last_x Set to the last x position blitted
/// @return A line of text or NULL. The position is updated with the position of the next line
char *pg_getNextLine(const char *text, int *position, int maxWidth, int *wordWrap, int *last_x);

/// @brief Manage the image and text_image command
/// @param surface The surface to blit the image
/// @param text The text of the line with
/// @param y The y value where the image have to be drawn
/// @return The height of the image
int pg_manageImageCommand(SDL_Surface *surface, const char *text, int y);
image_cache_item* pg_get_image_from_cache(const char *name);
image_cache_item* pg_add_image_to_cache(const char *name);

game_status pg_game_status;

#define RESTORE_MAX_SAVES 20
struct restore_menu
{
  int active;
  char cancelLabel[256];
  /// @brief Where the execution continues after *restore_game (and its block)
  long resumePosition;
  int savesCount;
  char savePaths[RESTORE_MAX_SAVES][600];
  int unrestorableCount;
  char unrestorableScenes[16][128];
  char unrestorableMessages[16][512];
} typedef restore_menu;

/// @brief The menu of *restore_game
static restore_menu pg_restoreMenu;
/// @brief A message shown at the start of the next page
static char *pg_pendingNotice = NULL;
static int line_height = 0;
static image_cache_item image_cache[32];
static int image_cache_count = 0;
static SDL_Surface* text_surface = NULL;
static int scroll_height;

void drawGame()
{
  if (_Globals.input[_Keys.KeyL1] == 1) {
    _Globals.input[_Keys.KeyL1] = 0;
    _Globals.status = GameMenu;
    return;
  }

  if (_Globals.input[_Keys.KeyR1] == 1) {
    _Globals.input[_Keys.KeyR1] = 0;
    _Globals.status = GameStats;
    return;
  }

  if (_Globals.input[_Keys.KeyStart] == 1) {
    _Globals.input[_Keys.KeyStart] = 0;
    _Globals.status = GameMenu;
    return;
  }

  // Initialize
  if (pg_game_status.fp == NULL) {
    pg_init();
    cs_exec();
  }

  pg_drawPage();
}

void pg_drawPage()
{
  int scrollY = pg_game_status.scrollY;
  if (pg_game_status.canScrollDown) {
    if (_Globals.input[_Keys.KeyDown] == 1) {
      _Globals.input[_Keys.KeyDown]= 0;
      pg_game_status.scrollY += scroll_height;
    }
  }

  if (_Globals.input[_Keys.KeyUp] == 1) {
    _Globals.input[_Keys.KeyUp]= 0;
    pg_game_status.scrollY -= scroll_height;
    if (pg_game_status.scrollY < 0)
      pg_game_status.scrollY = 0;
  }

  // *input_text: open the on screen keyboard as soon as the command is reached
  // (if it is closed without confirming, A opens it again)
  if (pg_game_status.inputTextActive && !pg_game_status.inputTextOpened) {
    pg_game_status.inputTextOpened = 1;
    _Globals.input[_Keys.KeyA] = 0;
    pg_openInputText();
    return;
  }

  if (_Globals.input[_Keys.KeyA] == 1) {
    if (!pg_game_status.canScrollDown && pg_game_status.inputTextActive) {
      _Globals.input[_Keys.KeyA] = 0;
      pg_openInputText();
      return;
    } else if (!pg_game_status.canScrollDown && pg_game_status.showPageBreak) {
      //Next
      _Globals.input[_Keys.KeyA] = 0;
      cs_exec();
    } else if (!pg_game_status.canScrollDown && pg_game_status.choicesCount > 0) {
      // Choice
      _Globals.input[_Keys.KeyA] = 0;
      _Globals.status = GameChoice;
      pg_game_status.selectedChoiceIndex = -1;
      return;
    }
  }

  if (scrollY != pg_game_status.scrollY) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }

  // The text of *input_text was confirmed
  if (pg_game_status.inputTextActive) {
    char inputText[4096];
    if (osk_getResult(inputText, sizeof(inputText))) {
      cs_setInputText(inputText);
      cs_exec();
    }
  }

  // A choice was selected
  if (pg_game_status.selectedChoiceIndex >= 0) {
    int index = pg_game_status.selectedChoiceIndex;
    pg_game_status.selectedChoiceIndex = -1;
    cs_selectChoice(index);
    cs_exec();
  }

  if (pg_game_status.text != NULL) {
    pg_renderText(pg_game_status.text, TEXT_MARGIN, HEADER_HEIGHT + TEXT_MARGIN, _Globals.windowWidth, _Globals.windowHeight - 2 * HEADER_HEIGHT, pg_game_status.scrollY);
    // Scroll down only while there is more text below (pages without a page break
    // or choices, e.g. the stats screen or the last page, must stop at the end)
    pg_game_status.canScrollDown = !pg_game_status.textFullyRendered;
  } else {
    // Empty page (e.g. *finish on an empty screen): nothing to scroll, the button is shown
    pg_game_status.textFullyRendered = 1;
    pg_game_status.canScrollDown = 0;
  }

  if (pg_game_status.showPageBreak) {
    if (pg_renderPageBreak()) {
      pg_game_status.canScrollDown = 0;
    } else {
      pg_game_status.canScrollDown = 1;
    }
  }

  if (pg_game_status.inputTextActive) {
    strcpy(pg_game_status.pageBreakMessage, "Enter text");
    if (pg_renderPageBreak()) {
      pg_game_status.canScrollDown = 0;
    } else {
      pg_game_status.canScrollDown = 1;
    }
  }

  if (pg_game_status.textFullyRendered && pg_game_status.choicesCount > 0) {
    pg_game_status.canScrollDown = 0;
    strcpy(pg_game_status.pageBreakMessage, "Choice");
    pg_renderPageBreak();
  }
  sdlBlitImage(dpad_up_down, 5, _Globals.windowHeight - 30, 30, 30);
}

/// @brief The game state saved while the stats screen is open
static game_status pg_savedGameStatus;
static int pg_statsActive = 0;

void pg_freeTextSurface()
{
  if (text_surface) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }
}

void pg_restoreGameNewPage()
{
  pg_restoreMenu.active = 0;
  // A message to show at the start of the page (e.g. a save of *restore_game that cannot be restored)
  if (pg_pendingNotice != NULL) {
    cs_appendText(pg_pendingNotice, 0);
    cs_appendParagraphBreak();
    free(pg_pendingNotice);
    pg_pendingNotice = NULL;
  }
}

int pg_isStatsActive()
{
  return pg_statsActive;
}

int pg_statsOpen()
{
  if (pg_statsActive)
    return 1;
  if (runningGame == NULL || pg_game_status.fp == NULL)
    return 0;

  char filePath[512];
  snprintf(filePath, sizeof(filePath), "%s/scenes/choicescript_stats.txt", runningGame->path);
  if (!fileExists(filePath)) {
    logInfo("No stats scene: %s", filePath);
    return 0;
  }

  // Save the game state: the stats scene has its own file, text, choices and temporary variables
  pg_savedGameStatus = pg_game_status;
  memset(&pg_game_status, 0, sizeof(pg_game_status));
  pg_game_status.labelsCount = -1;
  pg_game_status.selectedChoiceIndex = -1;
  pg_game_status.sceneIndex = pg_savedGameStatus.sceneIndex;
  if (text_surface) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }
  pg_statsActive = 1;

  if (!cs_openScene("choicescript_stats", 0)) {
    pg_game_status = pg_savedGameStatus;
    pg_statsActive = 0;
    return 0;
  }
  logInfo("Stats screen opened");
  cs_exec();
  return 1;
}

void pg_statsClose()
{
  if (!pg_statsActive)
    return;

  // Free the stats state
  cs_clearGosubStack();
  cs_clearParams();
  cs_clearTempVars();
  cs_clearChoices();
  cs_clear_labels();
  free(pg_game_status.text);
  pg_game_status.text = NULL;
  if (pg_game_status.fp)
    fclose(pg_game_status.fp);
  if (text_surface) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }
  // The images of the game page are loaded again when needed
  pg_clear_image_cache();

  pg_game_status = pg_savedGameStatus;
  memset(&pg_savedGameStatus, 0, sizeof(pg_savedGameStatus));
  pg_statsActive = 0;
  logInfo("Stats screen closed");
}

void pg_init()
{
  pg_clear_image_cache();
  if (text_surface) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }

  char filePath[256];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/scenes/startup.txt");

  pg_game_status.fp = fopen(filePath, "rb");
  if (!pg_game_status.fp) {
    logError("Failed to open file %s", filePath);
    return;
  }
  logInfo("Opened file %s", filePath);
  cs_clear_labels();

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);

  cs_clearChoices();
  pg_game_status.showPageBreak = 0;
  pg_game_status.tempVariables = NULL;
  pg_game_status.text = NULL;
  pg_game_status.sceneIndex = 0;
  snprintf(pg_game_status.sceneName, sizeof(pg_game_status.sceneName), "startup");
  pg_game_status.paramsCount = 0;
  pg_game_status.gameEnded = 0;
  // *hide_reuse in the startup header is read when the game is loaded
  pg_game_status.choiceReuse = runningGame->hide_reuse ? reuse_hide : reuse_allow;
  pg_game_status.loopLimit = 1000;
  pg_game_status.labelVisitsCount = 0;
  pg_loadAchievements();
  pg_game_status.fpPosition = runningGame->startPosition;
  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  pg_game_status.selectedChoiceIndex = -1;
  pg_game_status.gosubCount = 0;
  pg_game_status.used_choices_count = 0;
  pg_game_status.choiceContextsCount = 0;
  pg_game_status.choiceEndsCount = 0;

  logDebug("Position set to %li", pg_game_status.fpPosition);
  fseek(pg_game_status.fp, pg_game_status.fpPosition, SEEK_SET);

  pg_computeLineHeight();
}

/// @brief Line height and scroll step of the text (they depend on the text font)
static void pg_computeLineHeight()
{
  line_height = get_line_height(_Globals.textFont);
  logDebug("Line height %i", line_height);

  if (_Globals.scroll == scroll_page) {
    int lines = (_Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN) / line_height;
    scroll_height = lines * line_height;
  } else {
    scroll_height = line_height;
  }
  logDebug("scroll_height=%i", scroll_height);
}

void pg_refreshLayout()
{
  pg_computeLineHeight();
  // The scroll position depends on the line height: back to the top of the page
  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  if (text_surface) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }
}

void pg_dispose()
{
  pg_freeSnapshot();
  pg_clearCheckpoints();
  cs_clearGosubStack();
  cs_clearParams();
  pg_game_status.gameEnded = 0;
  pg_clear_image_cache();
  cs_clearTempVars();
  cs_clearChoices();
  cs_clear_labels();
  if (text_surface) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }

  pg_game_status.scrollY = 0;
  pg_game_status.textFullyRendered = 0;
  pg_game_status.lastRenderedY = 0;
  pg_game_status.gosubCount = 0;
  pg_game_status.used_choices_count = 0;
  pg_game_status.choiceContextsCount = 0;
  pg_game_status.choiceEndsCount = 0;

  free(pg_game_status.text);
  pg_game_status.text = NULL;

  // The game could be opened but not started (saves page of the game selection)
  if (pg_game_status.fp != NULL)
    fclose(pg_game_status.fp);
  pg_game_status.fp = NULL;
  pg_game_status.fpPosition = 0;

  freeGame(runningGame);
  runningGame = NULL;

  sdlFreeSurface(runningGameIcon);
  runningGameIcon = NULL;
}

void pg_renderTextToSurface(SDL_Surface *surface, const char* text, int scrollY)
{
  if (!surface || text == NULL || strlen(text) == 0)
    return;

  int width = surface->w;
  int height = surface->h;

  char test[5];
  TTF_Font *font = _Globals.textFont;

  pg_game_status.textFullyRendered = 1;
  pg_game_status.lastRenderedY = 0;

  int x = TEXT_MARGIN;
  int y = TEXT_MARGIN - scrollY;
  int text_width = width - 2 * TEXT_MARGIN;
  char *line = NULL;
  int linePos = 0;
  int wrap = 0;
  int last_x = 0;
  while ((line = pg_getNextLine(text, &linePos, text_width, &wrap, &last_x)) != NULL) {
    if (startsWithWord("*stat_bar", line)) {
      int barHeight = 2 * line_height;
      if (y + barHeight > height) {
        pg_game_status.textFullyRendered = 0;
        free(line);
        break;
      }
      y += pg_drawStatBar(surface, line, y);
      x = TEXT_MARGIN;
      free(line);
      continue;
    }

    if (startsWithWord("*text_image", line) || startsWithWord("*image", line)) {
      // Draw image
      // text_image filename.png alignment optional text
      // image filename.png alignment optional text
      y += pg_manageImageCommand(surface, line, y);
      x = TEXT_MARGIN;
      free(line);
      continue;
    }

    if (y < 0) {
      y += line_height;
      free(line);
      continue;
    }

    if (y + line_height > height) {
      pg_game_status.textFullyRendered = 0;
      free(line);
      break;
    }

    int lineLen = strlen(line);
    int pixels_to_justify = 0;
    int pixels_to_justify_first = 0;
    if (_Globals.justify_text && wrap) {
      int delta_x = text_width - last_x;
      if (delta_x > 0) {
        int spaces = 0;
        for (int i = 0; i < lineLen; i++) {
          if (line[i] == ' ')
            spaces++;
        }

        if (spaces > 0) {
          pixels_to_justify = delta_x / spaces;
          pixels_to_justify_first = delta_x - pixels_to_justify * spaces;
        }
      }
    }

    if (lineLen > 0) {
      int i = 0;
      while (i < lineLen) {
        // Check font change:
        if (line[i] == '[') {
          // Italic
          if (i + 2 < lineLen && line[i + 1] == 'i' && line[i + 2] == ']') {
            font = _Globals.textItalicFont;
            i += 3;
            continue;
          } else if (i + 3 < lineLen && line[i + 1] == '/' && line[i + 2] == 'i' && line[i + 3] == ']') {
            font = _Globals.textFont;
            i += 4;
            continue;
          }

          // Bold
          if (i + 2 < lineLen && line[i + 1] == 'b' && line[i + 2] == ']') {
            font = _Globals.textBoldFont;
            i += 3;
            continue;
          } else if (i + 3 < lineLen && line[i + 1] == '/' && line[i + 2] == 'b' && line[i + 3] == ']') {
            font = _Globals.textFont;
            i += 4;
            continue;
          }
        }

        int charBytes = u8seqlen(&line[i]);
        memset(&test, 0, sizeof(test));
        strncpy(test, &line[i], charBytes);

        SDL_Surface *char_surface = TTF_RenderUTF8_Blended(font, test, _theme->textColor);
        SDL_Rect source_rect = { 0, 0, char_surface->w, char_surface->h};
        SDL_Rect dest_rect = { x, y, source_rect.w, source_rect.h };

        if (SDL_BlitSurface(char_surface, &source_rect, surface, &dest_rect)) {
          logError("SDL_BlitSurface Fail: %s", SDL_GetError());
        }
        SDL_FreeSurface(char_surface);
        x += source_rect.w;

        if (charBytes == 1 && line[i] == ' ') {
          if (pixels_to_justify_first) {
            x += pixels_to_justify_first;
            pixels_to_justify_first = 0;
          }
          x += pixels_to_justify;
        }
        pg_game_status.lastRenderedY = y - scrollY + line_height;
        i += charBytes;
      }
    }

    free(line);
    y += line_height;
    x = TEXT_MARGIN;
    if (y - scrollY + line_height > _Globals.windowHeight - HEADER_HEIGHT) {
      pg_game_status.textFullyRendered = 0;
      break;
    }
  }
}

void pg_renderText(const char* text, int x, int y, int width, int height, int scrollY)
{
  if (text_surface == NULL) {
    text_surface = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
    if (text_surface == NULL) {
      logWarning("Failed to create text surface");
      return;
    }
    SDL_FillRect(text_surface, NULL, SDL_MapRGBA(text_surface->format, _theme->backgroundColor.r, _theme->backgroundColor.g, _theme->backgroundColor.b, 0));
    pg_renderTextToSurface(text_surface, text, scrollY);
  }

  sdlBlitImage(text_surface, 0, HEADER_HEIGHT, 0, 0);
}

int pg_renderPageBreak()
{
  if (pg_game_status.textFullyRendered) {
    sdlBlitImage(button_a, _Globals.windowWidth - 35, _Globals.windowHeight - 30, 30, 30);

    int width;
    TTF_MeasureUTF8(_Globals.defaultFont, pg_game_status.pageBreakMessage, _Globals.windowWidth, &width, NULL);
    // Vertically centered in the bottom toolbar
    const int y = sdlTextCenterY(_Globals.defaultFont, _Globals.windowHeight - HEADER_HEIGHT, HEADER_HEIGHT);
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, pg_game_status.pageBreakMessage, _Globals.windowWidth - 35 - width - 5, y, 0, 0);

    return 1;
  }

  return 0;
}

/// @brief Mix two colors
static SDL_Color pg_mixColor(SDL_Color c1, SDL_Color c2, int percent2)
{
  SDL_Color res = {
    (c1.r * (100 - percent2) + c2.r * percent2) / 100,
    (c1.g * (100 - percent2) + c2.g * percent2) / 100,
    (c1.b * (100 - percent2) + c2.b * percent2) / 100,
    255
  };
  return res;
}

static void pg_drawStatText(SDL_Surface *surface, const char *text, int x, int y)
{
  if (!text || !strlen(text))
    return;
  SDL_Surface *textSurface = TTF_RenderUTF8_Blended(_Globals.textFont, text, _theme->textColor);
  if (textSurface == NULL)
    return;
  SDL_Rect dest = { x, y, textSurface->w, textSurface->h };
  SDL_BlitSurface(textSurface, NULL, surface, &dest);
  SDL_FreeSurface(textSurface);
}

static int pg_drawStatBar(SDL_Surface *surface, const char *line, int y)
{
  // *stat_bar value|label[|opposed label]
  char data[1024];
  snprintf(data, sizeof(data), "%s", line + strlen("*stat_bar "));
  char *label = strchr(data, '|');
  char *opposedLabel = NULL;
  if (label != NULL) {
    *label++ = '\0';
    opposedLabel = strchr(label, '|');
    if (opposedLabel != NULL)
      *opposedLabel++ = '\0';
  }
  double value = atof(data);
  if (value < 0)
    value = 0;
  if (value > 100)
    value = 100;

  int barX = TEXT_MARGIN;
  int barWidth = surface->w - 2 * TEXT_MARGIN;

  // Labels on the first line, the opposed label is right aligned
  pg_drawStatText(surface, label, barX, y);
  if (opposedLabel != NULL && strlen(opposedLabel)) {
    int width = 0;
    TTF_MeasureUTF8(_Globals.textFont, opposedLabel, surface->w, &width, NULL);
    pg_drawStatText(surface, opposedLabel, barX + barWidth - width, y);
  }

  // Bar on the second line
  int barHeight = line_height / 2;
  SDL_Rect bar = { barX, y + line_height + (line_height - barHeight) / 2, barWidth, barHeight };
  SDL_Color emptyColor = pg_mixColor(_theme->backgroundColor, _theme->textColor, 20);
  SDL_FillRect(surface, &bar, SDL_MapRGBA(surface->format, emptyColor.r, emptyColor.g, emptyColor.b, 255));
  bar.w = (int)(barWidth * value / 100);
  if (bar.w > 0)
    SDL_FillRect(surface, &bar, SDL_MapRGBA(surface->format, _theme->selectedTextColor.r, _theme->selectedTextColor.g, _theme->selectedTextColor.b, 255));

  return 2 * line_height;
}

int pg_manageImageCommand(SDL_Surface *surface, const char *text, int y)
{
  char imageName[255];
  char alignment[64];

  int start = indexOfWordBreak(text, 0);
  if (start < 0)
    return 0;

  int idx = indexOfWordBreak(text, start + 1);
  if (idx < 0)
    return 0;
  subString(text, imageName, start + 1, idx - start - 1);
  start = idx;
  idx = indexOfWordBreak(text, idx + 1);
  if (idx < 0)
    return 0;
  subString(text, alignment, start + 1, idx - start - 1);

  image_cache_item *image = pg_get_image_from_cache(imageName);
  if (image && image->surface) {
    SDL_Rect destRect = { TEXT_MARGIN, y, image->w, image->h };

    float ratio = (float)destRect.w / (float)destRect.h;
    if (destRect.w > _Globals.windowWidth - 2 * TEXT_MARGIN) {
      destRect.w = _Globals.windowWidth - 2 * TEXT_MARGIN;
      destRect.h = destRect.w / ratio;
    }
    if (destRect.h > _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN) {
      destRect.h = _Globals.windowHeight - 2 * HEADER_HEIGHT - 2 * TEXT_MARGIN;
      destRect.w = destRect.h * ratio;
    }

    SDL_Rect srcRect = { 0, 0, image->w, image->h };

    // Always return multiple of line_height to not break text scrolling
    int res = destRect.h + TEXT_MARGIN;
    int diff = res % line_height;
    if (diff)
      res += line_height - diff;

    if (y < 0) {
      int deltaY = -y;
      if (deltaY >= destRect.h) {
        return res;
      }

      int srcDeltaY = srcRect.h * deltaY / destRect.h;
      srcRect.y += srcDeltaY;
      srcRect.h -= srcDeltaY;

      destRect.y = 0;
      destRect.h -= deltaY;
    }

    if (y + destRect.h > surface->h) {
      int deltaY = y + destRect.h - surface->h;
      int srcDeltaY = srcRect.h * deltaY / destRect.h;
      srcRect.h -= srcDeltaY;
      destRect.h -= deltaY;
    }

    int deltaX = surface->w - destRect.w;
    if (deltaX > 0) {
      if (stricmp(alignment, "center") == 0) {
        destRect.x += deltaX / 2;
      } else if (stricmp(alignment, "right") == 0) {
        destRect.x = deltaX;
      }
    }

    if (destRect.h > 0) {
      SDL_UpperBlitScaled(image->surface, &srcRect, surface, &destRect);
    }
    return res;
  }
  return 0;
}

void pg_manageAchieveCommand(const char *text)
{
  char name[256];
  cs_getCommandArgs(text, name, sizeof(name));

  achievement *ach = NULL;
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (stricmp(runningGame->achievements[i].name, name) == 0) {
      ach = &runningGame->achievements[i];
      break;
    }
  }
  if (ach == NULL) {
    logWarning("Achievement %s not declared in startup", name);
    return;
  }
  if (ach->achieved)
    return;

  ach->achieved = 1;
  pg_saveAchievements();
  const char *description = ach->postEarnedDescription ? ach->postEarnedDescription : ach->preEarnedDescription;
  logInfo("Achievement earned: %s (%s)", ach->name, ach->title);

  // Show the achievement in the text of the page
  char message[1024];
  snprintf(message, sizeof(message), "%s[b]Achievement: %s[/b]%s%s\n\n",
           pg_game_status.text && !endsWith("\n", pg_game_status.text) ? "\n\n" : "",
           ach->title ? ach->title : ach->name,
           description ? " - " : "",
           description ? description : "");
  cs_appendText(message, 0);
}

void pg_clear_image_cache()
{
  for (int i = 0; i < image_cache_count; i++) {
    free(image_cache[i].name);
    image_cache[i].name = NULL;
    SDL_FreeSurface(image_cache[i].surface);
    image_cache[i].surface = NULL;
    image_cache[i].w = 0;
    image_cache[i].h = 0;
  }
  image_cache_count = 0;
}

image_cache_item* pg_get_image_from_cache(const char *name)
{
  for (int i = 0; i < image_cache_count; i++) {
    if (stricmp(image_cache[i].name, name) == 0)
      return &image_cache[i];
  }

  return pg_add_image_to_cache(name);
}

image_cache_item *pg_add_image_to_cache(const char *name)
{
  if (image_cache_count + 1 >= sizeof(image_cache) / sizeof(image_cache_item)) {
    logWarning("Cannot add image to cache (cache full)");
    return NULL;
  }

  if (image_cache[image_cache_count].name != NULL) {
    free(image_cache[image_cache_count].name);
    image_cache[image_cache_count].name = NULL;
  }
  if (image_cache[image_cache_count].surface != NULL) {
    SDL_FreeSurface(image_cache[image_cache_count].surface);
    image_cache[image_cache_count].surface = NULL;
  }

  image_cache[image_cache_count].name = malloc(strlen(name) + 1);
  strcpy(image_cache[image_cache_count].name, name);

  char filePath[256];
  strcpy(filePath, runningGame->path);
  strcat(filePath, "/");
  strcat(filePath, name);
  SDL_Surface* surface = IMG_Load(filePath);
  if (surface == NULL) {
    logWarning("Failed to load image from %s", filePath);
    return NULL;
  }

  image_cache[image_cache_count].w = surface->w;
  image_cache[image_cache_count].h = surface->h;
  image_cache[image_cache_count].surface = surface;
  return &image_cache[image_cache_count++];
}

static void pg_openInputText()
{
  // The keyboard covers the page: show the last paragraph of the text (usually the question)
  char title[512] = "";
  const char *text = pg_game_status.text ? pg_game_status.text : "";
  int end = strlen(text);
  while (end > 0 && isspace((unsigned char)text[end - 1]))
    end--;
  int start = end;
  while (start > 0 && text[start - 1] != '\n')
    start--;
  if (end - start >= (int)sizeof(title)) {
    start = end - (sizeof(title) - 1);
    while (start < end && (text[start] & 0xC0) == 0x80)
      start++;
  }
  memcpy(title, text + start, end - start);
  title[end - start] = '\0';

  // Remove the formatting markup
  const char *markup[] = { "[b]", "[/b]", "[i]", "[/i]" };
  for (int i = 0; i < 4; i++) {
    char *pos;
    while ((pos = strstr(title, markup[i])) != NULL)
      memmove(pos, pos + strlen(markup[i]), strlen(pos + strlen(markup[i])) + 1);
  }

  if (pg_game_status.inputNumber)
    osk_openNumeric(title, "", 20, cs_validateInputNumber);
  else
    osk_open(title, "", pg_game_status.inputTextLong ? 1000 : 100, pg_game_status.inputTextAllowBlank);
}

char* pg_getNextLine(const char *text, int* startPosition, int maxWidth, int *wordWrap, int *last_x)
{
  if (wordWrap != NULL)
    *wordWrap = 0;
  if (last_x != NULL)
    *last_x = 0;

  if (!text || maxWidth <= 0)
    return NULL;

  int textLen = strlen(text);
  if (!textLen || *startPosition >= textLen - 1)
    return NULL;

  char *res = NULL;
  char test[5];
  int width = 0;
  int endOfLinePosition = *startPosition;
  int endOfPreviousWordPosition = -1;
  int previousWordWidth = 0;
  int i = *startPosition;
  int is_command = -1;

  TTF_Font *font = _Globals.textFont;
  while (i < textLen) {
    endOfLinePosition = i;
    if (text[i] == '\n') {
      break;
    }

    if (!isspace(text[i]) && is_command < 0) {
      // If this line contains a command, don't wrap it
      is_command = text[i] == '*' ? 1 : 0;
    }

    // Check font change:
    if (text[i] == '[')  {
      // Italic
      if (i + 2 < textLen && text[i + 1] == 'i' && text[i + 2] == ']') {
        font = _Globals.textItalicFont;
        i += 3;
        endOfLinePosition = i;
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'i' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 4;
        endOfLinePosition = i;
        continue;
      }

      // Bold
      if (i + 2 < textLen && text[i + 1] == 'b' && text[i + 2] == ']') {
        font = _Globals.textBoldFont;
        i += 3;
        endOfLinePosition = i;
        continue;
      } else if (i + 3 < textLen && text[i + 1] == '/' && text[i + 2] == 'b' && text[i + 3] == ']') {
        font = _Globals.textFont;
        i += 4;
        endOfLinePosition = i;
        continue;
      }
    }

    int charBytes = u8seqlen(&text[i]);
    memset(&test, 0, sizeof(test));
    strncpy(test, &text[i], charBytes);
    int charWidth;
    TTF_MeasureUTF8(font, test, _Globals.windowWidth, &charWidth, NULL);
    if (!is_command && width + charWidth > maxWidth) {
      // Wrap line
      if (endOfPreviousWordPosition < 0) {
        // We cannot word-break
        endOfLinePosition--;
      } else {
        endOfLinePosition = endOfPreviousWordPosition;
        if (wordWrap != NULL)
          *wordWrap = 1;
        if (last_x != NULL)
          *last_x = previousWordWidth;
      }
      break;
    }
    width += charWidth;
    if (text[i] == ' ' || text[i] == '\t') {
      endOfPreviousWordPosition = i;
      previousWordWidth = width - charWidth;
    }
    i += charBytes;
    if (i == textLen) {
      endOfLinePosition = i - 1;
    }
  }

  res = malloc(endOfLinePosition - *startPosition + 2);
  int idx = 0;
  for (int j = *startPosition; j <= endOfLinePosition; j++) {
    if (text[j] != '\r' && text[j] != '\n' && (idx > 0 || text[j] != ' '))
      res[idx++] = text[j];
  }
  res[idx] = '\0';

  // Remove trailing spaces
  while (--idx >= 0 && res[idx] == ' ') {
    res[idx] = '\0';
  }

  if (text[endOfLinePosition] == '\n' || text[endOfLinePosition] == ' ') {
    *startPosition = endOfLinePosition + 1;
  } else {
    *startPosition = endOfLinePosition;
  }
  return res;
}

/* ---------------------------------------------------------------------------
 * Save / load
 *
 * As in ChoiceScript, the saved state is the state at the start of the current
 * page: loading a game executes the page again, showing the same text and
 * choices without applying twice the commands of the page (*set...).
 *
 * The saves are text files: ./saves/<game folder>/slot<N>.sav
 * ------------------------------------------------------------------------- */

#define SAVE_HEADER "CoGSDL save 1"

/// @brief Append a string, truncated to the buffer size
static void pg_appendString(char *dst, size_t size, const char *src)
{
  size_t len = strlen(dst);
  if (len < size)
    cs_copyString(dst + len, size - len, src);
}

/// @brief The state at the start of the current page (serialized)
static char *pg_pageSnapshot = NULL;
/// @brief The scene at the start of the current page (used for the name of the save)
static char pg_snapshotScene[256];

static void pg_sbPrintf(cs_strbuf *sb, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len <= 0)
    return;

  char *tmp = malloc(len + 1);
  if (tmp == NULL)
    return;
  va_start(args, format);
  vsnprintf(tmp, len + 1, format, args);
  va_end(args);
  cs_sb_append(sb, tmp, len);
  free(tmp);
}

/// @brief Append a value escaping backslashes and new lines
static void pg_sbEscaped(cs_strbuf *sb, const char *value)
{
  if (value == NULL)
    return;
  for (const char *c = value; *c; c++) {
    if (*c == '\\')
      cs_sb_append(sb, "\\\\", 2);
    else if (*c == '\n')
      cs_sb_append(sb, "\\n", 2);
    else if (*c == '\r')
      cs_sb_append(sb, "\\r", 2);
    else
      cs_sb_append(sb, c, 1);
  }
}

static void pg_unescape(char *value)
{
  char *out = value;
  for (char *c = value; *c; c++) {
    if (*c == '\\' && c[1]) {
      c++;
      *out++ = *c == 'n' ? '\n' : *c == 'r' ? '\r' : *c;
    } else {
      *out++ = *c;
    }
  }
  *out = '\0';
}

static void pg_sbVariable(cs_strbuf *sb, const char *prefix, variable *var)
{
  if (var->name == NULL)
    return;
  pg_sbPrintf(sb, "%s %i %s ", prefix, (int)var->type, var->name);
  pg_sbEscaped(sb, var->value ? var->value : "");
  cs_sb_append(sb, "\n", 1);
}

/// @brief Serialize the state of the game
/// @param position The position where the execution continues
/// @return The state (must be freed)
static char *pg_serializeState(long position)
{
  cs_strbuf sb = { NULL, 0, 0 };
  pg_sbPrintf(&sb, "scene %s\n", pg_game_status.sceneName);
  pg_sbPrintf(&sb, "sceneindex %i\n", pg_game_status.sceneIndex);
  pg_sbPrintf(&sb, "position %li\n", position);
  pg_sbPrintf(&sb, "reuse %i\n", (int)pg_game_status.choiceReuse);
  pg_sbPrintf(&sb, "looplimit %i\n", pg_game_status.loopLimit);
  pg_sbPrintf(&sb, "ended %i\n", pg_game_status.gameEnded);
  // The random generator is saved too: loading shows the same results of *rand
  cs_randomSeed();
  pg_sbPrintf(&sb, "random %llu\n", (unsigned long long)cs_randomState);

  for (int i = 0; i < runningGame->variablesCount; i++)
    pg_sbVariable(&sb, "var", &runningGame->variables[i]);
  for (int i = 0; i < pg_game_status.tempVariablesCount; i++)
    pg_sbVariable(&sb, "temp", &pg_game_status.tempVariables[i]);
  for (int i = 0; i < pg_game_status.paramsCount; i++) {
    cs_sb_append(&sb, "param ", 6);
    pg_sbEscaped(&sb, pg_game_status.params[i]);
    cs_sb_append(&sb, "\n", 1);
  }
  for (int i = 0; i < pg_game_status.used_choices_count; i++)
    pg_sbPrintf(&sb, "used %s\n", pg_game_status.used_choices[i]);
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (runningGame->achievements[i].achieved)
      pg_sbPrintf(&sb, "achieved %s\n", runningGame->achievements[i].name);
  }
  for (int i = 0; i < pg_game_status.choiceContextsCount; i++) {
    choice_context *ctx = &pg_game_status.choiceContexts[i];
    pg_sbPrintf(&sb, "context %li %i %i\n", ctx->endPosition, ctx->indentation, ctx->gosubCount);
  }
  for (int i = 0; i < pg_game_status.choiceEndsCount; i++) {
    choice_end *ce = &pg_game_status.choiceEnds[i];
    pg_sbPrintf(&sb, "choiceend %u %li %li\n", ce->sceneHash, ce->position, ce->endPosition);
  }
  for (int i = 1; i <= pg_game_status.gosubCount; i++) {
    gosub_info *frame = &pg_game_status.gosub[i];
    pg_sbPrintf(&sb, "frame %i %i %li %i %i %i %s\n", i, frame->fp != NULL, frame->position, frame->sceneIndex,
                (int)frame->choiceReuse, frame->loopLimit, frame->fp != NULL ? frame->sceneName : "-");
    for (int j = 0; j < frame->tempVariablesCount; j++) {
      char prefix[32];
      snprintf(prefix, sizeof(prefix), "frametemp %i", i);
      pg_sbVariable(&sb, prefix, &frame->tempVariables[j]);
    }
  }

  return sb.buf;
}

void pg_takeSnapshot()
{
  if (runningGame == NULL || pg_game_status.fp == NULL)
    return;

  // After *finish the scene is ended (fpPosition = fpSize) even if the file is not at its end
  long position = pg_game_status.fpPosition == pg_game_status.fpSize ? pg_game_status.fpSize : ftell(pg_game_status.fp);
  free(pg_pageSnapshot);
  pg_pageSnapshot = pg_serializeState(position);
  cs_copyString(pg_snapshotScene, sizeof(pg_snapshotScene), pg_game_status.sceneName);
}

static void pg_freeSnapshot()
{
  free(pg_pageSnapshot);
  pg_pageSnapshot = NULL;
}

/// @brief Get the folder of the saves of a game (./saves/<game folder>)
static void pg_getGameSaveFolder(const char *gamePath, char *folder, int size)
{
  const char *name = gamePath;
  for (const char *c = gamePath; *c; c++) {
    if ((*c == '/' || *c == '\\') && c[1])
      name = c + 1;
  }
  snprintf(folder, size, "./saves/%s", name);
  // Remove a trailing separator
  int len = strlen(folder);
  if (len > 0 && (folder[len - 1] == '/' || folder[len - 1] == '\\'))
    folder[len - 1] = '\0';
}

/// @brief Get the folder of the saves of the running game (./saves/<game folder>)
static void pg_getSaveFolder(char *folder, int size)
{
  pg_getGameSaveFolder(runningGame->path, folder, size);
}

static void pg_getSavePath(int slot, char *path, int size)
{
  char folder[512];
  pg_getSaveFolder(folder, sizeof(folder));
  snprintf(path, size, "%s/slot%i.sav", folder, slot + 1);
}

static int pg_makeFolder(const char *path)
{
  if (fileExists(path))
    return 1;
#ifdef _WIN32
  return mkdir(path) == 0;
#else
  return mkdir(path, 0755) == 0;
#endif
}

#define ACHIEVEMENTS_HEADER "CoGSDL achievements 1"

static void pg_getAchievementsPath(char *path, int size)
{
  char folder[512];
  pg_getSaveFolder(folder, sizeof(folder));
  snprintf(path, size, "%s/achievements.txt", folder);
}

static void pg_loadAchievements()
{
  if (runningGame == NULL)
    return;

  char path[600];
  pg_getAchievementsPath(path, sizeof(path));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return;

  char line[512];
  int valid = 0;
  int count = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, ACHIEVEMENTS_HEADER) == 0;
      if (!valid)
        break;
      continue;
    }
    for (int i = 0; i < runningGame->achievementsCount; i++) {
      if (stricmp(runningGame->achievements[i].name, line) == 0) {
        runningGame->achievements[i].achieved = 1;
        count++;
        break;
      }
    }
  }
  fclose(fp);
  if (!valid)
    logWarning("Invalid achievements file: %s", path);
  else
    logInfo("Achievements earned: %i (%s)", count, path);
}

static int pg_saveAchievements()
{
  if (runningGame == NULL)
    return 0;

  char folder[512];
  pg_getSaveFolder(folder, sizeof(folder));
  if (!pg_makeFolder("./saves") || !pg_makeFolder(folder)) {
    logError("Cannot create the folder %s", folder);
    return 0;
  }

  char path[600];
  char tmpPath[610];
  pg_getAchievementsPath(path, sizeof(path));
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  FILE *fp = fopen(tmpPath, "wb");
  if (fp == NULL) {
    logError("Cannot write %s", tmpPath);
    return 0;
  }
  int ok = fprintf(fp, "%s\n", ACHIEVEMENTS_HEADER) > 0;
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (runningGame->achievements[i].achieved)
      ok = fprintf(fp, "%s\n", runningGame->achievements[i].name) > 0 && ok;
  }
  ok = (fclose(fp) == 0) && ok;
  if (!ok) {
    logError("Cannot write %s", tmpPath);
    remove(tmpPath);
    return 0;
  }
  remove(path);
  if (rename(tmpPath, path) != 0) {
    logError("Cannot rename %s to %s", tmpPath, path);
    return 0;
  }
  return 1;
}

/// @brief Build the name of a save from the position in the game: scene name and beginning of the page
static void pg_buildSaveName(char *name, int size)
{
  // Text of the page without commands (images, stat bars) and formatting
  cs_strbuf sb = { NULL, 0, 0 };
  const char *text = pg_game_status.text;
  if ((text == NULL || cs_isBlank(text)) && pg_game_status.choicesCount > 0)
    text = pg_game_status.choices[0].text;
  if (text != NULL) {
    int lineStart = 1;
    int lastSpace = 1;
    for (const char *c = text; *c; c++) {
      if (lineStart && *c == '*') {
        // Skip the command line
        while (*c && *c != '\n')
          c++;
        if (!*c)
          break;
        continue;
      }
      lineStart = *c == '\n';
      if (*c == '[') {
        const char *close = strchr(c, ']');
        if (close != NULL && close - c <= 3) {
          c = close;
          continue;
        }
      }
      if (isspace((unsigned char)*c)) {
        if (!lastSpace)
          cs_sb_append(&sb, " ", 1);
        lastSpace = 1;
      } else {
        cs_sb_append(&sb, c, 1);
        lastSpace = 0;
      }
    }
  }

  char excerpt[128] = "";
  if (sb.buf != NULL) {
    trim(sb.buf);
    const int maxLen = 45;
    int len = strlen(sb.buf);
    if (len <= maxLen) {
      snprintf(excerpt, sizeof(excerpt), "%s", sb.buf);
    } else {
      // Cut at a word boundary (or at a UTF-8 character boundary)
      int cut = maxLen;
      while (cut > maxLen / 2 && sb.buf[cut] != ' ')
        cut--;
      if (sb.buf[cut] != ' ') {
        cut = maxLen;
        while (cut > 0 && !isutf(sb.buf[cut]))
          cut--;
      }
      snprintf(excerpt, sizeof(excerpt), "%.*s...", cut, sb.buf);
    }
    free(sb.buf);
  }

  cs_copyString(name, size, pg_snapshotScene);
  if (strlen(excerpt)) {
    pg_appendString(name, size, ": ");
    pg_appendString(name, size, excerpt);
  }
}

/// @brief Read the information of a save file
static int pg_readSaveInfo(const char *path, save_info *info);

int pg_getSaveInfo(int slot, save_info *info)
{
  memset(info, 0, sizeof(save_info));
  if (runningGame == NULL || slot < 0 || slot >= SAVE_SLOTS)
    return 0;

  char path[600];
  pg_getSavePath(slot, path, sizeof(path));
  return pg_readSaveInfo(path, info);
}

int pg_findLastSave(int *gameIndex, int *slot)
{
  int found = 0;
  time_t lastTime = 0;
  for (int g = 0; g < _Globals.gamesCount; g++) {
    char folder[512];
    pg_getGameSaveFolder(_Globals.games[g].path, folder, sizeof(folder));
    for (int s = 0; s < SAVE_SLOTS; s++) {
      char path[600];
      save_info info;
      snprintf(path, sizeof(path), "%s/slot%i.sav", folder, s + 1);
      if (pg_readSaveInfo(path, &info) && (!found || info.time > lastTime)) {
        found = 1;
        lastTime = info.time;
        *gameIndex = g;
        *slot = s;
      }
    }
  }
  return found;
}

int pg_continueLastSave()
{
  int gameIndex, slot;
  if (!pg_findLastSave(&gameIndex, &slot))
    return 0;

  logInfo("Continue: %s, slot %i", _Globals.games[gameIndex].path, slot + 1);
  runningGame = malloc(sizeof(game));
  loadGame(_Globals.games[gameIndex].path, runningGame, 0);
  if (strlen(runningGame->icon) > 0)
    runningGameIcon = IMG_Load(runningGame->icon);

  pg_init();
  if (!pg_loadGame(slot)) {
    logWarning("Continue: cannot load the save");
    pg_dispose();
    return 0;
  }
  return 1;
}

static int pg_readSaveInfo(const char *path, save_info *info)
{
  memset(info, 0, sizeof(save_info));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return 0;

  char line[1024];
  int valid = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, SAVE_HEADER) == 0;
      if (!valid)
        break;
    } else if (startsWith("name ", line)) {
      cs_copyString(info->name, sizeof(info->name), line + 5);
      pg_unescape(info->name);
    } else if (startsWith("time ", line)) {
      info->time = (time_t)atoll(line + 5);
      break;
    }
  }
  fclose(fp);

  info->used = valid;
  return valid;
}

/* ---------------------------------------------------------------------------
 * Checkpoints: *save_checkpoint [slot], *restore_checkpoint [slot]
 * ------------------------------------------------------------------------- */

struct checkpoint
{
  char name[64];
  /// @brief The serialized state
  char *state;
} typedef checkpoint;

static checkpoint pg_checkpoints[16];
static int pg_checkpointsCount = 0;

static checkpoint *pg_getCheckpoint(const char *name)
{
  for (int i = 0; i < pg_checkpointsCount; i++) {
    if (stricmp(pg_checkpoints[i].name, name) == 0)
      return &pg_checkpoints[i];
  }
  return NULL;
}

/// @brief Set a checkpoint (the state is owned by the checkpoint)
static void pg_setCheckpoint(const char *name, char *state)
{
  checkpoint *cp = pg_getCheckpoint(name);
  if (cp == NULL) {
    if (pg_checkpointsCount >= (int)(sizeof(pg_checkpoints) / sizeof(checkpoint))) {
      logWarning("Too many checkpoints: %s not saved", name);
      free(state);
      return;
    }
    cp = &pg_checkpoints[pg_checkpointsCount++];
    cs_copyString(cp->name, sizeof(cp->name), name);
    cp->state = NULL;
  }
  free(cp->state);
  cp->state = state;
}

static void pg_clearCheckpoints()
{
  for (int i = 0; i < pg_checkpointsCount; i++) {
    free(pg_checkpoints[i].state);
    pg_checkpoints[i].state = NULL;
  }
  pg_checkpointsCount = 0;
}

/// @brief Get the checkpoint slot of *save_checkpoint/*restore_checkpoint
/// @param name Set to the checkpoint name
/// @param variable Set to the name of the variable choice_saved_checkpoint[_slot]
/// @return 0 if the slot name is not valid
static int pg_getCheckpointSlot(const char *text, char *name, int nameSize, char *stateVar, int stateVarSize)
{
  char slot[256];
  cs_getCommandArgs(text, slot, sizeof(slot));
  for (char *c = slot; *c; c++) {
    if (!isalnum((unsigned char)*c) && *c != '_') {
      logWarning("Invalid checkpoint slot, must be only letters, numbers, and _ underscores: %s", slot);
      return 0;
    }
    *c = tolower((unsigned char)*c);
  }
  if (strlen(slot)) {
    cs_copyString(name, nameSize, slot);
    snprintf(stateVar, stateVarSize, "choice_saved_checkpoint_%.*s", stateVarSize - 25, slot);
  } else {
    cs_copyString(name, nameSize, "checkpoint");
    cs_copyString(stateVar, stateVarSize, "choice_saved_checkpoint");
  }
  return 1;
}

void pg_manageSaveCheckpointCommand(const char *text)
{
  char name[64];
  char stateVar[128];
  if (pg_statsActive) {
    logWarning("Cannot *save_checkpoint in the stats screen");
    return;
  }
  if (!pg_getCheckpointSlot(text, name, sizeof(name), stateVar, sizeof(stateVar)))
    return;

  // Same as ChoiceScript: the variables are set before saving the state
  addGameVar(runningGame, boolean, stateVar, "true");
  cs_setTempVar("choice_just_restored_checkpoint", "false");
  pg_setCheckpoint(name, pg_serializeState(ftell(pg_game_status.fp)));
  logInfo("Checkpoint %s saved", name);
}

int pg_restoreCheckpoint(const char *text)
{
  char name[64];
  char stateVar[128];
  if (pg_statsActive) {
    logWarning("Cannot *restore_checkpoint in the stats screen");
    return 0;
  }
  if (!pg_getCheckpointSlot(text, name, sizeof(name), stateVar, sizeof(stateVar)))
    return 0;

  checkpoint *cp = pg_getCheckpoint(name);
  if (cp == NULL || cp->state == NULL) {
    logWarning("*restore_checkpoint failed: checkpoint %s not saved (use *if %s)", name, stateVar);
    return 0;
  }

  char *state = cs_strndup(cp->state, strlen(cp->state));
  char **lines;
  int linesCount = pg_splitLines(state, &lines);
  if (!pg_validateState(lines, 0, linesCount)) {
    logError("*restore_checkpoint: invalid checkpoint %s", name);
    free(lines);
    free(state);
    return 0;
  }

  // checkpoint_exclusions: variables that keep their current value
  char *exclusionNames[64];
  char *exclusionValues[64];
  int exclusionIsTemp[64];
  int exclusionsCount = 0;
  variable *exclusions = NULL;
  for (int i = 0; i < pg_game_status.tempVariablesCount && exclusions == NULL; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, "checkpoint_exclusions") == 0)
      exclusions = &pg_game_status.tempVariables[i];
  }
  if (exclusions == NULL)
    exclusions = getGameVarPrimitive("checkpoint_exclusions", runningGame);
  if (exclusions != NULL && exclusions->value != NULL) {
    char *list = cs_strndup(exclusions->value, strlen(exclusions->value));
    for (char *word = strtok(list, " \t"); word != NULL && exclusionsCount < 64; word = strtok(NULL, " \t")) {
      variable *var = NULL;
      int isTemp = 0;
      for (int i = 0; i < pg_game_status.tempVariablesCount && var == NULL; i++) {
        if (stricmp(pg_game_status.tempVariables[i].name, word) == 0) {
          var = &pg_game_status.tempVariables[i];
          isTemp = 1;
        }
      }
      if (var == NULL)
        var = getGameVarPrimitive(word, runningGame);
      if (var == NULL || var->value == NULL)
        continue;
      exclusionNames[exclusionsCount] = cs_strndup(word, strlen(word));
      exclusionValues[exclusionsCount] = cs_strndup(var->value, strlen(var->value));
      exclusionIsTemp[exclusionsCount] = isTemp;
      exclusionsCount++;
    }
    free(list);
  }

  long position = pg_applyState(lines, 0, linesCount);
  free(lines);
  free(state);

  for (int i = 0; i < exclusionsCount; i++) {
    if (exclusionIsTemp[i]) {
      cs_setTempVar(exclusionNames[i], exclusionValues[i]);
    } else {
      variable *var = getGameVarPrimitive(exclusionNames[i], runningGame);
      if (var != NULL) {
        free(var->value);
        var->value = cs_strndup(exclusionValues[i], strlen(exclusionValues[i]));
      }
    }
    free(exclusionNames[i]);
    free(exclusionValues[i]);
  }

  cs_setTempVar("choice_just_restored_checkpoint", "true");
  fseek(pg_game_status.fp, position, SEEK_SET);
  pg_game_status.fpPosition = position;
  logInfo("Checkpoint %s restored", name);
  return 1;
}

/* ---------------------------------------------------------------------------
 * *save_game destination_scene
 *
 * Saves the variables for the next episode of the game, which can import them
 * with *restore_game. The saves of all the games are in ./saves/episodes
 * (the next episode is another game, with another folder).
 * ------------------------------------------------------------------------- */

#define EPISODE_HEADER "CoGSDL episode 1"

void pg_manageSaveGameCommand(const char *text)
{
  char destination[256];
  cs_getCommandArgs(text, destination, sizeof(destination));
  if (strlen(destination) == 0) {
    logWarning("*save_game requires a destination file name, e.g. *save_game Episode2");
    return;
  }

  // Same as ChoiceScript: a game restored from a previous episode is not saved again
  for (int i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, "choice_user_restored") == 0) {
      if (stricmp(pg_game_status.tempVariables[i].value, "true") == 0)
        return;
      break;
    }
  }

  if (!pg_makeFolder("./saves") || !pg_makeFolder("./saves/episodes")) {
    logError("Cannot create the folder ./saves/episodes");
    return;
  }

  char gameFolder[512];
  pg_getSaveFolder(gameFolder, sizeof(gameFolder));
  const char *gameName = gameFolder + strlen("./saves/");

  time_t now = time(NULL);
  char date[64];
  strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&now));
  char name[512];
  cs_copyString(name, sizeof(name), runningGame->title ? runningGame->title : gameName);
  pg_appendString(name, sizeof(name), " - ");
  pg_appendString(name, sizeof(name), date);

  char path[700];
  char tmpPath[710];
  snprintf(path, sizeof(path), "./saves/episodes/%s - %lld.sav", gameName, (long long)now);
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

  cs_strbuf sb = { NULL, 0, 0 };
  pg_sbPrintf(&sb, "%s\n", EPISODE_HEADER);
  pg_sbPrintf(&sb, "game %s\n", gameName);
  cs_sb_append(&sb, "title ", strlen("title "));
  pg_sbEscaped(&sb, runningGame->title ? runningGame->title : gameName);
  cs_sb_append(&sb, "\nname ", strlen("\nname "));
  pg_sbEscaped(&sb, name);
  pg_sbPrintf(&sb, "\ntime %lld\n", (long long)now);
  pg_sbPrintf(&sb, "scene %s\n", destination);
  // Same as ChoiceScript: all the variables (stats), not the temporary ones
  for (int i = 0; i < runningGame->variablesCount; i++)
    pg_sbVariable(&sb, "var", &runningGame->variables[i]);
  pg_sbPrintf(&sb, "end\n");

  FILE *fp = fopen(tmpPath, "wb");
  int ok = fp != NULL && fwrite(sb.buf, 1, sb.len, fp) == sb.len;
  if (fp != NULL)
    ok = (fclose(fp) == 0) && ok;
  free(sb.buf);
  if (!ok || rename(tmpPath, path) != 0) {
    logError("Cannot write %s", path);
    remove(tmpPath);
    cs_appendBlockLine("[b]The game could not be saved for the next episode.[/b]");
    return;
  }

  logInfo("Game saved for the next episode: %s (%s)", path, destination);
  char message[600];
  snprintf(message, sizeof(message), "[b]Game saved for the next episode:[/b] %s", name);
  cs_appendBlockLine(message);
  cs_appendParagraphBreak();
}

/* ---------------------------------------------------------------------------
 * *restore_game [cancel=label]
 *   scene Message shown when the save points to this scene (optional block)
 *
 * Shows the saves of ./saves/episodes (written by *save_game) as the options of
 * a choice, plus "Cancel.". Restoring a save loads its variables and starts its
 * destination scene.
 * ------------------------------------------------------------------------- */

/// @brief Positions of the options of the restore menu (real options have positions >= 0)
#define RESTORE_CANCEL_POSITION (-2)
#define RESTORE_SAVE_POSITION(i) (-100 - (i))

struct episode_info
{
  char path[600];
  char name[512];
  char scene[256];
  long long time;
} typedef episode_info;

static int pg_compareEpisodes(const void *a, const void *b)
{
  long long ta = ((const episode_info *)a)->time;
  long long tb = ((const episode_info *)b)->time;
  return ta < tb ? 1 : ta > tb ? -1 : 0;
}

/// @brief Read the header of a save of ./saves/episodes
static int pg_readEpisodeInfo(const char *path, episode_info *info)
{
  memset(info, 0, sizeof(episode_info));
  cs_copyString(info->path, sizeof(info->path), path);
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return 0;
  char line[1024];
  int valid = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, EPISODE_HEADER) == 0;
      if (!valid)
        break;
    } else if (startsWith("name ", line)) {
      cs_copyString(info->name, sizeof(info->name), line + 5);
      pg_unescape(info->name);
    } else if (startsWith("time ", line)) {
      info->time = atoll(line + 5);
    } else if (startsWith("scene ", line)) {
      cs_copyString(info->scene, sizeof(info->scene), line + 6);
      break;
    }
  }
  fclose(fp);
  return valid && strlen(info->scene) > 0;
}

static void pg_addVirtualChoice(const char *text, long position, int index)
{
  choice *tmp = realloc(pg_game_status.choices, sizeof(choice) * (pg_game_status.choicesCount + 1));
  if (tmp == NULL)
    return;
  pg_game_status.choices = tmp;
  choice *c = &pg_game_status.choices[pg_game_status.choicesCount++];
  memset(c, 0, sizeof(choice));
  snprintf(c->id, sizeof(c->id), "restore_game_%i", index);
  c->text = cs_strndup(text, strlen(text));
  c->position = position;
  c->reusable = 1;
  c->selectable = 1;
}

long pg_manageRestoreGameCommand(const char *text, int indentation)
{
  restore_menu *menu = &pg_restoreMenu;
  memset(menu, 0, sizeof(restore_menu));

  // cancel=label
  char args[512];
  cs_getCommandArgs(text, args, sizeof(args));
  if (startsWith("cancel=", args))
    cs_copyString(menu->cancelLabel, sizeof(menu->cancelLabel), args + strlen("cancel="));
  else if (strlen(args))
    logWarning("Invalid restore_game line: %s", text);

  // Unrestorable scenes: indented block "scene message"
  char line[4096];
  long lineStart;
  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;
    if (getIndentation(line) <= indentation)
      break;
    trim(line);
    int nameEnd = 0;
    while (line[nameEnd] && !isspace((unsigned char)line[nameEnd]))
      nameEnd++;
    if (menu->unrestorableCount < 16) {
      snprintf(menu->unrestorableScenes[menu->unrestorableCount], sizeof(menu->unrestorableScenes[0]), "%.*s", nameEnd, line);
      const char *message = line + nameEnd;
      while (isspace((unsigned char)*message))
        message++;
      cs_copyString(menu->unrestorableMessages[menu->unrestorableCount], sizeof(menu->unrestorableMessages[0]), message);
      menu->unrestorableCount++;
    }
  }
  fseek(pg_game_status.fp, lineStart, SEEK_SET);
  menu->resumePosition = lineStart;

  // The saves, most recent first
  episode_info *episodes = calloc(RESTORE_MAX_SAVES * 4, sizeof(episode_info));
  int episodesCount = 0;
  DIR *dir = opendir("./saves/episodes");
  if (dir != NULL && episodes != NULL) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && episodesCount < RESTORE_MAX_SAVES * 4) {
      if (!endsWith(".sav", entry->d_name))
        continue;
      char path[600];
      snprintf(path, sizeof(path), "./saves/episodes/%.*s", (int)(sizeof(path) - 20), entry->d_name);
      if (pg_readEpisodeInfo(path, &episodes[episodesCount]))
        episodesCount++;
    }
  }
  if (dir != NULL)
    closedir(dir);
  if (episodes != NULL)
    qsort(episodes, episodesCount, sizeof(episode_info), pg_compareEpisodes);

  cs_clearChoices();
  for (int i = 0; i < episodesCount && i < RESTORE_MAX_SAVES; i++) {
    cs_copyString(menu->savePaths[i], sizeof(menu->savePaths[0]), episodes[i].path);
    pg_addVirtualChoice(episodes[i].name, RESTORE_SAVE_POSITION(i), i);
    menu->savesCount++;
  }
  free(episodes);
  if (menu->savesCount == 0)
    cs_appendBlockLine("No saved games from a previous episode.");
  pg_addVirtualChoice("Cancel.", RESTORE_CANCEL_POSITION, RESTORE_MAX_SAVES);

  menu->active = 1;
  pg_game_status.choiceEndPosition = menu->resumePosition;
  return menu->resumePosition;
}

/// @brief Continue after *restore_game (or at the cancel label)
static void pg_restoreGameCancel(int useCancelLabel)
{
  long position = pg_restoreMenu.resumePosition;
  if (useCancelLabel && strlen(pg_restoreMenu.cancelLabel)) {
    label *lbl = cs_get_label(pg_restoreMenu.cancelLabel);
    if (lbl != NULL)
      position = lbl->position;
    else
      logWarning("*restore_game: label %s not found", pg_restoreMenu.cancelLabel);
  }
  fseek(pg_game_status.fp, position, SEEK_SET);
  pg_game_status.fpPosition = position;
}

/// @brief Restore a save of ./saves/episodes: its variables, then its destination scene
static int pg_restoreEpisode(const char *path)
{
  episode_info info;
  if (!pg_readEpisodeInfo(path, &info)) {
    logError("Invalid save: %s", path);
    return 0;
  }

  // Scenes where the save cannot be restored
  for (int i = 0; i < pg_restoreMenu.unrestorableCount; i++) {
    if (stricmp(pg_restoreMenu.unrestorableScenes[i], info.scene) == 0) {
      free(pg_pendingNotice);
      pg_pendingNotice = cs_strndup(pg_restoreMenu.unrestorableMessages[i], strlen(pg_restoreMenu.unrestorableMessages[i]));
      return 0;
    }
  }
  if (!pg_sceneExists(info.scene)) {
    char message[600];
    cs_copyString(message, sizeof(message), "This saved game cannot be restored: the scene ");
    pg_appendString(message, sizeof(message), info.scene);
    pg_appendString(message, sizeof(message), " does not exist.");
    free(pg_pendingNotice);
    pg_pendingNotice = cs_strndup(message, strlen(message));
    return 0;
  }

  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return 0;
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *content = malloc(size + 1);
  if (content == NULL || fread(content, 1, size, fp) != (size_t)size) {
    fclose(fp);
    free(content);
    return 0;
  }
  fclose(fp);
  content[size] = '\0';

  // Variables of the save: the variables of this game with the same name are replaced
  char **lines;
  int linesCount = pg_splitLines(content, &lines);
  int count = 0;
  for (int i = 0; i < linesCount; i++) {
    if (!startsWith("var ", lines[i]))
      continue;
    int type;
    char *name, *value;
    if (!pg_parseSavedVariable(lines[i] + 4, &type, &name, &value))
      continue;
    variable *var = getGameVarPrimitive(name, runningGame);
    if (var == NULL) {
      addGameVar(runningGame, (enum var_types)type, name, value);
    } else {
      free(var->value);
      var->value = cs_strndup(value, strlen(value));
      var->type = (enum var_types)type;
    }
    count++;
  }
  free(lines);
  free(content);

  // Start the destination scene
  cs_clearGosubStack();
  cs_clearParams();
  cs_clearChoiceContexts(0);
  cs_clearTempVars();
  cs_openScene(info.scene, 1);
  cs_setTempVar("choice_user_restored", "true");
  cs_setTempVar("choice_restore_name", info.name);
  logInfo("Restored %s (%i variables), scene %s", path, count, info.scene);
  return 1;
}

/// @brief Manage the selection of an option of the restore menu
/// @return 1 if the option belongs to the restore menu
int pg_selectRestoreOption(choice *c)
{
  if (!pg_restoreMenu.active || c->position >= 0)
    return 0;

  pg_restoreMenu.active = 0;
  if (c->position == RESTORE_CANCEL_POSITION) {
    pg_restoreGameCancel(1);
    return 1;
  }

  int index = RESTORE_SAVE_POSITION(0) - c->position;
  if (index < 0 || index >= pg_restoreMenu.savesCount || !pg_restoreEpisode(pg_restoreMenu.savePaths[index]))
    pg_restoreGameCancel(0);
  return 1;
}

int pg_saveGame(int slot)
{
  if (runningGame == NULL || pg_pageSnapshot == NULL || slot < 0 || slot >= SAVE_SLOTS) {
    logWarning("Nothing to save");
    return 0;
  }

  char folder[512];
  pg_getSaveFolder(folder, sizeof(folder));
  if (!pg_makeFolder("./saves") || !pg_makeFolder(folder)) {
    logError("Cannot create the folder %s", folder);
    return 0;
  }

  char name[256];
  pg_buildSaveName(name, sizeof(name));

  char path[600];
  char tmpPath[610];
  pg_getSavePath(slot, path, sizeof(path));
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

  FILE *fp = fopen(tmpPath, "wb");
  if (fp == NULL) {
    logError("Cannot write %s", tmpPath);
    return 0;
  }

  cs_strbuf sb = { NULL, 0, 0 };
  const char *gameName = folder + strlen("./saves/");
  pg_sbPrintf(&sb, "%s\n", SAVE_HEADER);
  pg_sbPrintf(&sb, "game %s\n", gameName);
  cs_sb_append(&sb, "name ", 5);
  pg_sbEscaped(&sb, name);
  cs_sb_append(&sb, "\n", 1);
  pg_sbPrintf(&sb, "time %lld\n", (long long)time(NULL));
  cs_sb_append(&sb, pg_pageSnapshot, strlen(pg_pageSnapshot));
  for (int i = 0; i < pg_checkpointsCount; i++) {
    if (pg_checkpoints[i].state == NULL)
      continue;
    pg_sbPrintf(&sb, "checkpoint %s ", pg_checkpoints[i].name);
    pg_sbEscaped(&sb, pg_checkpoints[i].state);
    cs_sb_append(&sb, "\n", 1);
  }
  pg_sbPrintf(&sb, "end\n");

  int ok = fwrite(sb.buf, 1, sb.len, fp) == sb.len;
  ok = (fclose(fp) == 0) && ok;
  free(sb.buf);
  if (!ok) {
    logError("Cannot write %s", tmpPath);
    remove(tmpPath);
    return 0;
  }

  // Replace the old save only when the new one is complete
  remove(path);
  if (rename(tmpPath, path) != 0) {
    logError("Cannot rename %s to %s", tmpPath, path);
    return 0;
  }
  logInfo("Game saved in %s (%s)", path, name);
  return 1;
}

static int pg_sceneExists(const char *sceneName)
{
  char path[600];
  snprintf(path, sizeof(path), "%s/scenes/%s.txt", runningGame->path, sceneName);
  return fileExists(path);
}

/// @brief Parse "<type> <name> <value>" of a saved variable
/// @return 1 on success (name and value point inside the line)
static int pg_parseSavedVariable(char *data, int *type, char **name, char **value)
{
  char *space = strchr(data, ' ');
  if (space == NULL)
    return 0;
  *space = '\0';
  *type = atoi(data);
  *name = space + 1;
  space = strchr(*name, ' ');
  if (space == NULL) {
    *value = *name + strlen(*name);
  } else {
    *space = '\0';
    *value = space + 1;
  }
  pg_unescape(*value);
  return strlen(*name) > 0;
}

/// @brief Split a text in lines (the text is modified)
/// @param lines Set to the lines (must be freed)
/// @return The number of lines
static int pg_splitLines(char *content, char ***lines)
{
  int count = 1;
  for (char *c = content; *c; c++) {
    if (*c == '\n')
      count++;
  }
  *lines = malloc(sizeof(char *) * count);
  count = 0;
  char *start = content;
  for (char *c = content; ; c++) {
    if (*c == '\n' || *c == '\0') {
      int last = *c == '\0';
      *c = '\0';
      if (c > start && c[-1] == '\r')
        c[-1] = '\0';
      if (*start)
        (*lines)[count++] = start;
      if (last)
        break;
      start = c + 1;
    }
  }
  return count;
}

/// @brief Check that a saved state can be restored (the scenes exist)
static int pg_validateState(char **lines, int start, int end)
{
  int valid = 1;
  const char *sceneName = NULL;
  for (int i = start; valid && i < end; i++) {
    if (startsWith("scene ", lines[i])) {
      sceneName = lines[i] + 6;
      valid = pg_sceneExists(sceneName);
    } else if (startsWith("frame ", lines[i])) {
      int index, isScene, sceneIndex, reuse, loopLimit;
      long position;
      int nameStart = 0;
      if (sscanf(lines[i] + 6, "%i %i %li %i %i %i %n", &index, &isScene, &position, &sceneIndex, &reuse, &loopLimit, &nameStart) < 6 ||
          index < 1 || index >= (int)(sizeof(pg_game_status.gosub) / sizeof(gosub_info)) ||
          (isScene && !pg_sceneExists(lines[i] + 6 + nameStart)))
        valid = 0;
    }
  }
  return valid && sceneName != NULL;
}

/// @brief Restore a saved state (the lines are modified)
/// @return The position where the execution continues
static long pg_applyState(char **lines, int start, int end)
{
  // Reset the state of the game
  pg_statsClose();
  cs_clearGosubStack();
  cs_clearParams();
  cs_clearTempVars();
  cs_clearChoices();
  pg_game_status.choiceContextsCount = 0;
  pg_game_status.choiceEndsCount = 0;
  pg_game_status.used_choices_count = 0;
  pg_game_status.labelVisitsCount = 0;
  pg_game_status.gameEnded = 0;
  pg_game_status.showPageBreak = 0;
  pg_game_status.scrollY = 0;
  pg_game_status.selectedChoiceIndex = -1;
  free(pg_game_status.text);
  pg_game_status.text = NULL;
  if (text_surface) {
    sdlFreeSurface(text_surface);
    text_surface = NULL;
  }
  // The achievements are not restored: they stay earned (they are saved in achievements.txt)

  // The variables created after the save (e.g. by *check_purchase) are removed
  char *loadedVars = calloc(sizeof(runningGame->variables) / sizeof(variable), 1);

  long position = 0;
  for (int i = start; i < end; i++) {
    char *line = lines[i];
    char *data = strchr(line, ' ');
    if (data == NULL)
      continue;
    *data++ = '\0';

    int type;
    char *name, *value;
    if (strcmp(line, "scene") == 0) {
      cs_openScene(data, 1);
    } else if (strcmp(line, "sceneindex") == 0) {
      pg_game_status.sceneIndex = atoi(data);
    } else if (strcmp(line, "position") == 0) {
      position = atol(data);
    } else if (strcmp(line, "reuse") == 0) {
      pg_game_status.choiceReuse = (enum choice_reuse)atoi(data);
    } else if (strcmp(line, "looplimit") == 0) {
      pg_game_status.loopLimit = atoi(data);
    } else if (strcmp(line, "ended") == 0) {
      pg_game_status.gameEnded = atoi(data);
    } else if (strcmp(line, "random") == 0) {
      cs_randomState = (uint64_t)strtoull(data, NULL, 10);
    } else if (strcmp(line, "var") == 0) {
      if (pg_parseSavedVariable(data, &type, &name, &value)) {
        variable *var = NULL;
        for (int j = 0; j < runningGame->variablesCount; j++) {
          if (stricmp(runningGame->variables[j].name, name) == 0) {
            var = &runningGame->variables[j];
            break;
          }
        }
        if (var == NULL) {
          addGameVar(runningGame, (enum var_types)type, name, value);
          var = getGameVar(name, runningGame);
        } else {
          free(var->value);
          var->value = cs_strndup(value, strlen(value));
          var->type = (enum var_types)type;
        }
        if (var != NULL && loadedVars != NULL)
          loadedVars[var - runningGame->variables] = 1;
      }
    } else if (strcmp(line, "temp") == 0) {
      if (pg_parseSavedVariable(data, &type, &name, &value)) {
        cs_setTempVar(name, value);
        pg_game_status.tempVariables[pg_game_status.tempVariablesCount - 1].type = (enum var_types)type;
      }
    } else if (strcmp(line, "param") == 0) {
      if (pg_game_status.paramsCount < (int)(sizeof(pg_game_status.params) / sizeof(char *))) {
        pg_unescape(data);
        pg_game_status.params[pg_game_status.paramsCount++] = cs_strndup(data, strlen(data));
      }
    } else if (strcmp(line, "used") == 0) {
      if (pg_game_status.used_choices_count < (int)(sizeof(pg_game_status.used_choices) / sizeof(pg_game_status.used_choices[0])))
        snprintf(pg_game_status.used_choices[pg_game_status.used_choices_count++], sizeof(pg_game_status.used_choices[0]), "%s", data);
    } else if (strcmp(line, "achieved") == 0) {
      for (int j = 0; j < runningGame->achievementsCount; j++) {
        if (stricmp(runningGame->achievements[j].name, data) == 0)
          runningGame->achievements[j].achieved = 1;
      }
    } else if (strcmp(line, "choiceend") == 0) {
      if (pg_game_status.choiceEndsCount < (int)(sizeof(pg_game_status.choiceEnds) / sizeof(choice_end))) {
        choice_end *ce = &pg_game_status.choiceEnds[pg_game_status.choiceEndsCount++];
        sscanf(data, "%u %li %li", &ce->sceneHash, &ce->position, &ce->endPosition);
      }
    } else if (strcmp(line, "context") == 0) {
      if (pg_game_status.choiceContextsCount < (int)(sizeof(pg_game_status.choiceContexts) / sizeof(choice_context))) {
        choice_context *ctx = &pg_game_status.choiceContexts[pg_game_status.choiceContextsCount++];
        sscanf(data, "%li %i %i", &ctx->endPosition, &ctx->indentation, &ctx->gosubCount);
      }
    } else if (strcmp(line, "frame") == 0) {
      int index, isScene, sceneIndex, reuse, loopLimit;
      long framePosition;
      int nameStart = 0;
      sscanf(data, "%i %i %li %i %i %i %n", &index, &isScene, &framePosition, &sceneIndex, &reuse, &loopLimit, &nameStart);
      gosub_info *frame = &pg_game_status.gosub[index];
      memset(frame, 0, sizeof(gosub_info));
      frame->position = framePosition;
      frame->sceneIndex = sceneIndex;
      frame->choiceReuse = (enum choice_reuse)reuse;
      frame->loopLimit = loopLimit;
      if (isScene) {
        cs_copyString(frame->sceneName, sizeof(frame->sceneName), data + nameStart);
        char scenePath[600];
        snprintf(scenePath, sizeof(scenePath), "%s/scenes/%s.txt", runningGame->path, frame->sceneName);
        frame->fp = fopen(scenePath, "rb");
        if (frame->fp != NULL) {
          fseek(frame->fp, 0, SEEK_END);
          frame->fpSize = ftell(frame->fp);
          fseek(frame->fp, frame->position, SEEK_SET);
        }
      }
      if (index > pg_game_status.gosubCount)
        pg_game_status.gosubCount = index;
    } else if (strcmp(line, "checkpoint") == 0) {
      // checkpoint <name> <escaped state>
      char *space = strchr(data, ' ');
      if (space != NULL) {
        *space = '\0';
        pg_unescape(space + 1);
        pg_setCheckpoint(data, cs_strndup(space + 1, strlen(space + 1)));
      }
    } else if (strcmp(line, "frametemp") == 0) {
      char *space = strchr(data, ' ');
      if (space == NULL)
        continue;
      *space = '\0';
      int index = atoi(data);
      if (index < 1 || index > pg_game_status.gosubCount)
        continue;
      if (pg_parseSavedVariable(space + 1, &type, &name, &value)) {
        gosub_info *frame = &pg_game_status.gosub[index];
        variable *tmp = realloc(frame->tempVariables, sizeof(variable) * (frame->tempVariablesCount + 1));
        if (tmp == NULL)
          continue;
        frame->tempVariables = tmp;
        variable *var = &frame->tempVariables[frame->tempVariablesCount++];
        var->name = cs_strndup(name, strlen(name));
        var->value = cs_strndup(value, strlen(value));
        var->type = (enum var_types)type;
      }
    }
  }
  if (loadedVars != NULL) {
    int count = 0;
    for (int i = 0; i < runningGame->variablesCount; i++) {
      if (loadedVars[i]) {
        runningGame->variables[count++] = runningGame->variables[i];
      } else {
        logDebug("Variable %s removed (not in the save)", runningGame->variables[i].name);
        free(runningGame->variables[i].name);
        free(runningGame->variables[i].value);
      }
    }
    runningGame->variablesCount = count;
    free(loadedVars);
  }

  return position;
}

int pg_loadGame(int slot)
{
  if (runningGame == NULL || slot < 0 || slot >= SAVE_SLOTS)
    return 0;

  char path[600];
  pg_getSavePath(slot, path, sizeof(path));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    logWarning("Save not found: %s", path);
    return 0;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *content = malloc(size + 1);
  if (content == NULL || fread(content, 1, size, fp) != (size_t)size) {
    fclose(fp);
    free(content);
    return 0;
  }
  fclose(fp);
  content[size] = '\0';

  char **lines;
  int linesCount = pg_splitLines(content, &lines);

  // Validate before changing the state of the game
  int valid = linesCount > 1 && strcmp(lines[0], SAVE_HEADER) == 0 && strcmp(lines[linesCount - 1], "end") == 0 &&
              pg_validateState(lines, 1, linesCount - 1);
  if (!valid) {
    logError("Invalid save: %s", path);
    free(lines);
    free(content);
    return 0;
  }

  // The checkpoints are saved with the game
  pg_clearCheckpoints();
  long position = pg_applyState(lines, 1, linesCount - 1);
  free(lines);
  free(content);

  // Execute the saved page again
  fseek(pg_game_status.fp, position, SEEK_SET);
  pg_game_status.fpPosition = position;
  logInfo("Game loaded from %s", path);
  cs_exec();
  return 1;
}
