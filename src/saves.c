/* ---------------------------------------------------------------------------
 * Saves of the running game
 *
 * Save slots (save/load of the state), checkpoints (*save_checkpoint,
 * *restore_checkpoint), episodes (*save_game, *restore_game) and achievements
 * (*achieve).
 * ------------------------------------------------------------------------- */

#include "CoGSDL.h"
#include "gui_playgame.h"
#include "choicescript.h"
#include "saves.h"

static void sv_setCheckpoint(const char *name, char *state);
variable* getGameVarPrimitive(const char *varName, game* game);
static int sv_splitLines(char *content, char ***lines);
static int sv_validateState(char **lines, int start, int end);
static long sv_applyState(char **lines, int start, int end);
/// @brief Save the earned achievements
static int sv_saveAchievements();
static int sv_sceneExists(const char *sceneName);
static int sv_parseSavedVariable(char *data, int *type, char **name, char **value);
static void sv_unescape(char *value);

#define RESTORE_MAX_SAVES 20
struct restore_menu
{
  int active;
  char cancelLabel[256];
  /// @brief Where the execution continues after *restore_game (and its block)
  long resumePosition;
  int savesCount;
  char savePaths[RESTORE_MAX_SAVES][600];
  int unrestorableCount;
  char unrestorableScenes[16][128];
  char unrestorableMessages[16][512];
} typedef restore_menu;

/// @brief The menu of *restore_game
static restore_menu sv_restoreMenu;
/// @brief A message shown at the start of the next page
static char *sv_pendingNotice = NULL;

void sv_restoreGameNewPage()
{
  sv_restoreMenu.active = 0;
  // A message to show at the start of the page (e.g. a save of *restore_game that cannot be restored)
  if (sv_pendingNotice != NULL) {
    cs_appendText(sv_pendingNotice, 0);
    cs_appendParagraphBreak();
    free(sv_pendingNotice);
    sv_pendingNotice = NULL;
  }
}

void sv_manageAchieveCommand(const char *text)
{
  char name[256];
  cs_getCommandArgs(text, name, sizeof(name));

  achievement *ach = NULL;
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (stricmp(runningGame->achievements[i].name, name) == 0) {
      ach = &runningGame->achievements[i];
      break;
    }
  }
  if (ach == NULL) {
    logWarning("Achievement %s not declared in startup", name);
    return;
  }
  if (ach->achieved)
    return;

  ach->achieved = 1;
  sv_saveAchievements();
  const char *description = ach->postEarnedDescription ? ach->postEarnedDescription : ach->preEarnedDescription;
  logInfo("Achievement earned: %s (%s)", ach->name, ach->title);

  // Show the achievement in the text of the page
  char message[1024];
  snprintf(message, sizeof(message), "%s[b]Achievement: %s[/b]%s%s\n\n",
           pg_game_status.text && !endsWith("\n", pg_game_status.text) ? "\n\n" : "",
           ach->title ? ach->title : ach->name,
           description ? " - " : "",
           description ? description : "");
  cs_appendText(message, 0);
}

/* ---------------------------------------------------------------------------
 * Save / load
 *
 * As in ChoiceScript, the saved state is the state at the start of the current
 * page: loading a game executes the page again, showing the same text and
 * choices without applying twice the commands of the page (*set...).
 *
 * The saves are text files: ./saves/<game folder>/slot<N>.sav
 * ------------------------------------------------------------------------- */

#define SAVE_HEADER "CoGSDL save 1"

/* Play time: the sum of the time of all the game sessions. A session starts when the
 * game is started (or loaded from a save) and ends when the player saves (then a new
 * session starts) */

/// @brief Play time of the previous sessions (milliseconds)
static unsigned long long sv_playTimeBase = 0;
/// @brief Start of the current session (SDL_GetTicks)
static Uint32 sv_sessionStart = 0;

void sv_startPlayTime(unsigned long long playTime)
{
  sv_playTimeBase = playTime * 1000ULL;
  sv_sessionStart = SDL_GetTicks();
}

/// @brief Play time in milliseconds
static unsigned long long sv_getPlayTimeMs()
{
  return sv_playTimeBase + (Uint32)(SDL_GetTicks() - sv_sessionStart);
}

unsigned long long sv_getPlayTime()
{
  return sv_getPlayTimeMs() / 1000ULL;
}

int sv_mostPlayedSlot(const save_info *slots)
{
  int best = -1;
  for (int i = 0; i < SAVE_SLOTS; i++) {
    if (!slots[i].used)
      continue;
    if (best < 0 || slots[i].playTime > slots[best].playTime ||
        (slots[i].playTime == slots[best].playTime && slots[i].time > slots[best].time))
      best = i;
  }
  return best;
}

void sv_formatSaveDetails(const save_info *info, char *text, int size)
{
  unsigned long long minutes = info->playTime / 60ULL;
  char date[32];
  const struct tm* localTime = localtime(&info->time);
  if (localTime->tm_year + 1900 > 1970) {
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localTime);
    if (minutes >= 60)
      snprintf(text, size, "%lluh %llum - %s", minutes / 60ULL, minutes % 60ULL, date);
    else
      snprintf(text, size, "%llum - %s", minutes, date);
  } else {
    // No RTC, skip the time
    if (minutes >= 60)
      snprintf(text, size, "%lluh %llum", minutes / 60ULL, minutes % 60ULL);
    else
      snprintf(text, size, "%llum", minutes);
  }
}

/// @brief Append a string, truncated to the buffer size
static void sv_appendString(char *dst, size_t size, const char *src)
{
  size_t len = strlen(dst);
  if (len < size)
    cs_copyString(dst + len, size - len, src);
}

/// @brief The state at the start of the current page (serialized)
static char *sv_pageSnapshot = NULL;
/// @brief The scene at the start of the current page (used for the name of the save)
static char sv_snapshotScene[256];

static void sv_sbPrintf(cs_strbuf *sb, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  int len = vsnprintf(NULL, 0, format, args);
  va_end(args);
  if (len <= 0)
    return;

  char *tmp = malloc(len + 1);
  if (tmp == NULL)
    return;
  va_start(args, format);
  vsnprintf(tmp, len + 1, format, args);
  va_end(args);
  cs_sb_append(sb, tmp, len);
  free(tmp);
}

/// @brief Append a value escaping backslashes and new lines
static void sv_sbEscaped(cs_strbuf *sb, const char *value)
{
  if (value == NULL)
    return;
  for (const char *c = value; *c; c++) {
    if (*c == '\\')
      cs_sb_append(sb, "\\\\", 2);
    else if (*c == '\n')
      cs_sb_append(sb, "\\n", 2);
    else if (*c == '\r')
      cs_sb_append(sb, "\\r", 2);
    else
      cs_sb_append(sb, c, 1);
  }
}

static void sv_unescape(char *value)
{
  char *out = value;
  for (char *c = value; *c; c++) {
    if (*c == '\\' && c[1]) {
      c++;
      *out++ = *c == 'n' ? '\n' : *c == 'r' ? '\r' : *c;
    } else {
      *out++ = *c;
    }
  }
  *out = '\0';
}

