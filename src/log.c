#include "CoGSDL.h"

static FILE *log_fp;

int logInit(const char* filePath)
{
  setvbuf(stdout, NULL, _IONBF, 0);
  log_fp = fopen(filePath, "wb");
  if (log_fp == NULL)
      return -1;

  return 0;
}

void logDispose()
{
  if (log_fp != NULL) {
    fclose(log_fp);
    log_fp = NULL;
  }
}

void logPrimitive(const char* level, const char *format, va_list args)
{
  char msg[4096];
  char timeString[256];
  char levelString[20];
  time_t current_time;
  struct tm *info;

  vsnprintf(msg, sizeof(msg), format, args);

  current_time = time(NULL);
  info = localtime(&current_time);
  strftime(timeString, sizeof(timeString), "[%Y-%m-%dT%H:%M:%S]", info);
  snprintf(levelString, sizeof(levelString), "[%s]", level);

  if (log_fp != NULL) {
    fwrite(timeString, sizeof(char), strlen(timeString), log_fp);
    fwrite(levelString, sizeof(char), strlen(levelString), log_fp);
    fwrite(msg, sizeof(char), strlen(msg), log_fp);
    fflush(log_fp);
  }
  printf(timeString);
  printf(levelString);
  printf(msg);
}

void logDebug(const char *format, ...)
{
  if (_Globals.logLevel > Debug)
    return;
  va_list args;
  va_start(args, format);
  logPrimitive("DBG", format, args);
  va_end(args);
}

void logInfo(const char *format, ...)
{
  if (_Globals.logLevel > Info)
    return;
  va_list args;
  va_start(args, format);
  logPrimitive("INF", format, args);
  va_end(args);
}

void logWarning(const char *format, ...)
{
  if (_Globals.logLevel > Warning)
    return;
  va_list args;
  va_start(args, format);
  logPrimitive("WRN", format, args);
  va_end(args);
}

void logError(const char *format, ...)
{
  if (_Globals.logLevel > Error)
    return;

  va_list args;
  va_start(args, format);
  logPrimitive("ERR", format, args);
  va_end(args);
}