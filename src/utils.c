#include "CoGSDL.h"

int indexOfWordBreak(const char *string, const int startFrom)
{
  if (!string)
      return -1;

  int idx = startFrom;
  int len = strlen(string);
  if (startFrom >= len -1)
    return -1;
  while (idx < len) {
    if (string[idx] == ' ' || string[idx] == '\t')
      return idx;
    idx++;
  }

  return idx;
}

int indexOf(const char *string, const char character, const int startFrom)
{
  if (!string || character == '\0')
      return -1;

  int idx = startFrom;
  int len = strlen(string);
  while (idx < len) {
    if (string[idx] == character)
      return idx;
    idx++;
  }

  return -1;
}

int lastIndexOf(const char *string, const char character, const int startFrom)
{
  if (!string || character == '\0')
      return -1;

  int idx = startFrom;
  while (idx >= 0) {
    if (string[idx] == character)
      return idx;
    idx--;
  }

  return -1;
}

int startsWith(const char *prefix, const char *string)
{
  return strncmp(prefix, string, strlen(prefix)) == 0 ? 1 : 0;
}

int endsWith(const char *suffix, const char *string)
{
    if (!string || !suffix)
      return 0;
    int lenstr = strlen(string);
    int lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
      return 0;
    return strncmp(string + lenstr - lensuffix, suffix, lensuffix) == 0 ? 1 : 0;
}

int startsWithWord(const char *prefix, const char *string)
{
  char *test = malloc(strlen(prefix) + 1);
  snprintf(test, strlen(prefix) + 1, "%s ", prefix);
  if (startsWith(test, string)) {
    free(test);
    test = NULL;
    return 1;
  }

  snprintf(test, strlen(prefix) + 1, "%s\t", prefix);
  if (startsWith(test, string)) {
    free(test);
    test = NULL;
    return 1;
  }

  free(test);
  test = NULL;
  return 0;
}

void subString(const char *source, char *target, const int pos, const int length) {
  int i = 0;
  int sourceLen = strlen(source);
  while (i < length) {
    if (pos + i >= sourceLen)
      break;

    target[i] = source[pos + i];
    i++;
  }

  target[i] = '\0';
}

void trim(char *string) {
  int start = 0, end = strlen(string) - 1;

  // Remove leading whitespace
  while (isspace(string[start])) {
    start++;
  }

  // Remove trailing whitespace
  while (end > start && isspace(string[end])) {
    end--;
  }

  // If the string was trimmed, adjust the null terminator
  if (start > 0 || end < (strlen(string) - 1)) {
    memmove(string, string + start, end - start + 1);
    string[end - start + 1] = '\0';
  }
}

int getIndentation(const char *string)
{
  if (string == NULL)
    return 0;

  int start = 0;
  int end = strlen(string);
  int res = 0;
  while (start < end) {
    if (string[start] == ' ') {
      res++;
    } else if (string[start] == '\t') {
      res += TAB_SIZE;
    } else {
      break;
    }
    start++;
  }

  return res;
}

int isEmpty(const char *string)
{
  int start = 0, end = strlen(string) - 1;
  while(start < end) {
    if (!isspace(string[start]))
      return 0;
    start++;
  }
  return 1;
}

int fileExists (const char *filename) {
  struct stat buffer;
  return (stat(filename, &buffer) == 0) ? 1 : 0;
}

void sdlDrawSelection(int y, int height)
{
  SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);
  SDL_Rect rect = { 5, y, _Globals.windowWidth - 10, height };
  SDL_RenderFillRect(_Globals.renderer, &rect);
}

int sdlSelectionHeight()
{
  return TTF_FontHeight(_Globals.defaultFont) + 4;
}

/* ---------------------------------------------------------------------------
 * Texture caches
 *
 * Creating a texture from a surface (and scaling it) every frame is slow with
 * the software renderer: the textures of the images and of the texts are kept
 * between the frames and released when they are not used for a while.
 * ------------------------------------------------------------------------- */

#define IMAGE_CACHE_SIZE 64
#define TEXT_CACHE_SIZE 128
/// @brief Frames before an unused texture is released
#define IMAGE_CACHE_MAX_AGE 600
#define TEXT_CACHE_MAX_AGE 120

struct image_cache_entry
{
  SDL_Surface *surface;
  /// @brief Used to detect a different surface allocated at the same address
  void *pixels;
  int surfaceW;
  int surfaceH;
  int w;
  int h;
  SDL_Texture *texture;
  Uint32 lastUsed;
} typedef image_cache_entry;