static void sv_sbVariable(cs_strbuf *sb, const char *prefix, variable *var)
{
  if (var->name == NULL)
    return;
  sv_sbPrintf(sb, "%s %i %s ", prefix, (int)var->type, var->name);
  sv_sbEscaped(sb, var->value ? var->value : "");
  cs_sb_append(sb, "\n", 1);
}

/// @brief Serialize the state of the game
/// @param position The position where the execution continues
/// @return The state (must be freed)
static char *sv_serializeState(long position)
{
  cs_strbuf sb = { NULL, 0, 0 };
  sv_sbPrintf(&sb, "scene %s\n", pg_game_status.sceneName);
  sv_sbPrintf(&sb, "sceneindex %i\n", pg_game_status.sceneIndex);
  sv_sbPrintf(&sb, "position %li\n", position);
  sv_sbPrintf(&sb, "reuse %i\n", (int)pg_game_status.choiceReuse);
  sv_sbPrintf(&sb, "looplimit %i\n", pg_game_status.loopLimit);
  sv_sbPrintf(&sb, "ended %i\n", pg_game_status.gameEnded);
  // The random generator is saved too: loading shows the same results of *rand
  cs_randomSeed();
  sv_sbPrintf(&sb, "random %llu\n", (unsigned long long)cs_randomState);

  for (int i = 0; i < runningGame->variablesCount; i++)
    sv_sbVariable(&sb, "var", &runningGame->variables[i]);
  for (int i = 0; i < pg_game_status.tempVariablesCount; i++)
    sv_sbVariable(&sb, "temp", &pg_game_status.tempVariables[i]);
  for (int i = 0; i < pg_game_status.paramsCount; i++) {
    cs_sb_append(&sb, "param ", 6);
    sv_sbEscaped(&sb, pg_game_status.params[i]);
    cs_sb_append(&sb, "\n", 1);
  }
  for (int i = 0; i < pg_game_status.used_choices_count; i++)
    sv_sbPrintf(&sb, "used %s\n", pg_game_status.used_choices[i]);
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (runningGame->achievements[i].achieved)
      sv_sbPrintf(&sb, "achieved %s\n", runningGame->achievements[i].name);
  }
  for (int i = 0; i < pg_game_status.choiceContextsCount; i++) {
    choice_context *ctx = &pg_game_status.choiceContexts[i];
    sv_sbPrintf(&sb, "context %li %i %i\n", ctx->endPosition, ctx->indentation, ctx->gosubCount);
  }
  for (int i = 0; i < pg_game_status.choiceEndsCount; i++) {
    choice_end *ce = &pg_game_status.choiceEnds[i];
    sv_sbPrintf(&sb, "choiceend %u %li %li\n", ce->sceneHash, ce->position, ce->endPosition);
  }
  for (int i = 1; i <= pg_game_status.gosubCount; i++) {
    gosub_info *frame = &pg_game_status.gosub[i];
    sv_sbPrintf(&sb, "frame %i %i %li %i %i %i %s\n", i, frame->fp != NULL, frame->position, frame->sceneIndex,
                (int)frame->choiceReuse, frame->loopLimit, frame->fp != NULL ? frame->sceneName : "-");
    for (int j = 0; j < frame->tempVariablesCount; j++) {
      char prefix[32];
      snprintf(prefix, sizeof(prefix), "frametemp %i", i);
      sv_sbVariable(&sb, prefix, &frame->tempVariables[j]);
    }
  }

  return sb.buf;
}

void sv_takeSnapshot()
{
  if (runningGame == NULL || pg_game_status.fp == NULL)
    return;

  // After *finish the scene is ended (fpPosition = fpSize) even if the file is not at its end
  long position = pg_game_status.fpPosition == pg_game_status.fpSize ? pg_game_status.fpSize : ftell(pg_game_status.fp);
  free(sv_pageSnapshot);
  sv_pageSnapshot = sv_serializeState(position);
  cs_copyString(sv_snapshotScene, sizeof(sv_snapshotScene), pg_game_status.sceneName);
}

void sv_freeSnapshot()
{
  free(sv_pageSnapshot);
  sv_pageSnapshot = NULL;
}

/// @brief Get the folder of the saves of a game (./saves/<game folder>)
static void sv_getGameSaveFolder(const char *gamePath, char *folder, int size)
{
  const char *name = gamePath;
  for (const char *c = gamePath; *c; c++) {
    if ((*c == '/' || *c == '\\') && c[1])
      name = c + 1;
  }
  snprintf(folder, size, "./saves/%s", name);
  // Remove a trailing separator
  int len = strlen(folder);
  if (len > 0 && (folder[len - 1] == '/' || folder[len - 1] == '\\'))
    folder[len - 1] = '\0';
}

/// @brief Get the folder of the saves of the running game (./saves/<game folder>)
static void sv_getSaveFolder(char *folder, int size)
{
  sv_getGameSaveFolder(runningGame->path, folder, size);
}

static void sv_getSavePath(int slot, char *path, int size)
{
  char folder[512];
  sv_getSaveFolder(folder, sizeof(folder));
  snprintf(path, size, "%s/slot%i.sav", folder, slot + 1);
}

static int sv_makeFolder(const char *path)
{
  if (fileExists(path))
    return 1;
#ifdef _WIN32
  return mkdir(path) == 0;
#else
  return mkdir(path, 0755) == 0;
#endif
}

#define ACHIEVEMENTS_HEADER "CoGSDL achievements 1"

static void sv_getAchievementsPath(char *path, int size)
{
  char folder[512];
  sv_getSaveFolder(folder, sizeof(folder));
  snprintf(path, size, "%s/achievements.txt", folder);
}

void sv_loadAchievements()
{
  if (runningGame == NULL)
    return;

  char path[600];
  sv_getAchievementsPath(path, sizeof(path));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return;

  char line[512];
  int valid = 0;
  int count = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, ACHIEVEMENTS_HEADER) == 0;
      if (!valid)
        break;
      continue;
    }
    for (int i = 0; i < runningGame->achievementsCount; i++) {
      if (stricmp(runningGame->achievements[i].name, line) == 0) {
        runningGame->achievements[i].achieved = 1;
        count++;
        break;
      }
    }
  }
  fclose(fp);
  if (!valid)
    logWarning("Invalid achievements file: %s", path);
  else
    logInfo("Achievements earned: %i (%s)", count, path);
}

