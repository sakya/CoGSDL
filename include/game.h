struct variable
{
  char* name;
  char* value;

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

struct game
{
  char path[255];
  char icon[255];
  char* title;
  char* author;

  int scenesCount;
  char scenes[1024][256];

  int variablesCount;
  variable variables[1024];

  int achievementsCount;
  achievement achievements[1024];

  int labelsCount;
  label labels[1024];
} typedef game;

struct section
{
  char path[255];

  label labels[1024];
} typedef section;