struct text_cache_entry
{
  TTF_Font *font;
  Uint32 color;
  char *text;
  SDL_Texture *texture;
  int w;
  int h;
  Uint32 lastUsed;
} typedef text_cache_entry;

static image_cache_entry imageCache[IMAGE_CACHE_SIZE];
static text_cache_entry textCache[TEXT_CACHE_SIZE];
static Uint32 cacheFrame = 1;

static void sdlClearImageEntry(image_cache_entry *e)
{
  if (e->texture)
    SDL_DestroyTexture(e->texture);
  memset(e, 0, sizeof(image_cache_entry));
}

static void sdlClearTextEntry(text_cache_entry *e)
{
  if (e->texture)
    SDL_DestroyTexture(e->texture);
  free(e->text);
  memset(e, 0, sizeof(text_cache_entry));
}

SDL_Texture *sdlGetImageTexture(SDL_Surface *surface, int width, int height)
{
  if (surface == NULL || width <= 0 || height <= 0)
    return NULL;

  int freeIdx = -1;
  int oldestIdx = -1;
  for (int i = 0; i < IMAGE_CACHE_SIZE; i++) {
    image_cache_entry *e = &imageCache[i];
    if (e->texture == NULL) {
      if (freeIdx < 0)
        freeIdx = i;
      continue;
    }
    if (e->surface == surface && e->w == width && e->h == height) {
      if (e->pixels == surface->pixels && e->surfaceW == surface->w && e->surfaceH == surface->h) {
        e->lastUsed = cacheFrame;
        return e->texture;
      }
      // Another surface at the same address
      sdlClearImageEntry(e);
      if (freeIdx < 0)
        freeIdx = i;
      continue;
    }
    if (oldestIdx < 0 || e->lastUsed < imageCache[oldestIdx].lastUsed)
      oldestIdx = i;
  }

  int idx = freeIdx >= 0 ? freeIdx : oldestIdx;
  sdlClearImageEntry(&imageCache[idx]);

  // Scale the image once (scaling it at every frame is slow)
  SDL_Surface *scaled = NULL;
  if (width != surface->w || height != surface->h) {
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    scaled = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (converted == NULL || scaled == NULL || SDL_SoftStretchLinear(converted, NULL, scaled, NULL) < 0) {
      SDL_FreeSurface(scaled);
      scaled = NULL;
    }
    SDL_FreeSurface(converted);
  }
  SDL_Texture *texture = SDL_CreateTextureFromSurface(_Globals.renderer, scaled ? scaled : surface);
  SDL_FreeSurface(scaled);
  if (texture == NULL) {
    logWarning("Failed to create texture for image: %s", SDL_GetError());
    return NULL;
  }

  image_cache_entry *e = &imageCache[idx];
  e->surface = surface;
  e->pixels = surface->pixels;
  e->surfaceW = surface->w;
  e->surfaceH = surface->h;
  e->w = width;
  e->h = height;
  e->texture = texture;
  e->lastUsed = cacheFrame;
  return texture;
}

void sdlFreeSurface(SDL_Surface *surface)
{
  if (surface == NULL)
    return;
  for (int i = 0; i < IMAGE_CACHE_SIZE; i++) {
    if (imageCache[i].texture && imageCache[i].surface == surface)
      sdlClearImageEntry(&imageCache[i]);
  }
  SDL_FreeSurface(surface);
}

void sdlCacheEndFrame()
{
  cacheFrame++;
  for (int i = 0; i < IMAGE_CACHE_SIZE; i++) {
    if (imageCache[i].texture && cacheFrame - imageCache[i].lastUsed > IMAGE_CACHE_MAX_AGE)
      sdlClearImageEntry(&imageCache[i]);
  }
  for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
    if (textCache[i].texture && cacheFrame - textCache[i].lastUsed > TEXT_CACHE_MAX_AGE)
      sdlClearTextEntry(&textCache[i]);
  }
}

void sdlCacheClear()
{
  for (int i = 0; i < IMAGE_CACHE_SIZE; i++)
    sdlClearImageEntry(&imageCache[i]);
  for (int i = 0; i < TEXT_CACHE_SIZE; i++)
    sdlClearTextEntry(&textCache[i]);
}