static int sv_saveAchievements()
{
  if (runningGame == NULL)
    return 0;

  char folder[512];
  sv_getSaveFolder(folder, sizeof(folder));
  if (!sv_makeFolder("./saves") || !sv_makeFolder(folder)) {
    logError("Cannot create the folder %s", folder);
    return 0;
  }

  char path[600];
  char tmpPath[610];
  sv_getAchievementsPath(path, sizeof(path));
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);
  FILE *fp = fopen(tmpPath, "wb");
  if (fp == NULL) {
    logError("Cannot write %s", tmpPath);
    return 0;
  }
  int ok = fprintf(fp, "%s\n", ACHIEVEMENTS_HEADER) > 0;
  for (int i = 0; i < runningGame->achievementsCount; i++) {
    if (runningGame->achievements[i].achieved)
      ok = fprintf(fp, "%s\n", runningGame->achievements[i].name) > 0 && ok;
  }
  ok = (fclose(fp) == 0) && ok;
  if (!ok) {
    logError("Cannot write %s", tmpPath);
    remove(tmpPath);
    return 0;
  }
  remove(path);
  if (rename(tmpPath, path) != 0) {
    logError("Cannot rename %s to %s", tmpPath, path);
    return 0;
  }
  return 1;
}

/// @brief Build the name of a save from the position in the game: scene name and beginning of the page
static void sv_buildSaveName(char *name, int size)
{
  // Text of the page without commands (images, stat bars) and formatting
  cs_strbuf sb = { NULL, 0, 0 };
  const char *text = pg_game_status.text;
  if ((text == NULL || cs_isBlank(text)) && pg_game_status.choicesCount > 0)
    text = pg_game_status.choices[0].text;
  if (text != NULL) {
    int lineStart = 1;
    int lastSpace = 1;
    for (const char *c = text; *c; c++) {
      if (lineStart && *c == '*') {
        // Skip the command line
        while (*c && *c != '\n')
          c++;
        if (!*c)
          break;
        continue;
      }
      lineStart = *c == '\n';
      if (*c == '[') {
        const char *close = strchr(c, ']');
        if (close != NULL && close - c <= 3) {
          c = close;
          continue;
        }
      }
      if (isspace((unsigned char)*c)) {
        if (!lastSpace)
          cs_sb_append(&sb, " ", 1);
        lastSpace = 1;
      } else {
        cs_sb_append(&sb, c, 1);
        lastSpace = 0;
      }
    }
  }

  char excerpt[128] = "";
  if (sb.buf != NULL) {
    trim(sb.buf);
    const int maxLen = 45;
    int len = strlen(sb.buf);
    if (len <= maxLen) {
      snprintf(excerpt, sizeof(excerpt), "%s", sb.buf);
    } else {
      // Cut at a word boundary (or at a UTF-8 character boundary)
      int cut = maxLen;
      while (cut > maxLen / 2 && sb.buf[cut] != ' ')
        cut--;
      if (sb.buf[cut] != ' ') {
        cut = maxLen;
        while (cut > 0 && !isutf(sb.buf[cut]))
          cut--;
      }
      snprintf(excerpt, sizeof(excerpt), "%.*s...", cut, sb.buf);
    }
    free(sb.buf);
  }

  cs_copyString(name, size, sv_snapshotScene);
  if (strlen(excerpt)) {
    sv_appendString(name, size, ": ");
    sv_appendString(name, size, excerpt);
  }
}

/// @brief Read the information of a save file
static int sv_readSaveInfo(const char *path, save_info *info);

int sv_getSaveInfo(int slot, save_info *info)
{
  memset(info, 0, sizeof(save_info));
  if (runningGame == NULL || slot < 0 || slot >= SAVE_SLOTS)
    return 0;

  char path[600];
  sv_getSavePath(slot, path, sizeof(path));
  return sv_readSaveInfo(path, info);
}

/* The last save (used by "Continue") is written in ./saves/last_save.txt: the time of the
 * saves cannot be used on the devices without a real time clock (e.g. Miyoo Mini) */

#define LAST_SAVE_HEADER "CoGSDL last save 1"
#define LAST_SAVE_PATH "./saves/last_save.txt"

/// @brief Write the game folder and the slot of the last save
static void sv_writeLastSave(const char *gameName, int slot)
{
  char tmpPath[64];
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", LAST_SAVE_PATH);
  FILE *fp = fopen(tmpPath, "wb");
  if (fp == NULL) {
    logError("Cannot write %s", tmpPath);
    return;
  }
  int ok = fprintf(fp, "%s\ngame %s\nslot %i\n", LAST_SAVE_HEADER, gameName, slot + 1) > 0;
  ok = (fclose(fp) == 0) && ok;
  if (!ok) {
    logError("Cannot write %s", tmpPath);
    remove(tmpPath);
    return;
  }
  remove(LAST_SAVE_PATH);
  if (rename(tmpPath, LAST_SAVE_PATH) != 0)
    logError("Cannot rename %s to %s", tmpPath, LAST_SAVE_PATH);
}

/// @brief Read the last save written by sv_writeLastSave
/// @return 1 if the game is in the list of the games and the slot contains a valid save
static int sv_readLastSave(int *gameIndex, int *slot)
{
  FILE *fp = fopen(LAST_SAVE_PATH, "rb");
  if (fp == NULL)
    return 0;

  char line[600];
  char gameName[512] = "";
  int slotNumber = 0;
  int valid = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, LAST_SAVE_HEADER) == 0;
      if (!valid)
        break;
    } else if (startsWith("game ", line)) {
      cs_copyString(gameName, sizeof(gameName), line + 5);
    } else if (startsWith("slot ", line)) {
      slotNumber = atoi(line + 5);
    }
  }
  fclose(fp);
  if (!valid || strlen(gameName) == 0 || slotNumber < 1 || slotNumber > SAVE_SLOTS)
    return 0;

  // The game folder of the save is compared with the folders of the games
  for (int g = 0; g < _Globals.gamesCount; g++) {
    char folder[512];
    sv_getGameSaveFolder(_Globals.games[g].path, folder, sizeof(folder));
    if (strcmp(folder + strlen("./saves/"), gameName) != 0)
      continue;

    char path[600];
    save_info info;
    snprintf(path, sizeof(path), "%s/slot%i.sav", folder, slotNumber);
    if (!sv_readSaveInfo(path, &info))
      return 0;
    *gameIndex = g;
    *slot = slotNumber - 1;
    return 1;
  }
  return 0;
}

int sv_findLastSave(int *gameIndex, int *slot)
{
  if (sv_readLastSave(gameIndex, slot))
    return 1;

  // No last save (e.g. saves of a previous version): the most recent save by time
  int found = 0;
  time_t lastTime = 0;
  for (int g = 0; g < _Globals.gamesCount; g++) {
    char folder[512];
    sv_getGameSaveFolder(_Globals.games[g].path, folder, sizeof(folder));
    for (int s = 0; s < SAVE_SLOTS; s++) {
      char path[600];
      save_info info;
      snprintf(path, sizeof(path), "%s/slot%i.sav", folder, s + 1);
      if (sv_readSaveInfo(path, &info) && (!found || info.time > lastTime)) {
        found = 1;
        lastTime = info.time;
        *gameIndex = g;
        *slot = s;
      }
    }
  }
  return found;
}

