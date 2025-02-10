#include "CoGSDL.h"

int indexOfWordBreak(const char *string, const int startFrom)
{
  if (!string)
      return -1;

  int idx = startFrom;
  int len = strlen(string);
  while (idx < len) {
    if (string[idx] == ' ' || string[idx] == '\t')
      return idx;
    idx++;
  }

  return -1;
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

int startsWith(const char *prefix, const char *string)
{
  return strncmp(prefix, string, strlen(prefix)) == 0 ? 1 : 0;
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
  return (stat (filename, &buffer) == 0) ? 1 : 0;
}

void sdlWriteText(TTF_Font* font, SDL_Color color, char* text, int x, int y, int maxWidth, int maxHeight)
{
  SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
  SDL_Texture* texture = SDL_CreateTextureFromSurface(_Globals.renderer, surface);

  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = maxWidth > 0 && maxWidth < surface->w ? maxWidth : surface->w;
  rect.h = maxHeight > 0 && maxHeight < surface->h ? maxHeight : surface->h;

  SDL_RenderCopyEx(_Globals.renderer, texture, NULL, &rect, 0, NULL, SDL_FLIP_NONE);

  SDL_FreeSurface(surface);
  SDL_DestroyTexture(texture);
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
  SDL_Texture* texture = SDL_CreateTextureFromSurface(_Globals.renderer, surface);

  SDL_Rect rect;
  rect.x = x;
  rect.y = y;
  rect.w = surface->w;
  rect.h = surface->h;

  float ratio = rect.w / rect.h;
  if (rect.w > maxWidth) {
    rect.w = maxWidth;
    rect.h = rect.w / ratio;
  }
  if (rect.h > maxHeight) {
    rect.h = maxHeight;
    rect.w = rect.h * ratio;
  }

  SDL_RenderCopyEx(_Globals.renderer, texture, NULL, &rect, 0, NULL, SDL_FLIP_NONE);
  SDL_DestroyTexture(texture);
}