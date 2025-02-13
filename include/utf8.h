#ifndef UTF8_H__
#define UTF8_H__

#include "string.h"

#define isutf(c) (((c)&0xC0)!=0x80)

/// @brief Count the number of characters in a UTF-8 string
/// @param s The string
/// @return Then number of characters of the string
int u8strlen(const char *s);

/// @brief Return the next character, updating an index variable
/// @param s The string
/// @param i The index
/// @return The next character
unsigned int u8nextchar(const char *s, int *i);

/// @brief Returns length of next utf-8 sequence
/// @param s The string
/// @return The length of next utf-8 sequence
unsigned int u8seqlen(const char *s);
 #endif