int sv_continueLastSave()
{
  int gameIndex, slot;
  if (!sv_findLastSave(&gameIndex, &slot))
    return 0;

  logInfo("Continue: %s, slot %i", _Globals.games[gameIndex].path, slot + 1);
  runningGame = malloc(sizeof(game));
  loadGame(_Globals.games[gameIndex].path, runningGame, 0);
  if (strlen(runningGame->icon) > 0)
    runningGameIcon = IMG_Load(runningGame->icon);

  pg_init();
  if (!sv_loadGame(slot)) {
    logWarning("Continue: cannot load the save");
    pg_dispose();
    return 0;
  }
  return 1;
}

static int sv_readSaveInfo(const char *path, save_info *info)
{
  memset(info, 0, sizeof(save_info));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return 0;

  char line[1024];
  int valid = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, SAVE_HEADER) == 0;
      if (!valid)
        break;
    } else if (startsWith("name ", line)) {
      cs_copyString(info->name, sizeof(info->name), line + 5);
      sv_unescape(info->name);
    } else if (startsWith("playtime ", line)) {
      info->playTime = strtoull(line + 9, NULL, 10);
    } else if (startsWith("time ", line)) {
      info->time = (time_t)atoll(line + 5);
      break;
    }
  }
  fclose(fp);

  info->used = valid;
  return valid;
}

/* ---------------------------------------------------------------------------
 * Checkpoints: *save_checkpoint [slot], *restore_checkpoint [slot]
 * ------------------------------------------------------------------------- */

struct checkpoint
{
  char name[64];
  /// @brief The serialized state
  char *state;
} typedef checkpoint;

static checkpoint sv_checkpoints[16];
static int sv_checkpointsCount = 0;

static checkpoint *sv_getCheckpoint(const char *name)
{
  for (int i = 0; i < sv_checkpointsCount; i++) {
    if (stricmp(sv_checkpoints[i].name, name) == 0)
      return &sv_checkpoints[i];
  }
  return NULL;
}

/// @brief Set a checkpoint (the state is owned by the checkpoint)
static void sv_setCheckpoint(const char *name, char *state)
{
  checkpoint *cp = sv_getCheckpoint(name);
  if (cp == NULL) {
    if (sv_checkpointsCount >= (int)(sizeof(sv_checkpoints) / sizeof(checkpoint))) {
      logWarning("Too many checkpoints: %s not saved", name);
      free(state);
      return;
    }
    cp = &sv_checkpoints[sv_checkpointsCount++];
    cs_copyString(cp->name, sizeof(cp->name), name);
    cp->state = NULL;
  }
  free(cp->state);
  cp->state = state;
}

void sv_clearCheckpoints()
{
  for (int i = 0; i < sv_checkpointsCount; i++) {
    free(sv_checkpoints[i].state);
    sv_checkpoints[i].state = NULL;
  }
  sv_checkpointsCount = 0;
}

/// @brief Get the checkpoint slot of *save_checkpoint/*restore_checkpoint
/// @param name Set to the checkpoint name
/// @param variable Set to the name of the variable choice_saved_checkpoint[_slot]
/// @return 0 if the slot name is not valid
static int sv_getCheckpointSlot(const char *text, char *name, int nameSize, char *stateVar, int stateVarSize)
{
  char slot[256];
  cs_getCommandArgs(text, slot, sizeof(slot));
  for (char *c = slot; *c; c++) {
    if (!isalnum((unsigned char)*c) && *c != '_') {
      logWarning("Invalid checkpoint slot, must be only letters, numbers, and _ underscores: %s", slot);
      return 0;
    }
    *c = tolower((unsigned char)*c);
  }
  if (strlen(slot)) {
    cs_copyString(name, nameSize, slot);
    snprintf(stateVar, stateVarSize, "choice_saved_checkpoint_%.*s", stateVarSize - 25, slot);
  } else {
    cs_copyString(name, nameSize, "checkpoint");
    cs_copyString(stateVar, stateVarSize, "choice_saved_checkpoint");
  }
  return 1;
}

void sv_manageSaveCheckpointCommand(const char *text)
{
  char name[64];
  char stateVar[128];
  if (pg_isStatsActive()) {
    logWarning("Cannot *save_checkpoint in the stats screen");
    return;
  }
  if (!sv_getCheckpointSlot(text, name, sizeof(name), stateVar, sizeof(stateVar)))
    return;

  // Same as ChoiceScript: the variables are set before saving the state
  addGameVar(runningGame, boolean, stateVar, "true");
  cs_setTempVar("choice_just_restored_checkpoint", "false");
  sv_setCheckpoint(name, sv_serializeState(ftell(pg_game_status.fp)));
  logInfo("Checkpoint %s saved", name);
}

int sv_restoreCheckpoint(const char *text)
{
  char name[64];
  char stateVar[128];
  if (pg_isStatsActive()) {
    logWarning("Cannot *restore_checkpoint in the stats screen");
    return 0;
  }
  if (!sv_getCheckpointSlot(text, name, sizeof(name), stateVar, sizeof(stateVar)))
    return 0;

  checkpoint *cp = sv_getCheckpoint(name);
  if (cp == NULL || cp->state == NULL) {
    logWarning("*restore_checkpoint failed: checkpoint %s not saved (use *if %s)", name, stateVar);
    return 0;
  }

  char *state = cs_strndup(cp->state, strlen(cp->state));
  char **lines;
  int linesCount = sv_splitLines(state, &lines);
  if (!sv_validateState(lines, 0, linesCount)) {
    logError("*restore_checkpoint: invalid checkpoint %s", name);
    free(lines);
    free(state);
    return 0;
  }

  // checkpoint_exclusions: variables that keep their current value
  char *exclusionNames[64];
  char *exclusionValues[64];
  int exclusionIsTemp[64];
  int exclusionsCount = 0;
  variable *exclusions = NULL;
  for (int i = 0; i < pg_game_status.tempVariablesCount && exclusions == NULL; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, "checkpoint_exclusions") == 0)
      exclusions = &pg_game_status.tempVariables[i];
  }
  if (exclusions == NULL)
    exclusions = getGameVarPrimitive("checkpoint_exclusions", runningGame);
  if (exclusions != NULL && exclusions->value != NULL) {
    char *list = cs_strndup(exclusions->value, strlen(exclusions->value));
    for (char *word = strtok(list, " \t"); word != NULL && exclusionsCount < 64; word = strtok(NULL, " \t")) {
      variable *var = NULL;
      int isTemp = 0;
      for (int i = 0; i < pg_game_status.tempVariablesCount && var == NULL; i++) {
        if (stricmp(pg_game_status.tempVariables[i].name, word) == 0) {
          var = &pg_game_status.tempVariables[i];
          isTemp = 1;
        }
      }
      if (var == NULL)
        var = getGameVarPrimitive(word, runningGame);
      if (var == NULL || var->value == NULL)
        continue;
      exclusionNames[exclusionsCount] = cs_strndup(word, strlen(word));
      exclusionValues[exclusionsCount] = cs_strndup(var->value, strlen(var->value));
      exclusionIsTemp[exclusionsCount] = isTemp;
      exclusionsCount++;
    }
    free(list);
  }

  long position = sv_applyState(lines, 0, linesCount);
  free(lines);
  free(state);

  for (int i = 0; i < exclusionsCount; i++) {
    if (exclusionIsTemp[i]) {
      cs_setTempVar(exclusionNames[i], exclusionValues[i]);
    } else {
      variable *var = getGameVarPrimitive(exclusionNames[i], runningGame);
      if (var != NULL) {
        free(var->value);
        var->value = cs_strndup(exclusionValues[i], strlen(exclusionValues[i]));
      }
    }
    free(exclusionNames[i]);
    free(exclusionValues[i]);
  }

  cs_setTempVar("choice_just_restored_checkpoint", "true");
  fseek(pg_game_status.fp, position, SEEK_SET);
  pg_game_status.fpPosition = position;
  logInfo("Checkpoint %s restored", name);
  return 1;
}

