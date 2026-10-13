#ifndef GAME_H__
#define GAME_H__

enum var_types
{
  string = 0,
  number = 1,
  boolean = 2,
};

struct variable
{
  char* name;
  char* value;
  enum var_types type;
} typedef variable;

struct label
{
  char* name;
  long position;
} typedef label;

struct achievement
{
  char* name;
  int visible;
  int points;
  char* title;
  char* preEarnedDescription;
  char* postEarnedDescription;
} typedef achievement;

struct choice
{
  /// @brief Choice id
  char id[32];
  /// @brief The choice text
  char *text;
  /// @brief The file position to go if the choice is selected
  long position;
  /// @brief Set to 1 id the choice can be reused
  int reusable;
  /// @brief Set to 1 if the choice has been used
  int used;
  /// @brief Set to 0 if the choice is shown but cannot be selected (*selectable_if)
  int selectable;
  /// @brief Set to 1 if the choice is disabled after it has been used (*disable_reuse)
  int disable_reuse;
} typedef choice;

struct game
{
  char path[256];
  char icon[256];
  char background[256];
  char *title;
  char *author;
  char *ifid;

  int scenesCount;
  char scenes[1024][256];

  int variablesCount;
  variable variables[4096];

  int achievementsCount;
  achievement achievements[4096];

  long startPosition;
  int hide_reuse;
} typedef game;

#endif