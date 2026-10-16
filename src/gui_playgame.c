#include "CoGSDL.h"
#include "gui_playgame.h"
#include "choicescript.h"
#include "saves.h"

void pg_init();
void pg_dispose();
void pg_renderText(const char* text, int x, int y, int width, int height, int scrollY);
int pg_renderPageBreak();
static void pg_computeLineHeight();
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
  sv_loadAchievements();
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
  sv_freeSnapshot();
  sv_clearCheckpoints();
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