/* ---------------------------------------------------------------------------
 * *save_game destination_scene
 *
 * Saves the variables for the next episode of the game, which can import them
 * with *restore_game. The saves of all the games are in ./saves/episodes
 * (the next episode is another game, with another folder).
 * ------------------------------------------------------------------------- */

#define EPISODE_HEADER "CoGSDL episode 1"

void sv_manageSaveGameCommand(const char *text)
{
  char destination[256];
  cs_getCommandArgs(text, destination, sizeof(destination));
  if (strlen(destination) == 0) {
    logWarning("*save_game requires a destination file name, e.g. *save_game Episode2");
    return;
  }

  // Same as ChoiceScript: a game restored from a previous episode is not saved again
  for (int i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, "choice_user_restored") == 0) {
      if (stricmp(pg_game_status.tempVariables[i].value, "true") == 0)
        return;
      break;
    }
  }

  if (!sv_makeFolder("./saves") || !sv_makeFolder("./saves/episodes")) {
    logError("Cannot create the folder ./saves/episodes");
    return;
  }

  char gameFolder[512];
  sv_getSaveFolder(gameFolder, sizeof(gameFolder));
  const char *gameName = gameFolder + strlen("./saves/");

  time_t now = time(NULL);
  char date[64];
  strftime(date, sizeof(date), "%Y-%m-%d %H:%M", localtime(&now));
  char name[512];
  cs_copyString(name, sizeof(name), runningGame->title ? runningGame->title : gameName);
  sv_appendString(name, sizeof(name), " - ");
  sv_appendString(name, sizeof(name), date);

  char path[700];
  char tmpPath[710];
  snprintf(path, sizeof(path), "./saves/episodes/%s - %lld.sav", gameName, (long long)now);
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

  cs_strbuf sb = { NULL, 0, 0 };
  sv_sbPrintf(&sb, "%s\n", EPISODE_HEADER);
  sv_sbPrintf(&sb, "game %s\n", gameName);
  cs_sb_append(&sb, "title ", strlen("title "));
  sv_sbEscaped(&sb, runningGame->title ? runningGame->title : gameName);
  cs_sb_append(&sb, "\nname ", strlen("\nname "));
  sv_sbEscaped(&sb, name);
  sv_sbPrintf(&sb, "\ntime %lld\n", (long long)now);
  sv_sbPrintf(&sb, "scene %s\n", destination);
  // Same as ChoiceScript: all the variables (stats), not the temporary ones
  for (int i = 0; i < runningGame->variablesCount; i++)
    sv_sbVariable(&sb, "var", &runningGame->variables[i]);
  sv_sbPrintf(&sb, "end\n");

  FILE *fp = fopen(tmpPath, "wb");
  int ok = fp != NULL && fwrite(sb.buf, 1, sb.len, fp) == sb.len;
  if (fp != NULL)
    ok = (fclose(fp) == 0) && ok;
  free(sb.buf);
  if (!ok || rename(tmpPath, path) != 0) {
    logError("Cannot write %s", path);
    remove(tmpPath);
    cs_appendBlockLine("[b]The game could not be saved for the next episode.[/b]");
    return;
  }

  logInfo("Game saved for the next episode: %s (%s)", path, destination);
  char message[600];
  snprintf(message, sizeof(message), "[b]Game saved for the next episode:[/b] %s", name);
  cs_appendBlockLine(message);
  cs_appendParagraphBreak();
}

/* ---------------------------------------------------------------------------
 * *restore_game [cancel=label]
 *   scene Message shown when the save points to this scene (optional block)
 *
 * Shows the saves of ./saves/episodes (written by *save_game) as the options of
 * a choice, plus "Cancel.". Restoring a save loads its variables and starts its
 * destination scene.
 * ------------------------------------------------------------------------- */

/// @brief Positions of the options of the restore menu (real options have positions >= 0)
#define RESTORE_CANCEL_POSITION (-2)
#define RESTORE_SAVE_POSITION(i) (-100 - (i))

struct episode_info
{
  char path[600];
  char name[512];
  char scene[256];
  long long time;
} typedef episode_info;

static int sv_compareEpisodes(const void *a, const void *b)
{
  long long ta = ((const episode_info *)a)->time;
  long long tb = ((const episode_info *)b)->time;
  return ta < tb ? 1 : ta > tb ? -1 : 0;
}

/// @brief Read the header of a save of ./saves/episodes
static int sv_readEpisodeInfo(const char *path, episode_info *info)
{
  memset(info, 0, sizeof(episode_info));
  cs_copyString(info->path, sizeof(info->path), path);
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return 0;
  char line[1024];
  int valid = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    if (!valid) {
      valid = strcmp(line, EPISODE_HEADER) == 0;
      if (!valid)
        break;
    } else if (startsWith("name ", line)) {
      cs_copyString(info->name, sizeof(info->name), line + 5);
      sv_unescape(info->name);
    } else if (startsWith("time ", line)) {
      info->time = atoll(line + 5);
    } else if (startsWith("scene ", line)) {
      cs_copyString(info->scene, sizeof(info->scene), line + 6);
      break;
    }
  }
  fclose(fp);
  return valid && strlen(info->scene) > 0;
}

static void sv_addVirtualChoice(const char *text, long position, int index)
{
  choice *tmp = realloc(pg_game_status.choices, sizeof(choice) * (pg_game_status.choicesCount + 1));
  if (tmp == NULL)
    return;
  pg_game_status.choices = tmp;
  choice *c = &pg_game_status.choices[pg_game_status.choicesCount++];
  memset(c, 0, sizeof(choice));
  snprintf(c->id, sizeof(c->id), "restore_game_%i", index);
  c->text = cs_strndup(text, strlen(text));
  c->position = position;
  c->reusable = 1;
  c->selectable = 1;
}

