#include "CoGSDL.h"

int indexOf(const char *string, char character, int startFrom)
{
  if (!string || character == '\0')
      return -1;

  int idx = startFrom;
  string += startFrom;
  for (;*string; string++) {
    if (*string == character)
        return idx;
    idx++;
  }
  return -1;
}

int startsWith(const char *prefix, const char *string)
{
    return strncmp(prefix, string, strlen(prefix)) == 0 ? 1 : 0;
}

void subString(const char *source, char *target, int pos, int length) {
    int i = 0;

    while (i < length) {
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

int fileExists (char *filename) {
  struct stat buffer;
  return (stat (filename, &buffer) == 0) ? 1 : 0;
}