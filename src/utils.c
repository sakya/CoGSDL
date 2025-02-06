#include "CoGSDL.h"

int indexOf(const char *string, char separator)
{
  if (!string || separator == '\0')
      return -1;

  int idx = 0;
  for (;*string; string++) {
    if (*string == separator)
        return idx;
    idx++;
  }
  return -1;
}

int startsWith(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

void subString(const char *source, char *target, int pos, int length) {
    int i = 0;

    while (i < length) {
        target[i] = source[pos + i];
        i++;
    }

    target[i] = '\0';
}