long sv_manageRestoreGameCommand(const char *text, int indentation)
{
  restore_menu *menu = &sv_restoreMenu;
  memset(menu, 0, sizeof(restore_menu));

  // cancel=label
  char args[512];
  cs_getCommandArgs(text, args, sizeof(args));
  if (startsWith("cancel=", args))
    cs_copyString(menu->cancelLabel, sizeof(menu->cancelLabel), args + strlen("cancel="));
  else if (strlen(args))
    logWarning("Invalid restore_game line: %s", text);

  // Unrestorable scenes: indented block "scene message"
  char line[4096];
  long lineStart;
  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;
    if (getIndentation(line) <= indentation)
      break;
    trim(line);
    int nameEnd = 0;
    while (line[nameEnd] && !isspace((unsigned char)line[nameEnd]))
      nameEnd++;
    if (menu->unrestorableCount < 16) {
      snprintf(menu->unrestorableScenes[menu->unrestorableCount], sizeof(menu->unrestorableScenes[0]), "%.*s", nameEnd, line);
      const char *message = line + nameEnd;
      while (isspace((unsigned char)*message))
        message++;
      cs_copyString(menu->unrestorableMessages[menu->unrestorableCount], sizeof(menu->unrestorableMessages[0]), message);
      menu->unrestorableCount++;
    }
  }
  fseek(pg_game_status.fp, lineStart, SEEK_SET);
  menu->resumePosition = lineStart;

  // The saves, most recent first
  episode_info *episodes = calloc(RESTORE_MAX_SAVES * 4, sizeof(episode_info));
  int episodesCount = 0;
  DIR *dir = opendir("./saves/episodes");
  if (dir != NULL && episodes != NULL) {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && episodesCount < RESTORE_MAX_SAVES * 4) {
      if (!endsWith(".sav", entry->d_name))
        continue;
      char path[600];
      snprintf(path, sizeof(path), "./saves/episodes/%.*s", (int)(sizeof(path) - 20), entry->d_name);
      if (sv_readEpisodeInfo(path, &episodes[episodesCount]))
        episodesCount++;
    }
  }
  if (dir != NULL)
    closedir(dir);
  if (episodes != NULL)
    qsort(episodes, episodesCount, sizeof(episode_info), sv_compareEpisodes);

  cs_clearChoices();
  for (int i = 0; i < episodesCount && i < RESTORE_MAX_SAVES; i++) {
    cs_copyString(menu->savePaths[i], sizeof(menu->savePaths[0]), episodes[i].path);
    sv_addVirtualChoice(episodes[i].name, RESTORE_SAVE_POSITION(i), i);
    menu->savesCount++;
  }
  free(episodes);
  if (menu->savesCount == 0)
    cs_appendBlockLine("No saved games from a previous episode.");
  sv_addVirtualChoice("Cancel.", RESTORE_CANCEL_POSITION, RESTORE_MAX_SAVES);

  menu->active = 1;
  pg_game_status.choiceEndPosition = menu->resumePosition;
  return menu->resumePosition;
}

/// @brief Continue after *restore_game (or at the cancel label)
static void sv_restoreGameCancel(int useCancelLabel)
{
  long position = sv_restoreMenu.resumePosition;
  if (useCancelLabel && strlen(sv_restoreMenu.cancelLabel)) {
    label *lbl = cs_get_label(sv_restoreMenu.cancelLabel);
    if (lbl != NULL)
      position = lbl->position;
    else
      logWarning("*restore_game: label %s not found", sv_restoreMenu.cancelLabel);
  }
  fseek(pg_game_status.fp, position, SEEK_SET);
  pg_game_status.fpPosition = position;
}

/// @brief Restore a save of ./saves/episodes: its variables, then its destination scene
static int sv_restoreEpisode(const char *path)
{
  episode_info info;
  if (!sv_readEpisodeInfo(path, &info)) {
    logError("Invalid save: %s", path);
    return 0;
  }

  // Scenes where the save cannot be restored
  for (int i = 0; i < sv_restoreMenu.unrestorableCount; i++) {
    if (stricmp(sv_restoreMenu.unrestorableScenes[i], info.scene) == 0) {
      free(sv_pendingNotice);
      sv_pendingNotice = cs_strndup(sv_restoreMenu.unrestorableMessages[i], strlen(sv_restoreMenu.unrestorableMessages[i]));
      return 0;
    }
  }
  if (!sv_sceneExists(info.scene)) {
    char message[600];
    cs_copyString(message, sizeof(message), "This saved game cannot be restored: the scene ");
    sv_appendString(message, sizeof(message), info.scene);
    sv_appendString(message, sizeof(message), " does not exist.");
    free(sv_pendingNotice);
    sv_pendingNotice = cs_strndup(message, strlen(message));
    return 0;
  }

  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return 0;
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *content = malloc(size + 1);
  if (content == NULL || fread(content, 1, size, fp) != (size_t)size) {
    fclose(fp);
    free(content);
    return 0;
  }
  fclose(fp);
  content[size] = '\0';

  // Variables of the save: the variables of this game with the same name are replaced
  char **lines;
  int linesCount = sv_splitLines(content, &lines);
  int count = 0;
  for (int i = 0; i < linesCount; i++) {
    if (!startsWith("var ", lines[i]))
      continue;
    int type;
    char *name, *value;
    if (!sv_parseSavedVariable(lines[i] + 4, &type, &name, &value))
      continue;
    variable *var = getGameVarPrimitive(name, runningGame);
    if (var == NULL) {
      addGameVar(runningGame, (enum var_types)type, name, value);
    } else {
      free(var->value);
      var->value = cs_strndup(value, strlen(value));
      var->type = (enum var_types)type;
    }
    count++;
  }
  free(lines);
  free(content);

  // Start the destination scene
  cs_clearGosubStack();
  cs_clearParams();
  cs_clearChoiceContexts(0);
  cs_clearTempVars();
  cs_openScene(info.scene, 1);
  cs_setTempVar("choice_user_restored", "true");
  cs_setTempVar("choice_restore_name", info.name);
  logInfo("Restored %s (%i variables), scene %s", path, count, info.scene);
  return 1;
}

/// @brief Manage the selection of an option of the restore menu
/// @return 1 if the option belongs to the restore menu
int sv_selectRestoreOption(choice *c)
{
  if (!sv_restoreMenu.active || c->position >= 0)
    return 0;

  sv_restoreMenu.active = 0;
  if (c->position == RESTORE_CANCEL_POSITION) {
    sv_restoreGameCancel(1);
    return 1;
  }

  int index = RESTORE_SAVE_POSITION(0) - c->position;
  if (index < 0 || index >= sv_restoreMenu.savesCount || !sv_restoreEpisode(sv_restoreMenu.savePaths[index]))
    sv_restoreGameCancel(0);
  return 1;
}

