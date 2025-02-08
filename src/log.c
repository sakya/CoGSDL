#include "CoGSDL.h"

FILE *log_fp;

int logInit(const char* filePath)
{
  setvbuf(stdout, NULL, _IONBF, 0);
  log_fp = fopen(filePath, "w");
  if (log_fp == NULL)
      return -1;

  return 0;
}

void logInfo(const char *format, ...)
{
  char msg[1024];

  va_list args;
  va_start(args, format);
  vsnprintf(msg, sizeof(msg), format, args);
  va_end(args);

  if (log_fp != NULL) {
    fwrite(msg, sizeof(char), strlen(msg), log_fp);
    fflush(log_fp);
  }
  printf(msg);
}