/// @brief Get the (cached) texture of a text
static text_cache_entry *sdlGetTextTexture(TTF_Font *font, SDL_Color color, const char *text)
{
  const Uint32 packedColor = ((Uint32)color.r << 24) | ((Uint32)color.g << 16) | ((Uint32)color.b << 8) | color.a;
  int freeIdx = -1;
  int oldestIdx = -1;
  for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
    text_cache_entry *e = &textCache[i];
    if (e->texture == NULL) {
      if (freeIdx < 0)
        freeIdx = i;
      continue;
    }
    if (e->font == font && e->color == packedColor && strcmp(e->text, text) == 0) {
      e->lastUsed = cacheFrame;
      return e;
    }
    if (oldestIdx < 0 || e->lastUsed < textCache[oldestIdx].lastUsed)
      oldestIdx = i;
  }

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
  if (surface == NULL) {
    logWarning("Failed to create surface for text");
    return NULL;
  }
  SDL_Texture *texture = SDL_CreateTextureFromSurface(_Globals.renderer, surface);
  if (texture == NULL) {
    SDL_FreeSurface(surface);
    logWarning("Failed to create texture for text");
    return NULL;
  }

  text_cache_entry *e = &textCache[freeIdx >= 0 ? freeIdx : oldestIdx];
  sdlClearTextEntry(e);
  e->font = font;
  e->color = packedColor;
  e->text = strdup(text);
  e->texture = texture;
  e->w = surface->w;
  e->h = surface->h;
  e->lastUsed = cacheFrame;
  SDL_FreeSurface(surface);
  return e;
}

int sdlWriteText(TTF_Font* font, SDL_Color color, char* text, int x, int y, int maxWidth, int maxHeight)
{
  if (!text || !font || !strlen(text))
    return 0;
  text_cache_entry *e = sdlGetTextTexture(font, color, text);
  if (e == NULL)
    return 0;

  SDL_Rect rect = { x, y, maxWidth > 0 && maxWidth < e->w ? maxWidth : e->w, maxHeight > 0 && maxHeight < e->h ? maxHeight : e->h };
  SDL_RenderCopy(_Globals.renderer, e->texture, NULL, &rect);
  return rect.w;
}

void sdlBlitImageFile(char* image, int x, int y, int maxWidth, int maxHeight)
{
  SDL_Surface *surface = IMG_Load(image);
  if (surface == NULL)
    return;

  sdlBlitImage(surface, x, y, maxWidth, maxHeight);
  SDL_FreeSurface(surface);
}

void sdlBlitImage(SDL_Surface *surface, int x, int y, int maxWidth, int maxHeight)
{
  if (surface == NULL)
    return;

  SDL_Rect rect = { x, y, surface->w, surface->h };
  float ratio = (float)rect.w / (float)rect.h;
  if (maxWidth > 0 && rect.w > maxWidth) {
    rect.w = maxWidth;
    rect.h = rect.w / ratio;
  }
  if (maxHeight > 0 && rect.h > maxHeight) {
    rect.h = maxHeight;
    rect.w = rect.h * ratio;
  }

  SDL_Texture *texture = sdlGetImageTexture(surface, rect.w, rect.h);
  if (texture)
    SDL_RenderCopy(_Globals.renderer, texture, NULL, &rect);
}

int get_line_height(TTF_Font *font)
{
  if (font == NULL)
    return 0;

  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, "H", _theme->textColor);
  if (surface == NULL) {
    logWarning("Failed to create surface");
    return 0;
  }
  int res = surface->h;
  SDL_FreeSurface(surface);
  surface = NULL;

  return res;
}

int is_number(const char *text, int *value)
{
  if (!text)
    return 0;

  int end = strlen(text);
  for (int i = 0; i < end; i++) {
    if ((text[i] == '-' || text[i] == '+') && i > 0)
      return 0;

    if (!isdigit(text[i]) && text[i] != '.' && text[i] != '-' && text[i] != '+')
      return 0;
  }

  if (value != NULL)
    *value = atoi(text);
  return 1;
}

int stricmp(const char *s1, const char *s2)
{
  int s1_len = strlen(s1);
  int s2_len = strlen(s2);
  for (int i = 0; i < s1_len; i++) {
    if (i == s2_len)
      return 1;

    char char1 = tolower(s1[i]);
    char char2 = tolower(s2[i]);
    if (char1 > char2)
      return 1;
    if (char1 < char2)
      return -1;
  }

  if (s2_len > s1_len) {
    return -1;
  }

  return 0;
}