int sv_saveGame(int slot)
{
  if (runningGame == NULL || sv_pageSnapshot == NULL || slot < 0 || slot >= SAVE_SLOTS) {
    logWarning("Nothing to save");
    return 0;
  }

  char folder[512];
  sv_getSaveFolder(folder, sizeof(folder));
  if (!sv_makeFolder("./saves") || !sv_makeFolder(folder)) {
    logError("Cannot create the folder %s", folder);
    return 0;
  }

  char name[256];
  sv_buildSaveName(name, sizeof(name));

  char path[600];
  char tmpPath[610];
  sv_getSavePath(slot, path, sizeof(path));
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

  FILE *fp = fopen(tmpPath, "wb");
  if (fp == NULL) {
    logError("Cannot write %s", tmpPath);
    return 0;
  }

  cs_strbuf sb = { NULL, 0, 0 };
  const char *gameName = folder + strlen("./saves/");
  sv_sbPrintf(&sb, "%s\n", SAVE_HEADER);
  sv_sbPrintf(&sb, "game %s\n", gameName);
  cs_sb_append(&sb, "name ", 5);
  sv_sbEscaped(&sb, name);
  cs_sb_append(&sb, "\n", 1);
  // The play time is before the time: sv_readSaveInfo stops reading at the time
  unsigned long long playTimeMs = sv_getPlayTimeMs();
  sv_sbPrintf(&sb, "playtime %llu\n", playTimeMs / 1000ULL);
  sv_sbPrintf(&sb, "time %lld\n", (long long)time(NULL));
  cs_sb_append(&sb, sv_pageSnapshot, strlen(sv_pageSnapshot));
  for (int i = 0; i < sv_checkpointsCount; i++) {
    if (sv_checkpoints[i].state == NULL)
      continue;
    sv_sbPrintf(&sb, "checkpoint %s ", sv_checkpoints[i].name);
    sv_sbEscaped(&sb, sv_checkpoints[i].state);
    cs_sb_append(&sb, "\n", 1);
  }
  sv_sbPrintf(&sb, "end\n");

  int ok = fwrite(sb.buf, 1, sb.len, fp) == sb.len;
  ok = (fclose(fp) == 0) && ok;
  free(sb.buf);
  if (!ok) {
    logError("Cannot write %s", tmpPath);
    remove(tmpPath);
    return 0;
  }

  // Replace the old save only when the new one is complete
  remove(path);
  if (rename(tmpPath, path) != 0) {
    logError("Cannot rename %s to %s", tmpPath, path);
    return 0;
  }
  sv_writeLastSave(gameName, slot);

  // The session ends with the save: the next one starts now
  sv_playTimeBase = playTimeMs;
  sv_sessionStart = SDL_GetTicks();
  logInfo("Game saved in %s (%s), play time %llus", path, name, playTimeMs / 1000ULL);
  return 1;
}

static int sv_sceneExists(const char *sceneName)
{
  char path[600];
  snprintf(path, sizeof(path), "%s/scenes/%s.txt", runningGame->path, sceneName);
  return fileExists(path);
}

/// @brief Parse "<type> <name> <value>" of a saved variable
/// @return 1 on success (name and value point inside the line)
static int sv_parseSavedVariable(char *data, int *type, char **name, char **value)
{
  char *space = strchr(data, ' ');
  if (space == NULL)
    return 0;
  *space = '\0';
  *type = atoi(data);
  *name = space + 1;
  space = strchr(*name, ' ');
  if (space == NULL) {
    *value = *name + strlen(*name);
  } else {
    *space = '\0';
    *value = space + 1;
  }
  sv_unescape(*value);
  return strlen(*name) > 0;
}

/// @brief Split a text in lines (the text is modified)
/// @param lines Set to the lines (must be freed)
/// @return The number of lines
static int sv_splitLines(char *content, char ***lines)
{
  int count = 1;
  for (char *c = content; *c; c++) {
    if (*c == '\n')
      count++;
  }
  *lines = malloc(sizeof(char *) * count);
  count = 0;
  char *start = content;
  for (char *c = content; ; c++) {
    if (*c == '\n' || *c == '\0') {
      int last = *c == '\0';
      *c = '\0';
      if (c > start && c[-1] == '\r')
        c[-1] = '\0';
      if (*start)
        (*lines)[count++] = start;
      if (last)
        break;
      start = c + 1;
    }
  }
  return count;
}

/// @brief Check that a saved state can be restored (the scenes exist)
static int sv_validateState(char **lines, int start, int end)
{
  int valid = 1;
  const char *sceneName = NULL;
  for (int i = start; valid && i < end; i++) {
    if (startsWith("scene ", lines[i])) {
      sceneName = lines[i] + 6;
      valid = sv_sceneExists(sceneName);
    } else if (startsWith("frame ", lines[i])) {
      int index, isScene, sceneIndex, reuse, loopLimit;
      long position;
      int nameStart = 0;
      if (sscanf(lines[i] + 6, "%i %i %li %i %i %i %n", &index, &isScene, &position, &sceneIndex, &reuse, &loopLimit, &nameStart) < 6 ||
          index < 1 || index >= (int)(sizeof(pg_game_status.gosub) / sizeof(gosub_info)) ||
          (isScene && !sv_sceneExists(lines[i] + 6 + nameStart)))
        valid = 0;
    }
  }
  return valid && sceneName != NULL;
}

