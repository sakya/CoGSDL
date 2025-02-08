#include "CoGSDL.h"

int indexOfWordBreak(const char *string, const int startFrom)
{
  int wIdx = indexOf(string, ' ', startFrom);
  int tIdx = indexOf(string, '\t', startFrom);
  if (tIdx == -1)
    return wIdx;

  return wIdx < tIdx ? wIdx : tIdx;
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