/// @brief Restore a saved state (the lines are modified)
/// @return The position where the execution continues
static long sv_applyState(char **lines, int start, int end)
{
  // Reset the state of the game
  pg_statsClose();
  cs_clearGosubStack();
  cs_clearParams();
  cs_clearTempVars();
  cs_clearChoices();
  pg_game_status.choiceContextsCount = 0;
  pg_game_status.choiceEndsCount = 0;
  pg_game_status.used_choices_count = 0;
  pg_game_status.labelVisitsCount = 0;
  pg_game_status.gameEnded = 0;
  pg_game_status.showPageBreak = 0;
  pg_game_status.scrollY = 0;
  pg_game_status.selectedChoiceIndex = -1;
  free(pg_game_status.text);
  pg_game_status.text = NULL;
  pg_freeTextSurface();
  // The achievements are not restored: they stay earned (they are saved in achievements.txt)

  // The variables created after the save (e.g. by *check_purchase) are removed
  char *loadedVars = calloc(sizeof(runningGame->variables) / sizeof(variable), 1);

  long position = 0;
  for (int i = start; i < end; i++) {
    char *line = lines[i];
    char *data = strchr(line, ' ');
    if (data == NULL)
      continue;
    *data++ = '\0';

    int type;
    char *name, *value;
    if (strcmp(line, "scene") == 0) {
      cs_openScene(data, 1);
    } else if (strcmp(line, "sceneindex") == 0) {
      pg_game_status.sceneIndex = atoi(data);
    } else if (strcmp(line, "position") == 0) {
      position = atol(data);
    } else if (strcmp(line, "reuse") == 0) {
      pg_game_status.choiceReuse = (enum choice_reuse)atoi(data);
    } else if (strcmp(line, "looplimit") == 0) {
      pg_game_status.loopLimit = atoi(data);
    } else if (strcmp(line, "ended") == 0) {
      pg_game_status.gameEnded = atoi(data);
    } else if (strcmp(line, "random") == 0) {
      cs_randomState = (uint64_t)strtoull(data, NULL, 10);
    } else if (strcmp(line, "var") == 0) {
      if (sv_parseSavedVariable(data, &type, &name, &value)) {
        variable *var = NULL;
        for (int j = 0; j < runningGame->variablesCount; j++) {
          if (stricmp(runningGame->variables[j].name, name) == 0) {
            var = &runningGame->variables[j];
            break;
          }
        }
        if (var == NULL) {
          addGameVar(runningGame, (enum var_types)type, name, value);
          var = getGameVar(name, runningGame);
        } else {
          free(var->value);
          var->value = cs_strndup(value, strlen(value));
          var->type = (enum var_types)type;
        }
        if (var != NULL && loadedVars != NULL)
          loadedVars[var - runningGame->variables] = 1;
      }
    } else if (strcmp(line, "temp") == 0) {
      if (sv_parseSavedVariable(data, &type, &name, &value)) {
        cs_setTempVar(name, value);
        pg_game_status.tempVariables[pg_game_status.tempVariablesCount - 1].type = (enum var_types)type;
      }
    } else if (strcmp(line, "param") == 0) {
      if (pg_game_status.paramsCount < (int)(sizeof(pg_game_status.params) / sizeof(char *))) {
        sv_unescape(data);
        pg_game_status.params[pg_game_status.paramsCount++] = cs_strndup(data, strlen(data));
      }
    } else if (strcmp(line, "used") == 0) {
      if (pg_game_status.used_choices_count < (int)(sizeof(pg_game_status.used_choices) / sizeof(pg_game_status.used_choices[0])))
        snprintf(pg_game_status.used_choices[pg_game_status.used_choices_count++], sizeof(pg_game_status.used_choices[0]), "%s", data);
    } else if (strcmp(line, "achieved") == 0) {
      for (int j = 0; j < runningGame->achievementsCount; j++) {
        if (stricmp(runningGame->achievements[j].name, data) == 0)
          runningGame->achievements[j].achieved = 1;
      }
    } else if (strcmp(line, "choiceend") == 0) {
      if (pg_game_status.choiceEndsCount < (int)(sizeof(pg_game_status.choiceEnds) / sizeof(choice_end))) {
        choice_end *ce = &pg_game_status.choiceEnds[pg_game_status.choiceEndsCount++];
        sscanf(data, "%u %li %li", &ce->sceneHash, &ce->position, &ce->endPosition);
      }
    } else if (strcmp(line, "context") == 0) {
      if (pg_game_status.choiceContextsCount < (int)(sizeof(pg_game_status.choiceContexts) / sizeof(choice_context))) {
        choice_context *ctx = &pg_game_status.choiceContexts[pg_game_status.choiceContextsCount++];
        sscanf(data, "%li %i %i", &ctx->endPosition, &ctx->indentation, &ctx->gosubCount);
      }
    } else if (strcmp(line, "frame") == 0) {
      int index, isScene, sceneIndex, reuse, loopLimit;
      long framePosition;
      int nameStart = 0;
      sscanf(data, "%i %i %li %i %i %i %n", &index, &isScene, &framePosition, &sceneIndex, &reuse, &loopLimit, &nameStart);
      gosub_info *frame = &pg_game_status.gosub[index];
      memset(frame, 0, sizeof(gosub_info));
      frame->position = framePosition;
      frame->sceneIndex = sceneIndex;
      frame->choiceReuse = (enum choice_reuse)reuse;
      frame->loopLimit = loopLimit;
      if (isScene) {
        cs_copyString(frame->sceneName, sizeof(frame->sceneName), data + nameStart);
        char scenePath[600];
        snprintf(scenePath, sizeof(scenePath), "%s/scenes/%s.txt", runningGame->path, frame->sceneName);
        frame->fp = fopen(scenePath, "rb");
        if (frame->fp != NULL) {
          fseek(frame->fp, 0, SEEK_END);
          frame->fpSize = ftell(frame->fp);
          fseek(frame->fp, frame->position, SEEK_SET);
        }
      }
      if (index > pg_game_status.gosubCount)
        pg_game_status.gosubCount = index;
    } else if (strcmp(line, "checkpoint") == 0) {
      // checkpoint <name> <escaped state>
      char *space = strchr(data, ' ');
      if (space != NULL) {
        *space = '\0';
        sv_unescape(space + 1);
        sv_setCheckpoint(data, cs_strndup(space + 1, strlen(space + 1)));
      }
    } else if (strcmp(line, "frametemp") == 0) {
      char *space = strchr(data, ' ');
      if (space == NULL)
        continue;
      *space = '\0';
      int index = atoi(data);
      if (index < 1 || index > pg_game_status.gosubCount)
        continue;
      if (sv_parseSavedVariable(space + 1, &type, &name, &value)) {
        gosub_info *frame = &pg_game_status.gosub[index];
        variable *tmp = realloc(frame->tempVariables, sizeof(variable) * (frame->tempVariablesCount + 1));
        if (tmp == NULL)
          continue;
        frame->tempVariables = tmp;
        variable *var = &frame->tempVariables[frame->tempVariablesCount++];
        var->name = cs_strndup(name, strlen(name));
        var->value = cs_strndup(value, strlen(value));
        var->type = (enum var_types)type;
      }
    }
  }
  if (loadedVars != NULL) {
    int count = 0;
    for (int i = 0; i < runningGame->variablesCount; i++) {
      if (loadedVars[i]) {
        runningGame->variables[count++] = runningGame->variables[i];
      } else {
        logDebug("Variable %s removed (not in the save)", runningGame->variables[i].name);
        free(runningGame->variables[i].name);
        free(runningGame->variables[i].value);
      }
    }
    runningGame->variablesCount = count;
    free(loadedVars);
  }

  return position;
}

int sv_loadGame(int slot)
{
  if (runningGame == NULL || slot < 0 || slot >= SAVE_SLOTS)
    return 0;

  char path[600];
  sv_getSavePath(slot, path, sizeof(path));
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    logWarning("Save not found: %s", path);
    return 0;
  }
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char *content = malloc(size + 1);
  if (content == NULL || fread(content, 1, size, fp) != (size_t)size) {
    fclose(fp);
    free(content);
    return 0;
  }
  fclose(fp);
  content[size] = '\0';

  char **lines;
  int linesCount = sv_splitLines(content, &lines);

  // Validate before changing the state of the game
  int valid = linesCount > 1 && strcmp(lines[0], SAVE_HEADER) == 0 && strcmp(lines[linesCount - 1], "end") == 0 &&
              sv_validateState(lines, 1, linesCount - 1);
  if (!valid) {
    logError("Invalid save: %s", path);
    free(lines);
    free(content);
    return 0;
  }

  // A new session starts with the play time of the save (0 for the saves without it)
  unsigned long long playTime = 0;
  for (int i = 1; i < linesCount - 1; i++) {
    if (startsWith("playtime ", lines[i])) {
      playTime = strtoull(lines[i] + 9, NULL, 10);
      break;
    }
  }
  sv_startPlayTime(playTime);

  // The checkpoints are saved with the game
  sv_clearCheckpoints();
  long position = sv_applyState(lines, 1, linesCount - 1);
  free(lines);
  free(content);

  // Execute the saved page again
  fseek(pg_game_status.fp, position, SEEK_SET);
  pg_game_status.fpPosition = position;
  logInfo("Game loaded from %s", path);
  cs_exec();
  return 1;
}
