/* ---------------------------------------------------------------------------
 * ChoiceScript interpreter
 *
 * Execution of the scenes: reading of the lines, commands (*if, *goto, *gosub,
 * *choice, *set, *temp, *rand...), labels, variables, expressions and text
 * substitutions. The page text is built in pg_game_status.text and drawn by
 * gui_playgame.c. Saves, checkpoints, episodes and achievements are in saves.c.
 * ------------------------------------------------------------------------- */

#include "CoGSDL.h"
#include "gui_playgame.h"
#include "choicescript.h"

variable* getGameVarPrimitive(const char *varName, game* game);
static void cs_popGosub();
/// @brief Random number in [0, 1)
static double cs_random();
static void cs_formatNumber(double number, char *value);
static void cs_assignVar(variable *var, const char *value);
/// @brief Manage the input_text command
/// @return 1 if the game must wait for the text
static int cs_manageInputTextCommand(const char *line);
/// @brief Manage the input_number command
/// @return 1 if the game must wait for the number
static int cs_manageInputNumberCommand(const char *line);
/// @brief Count a jump to a label (*looplimit)
/// @return 0 if the label was visited too many times in this page
static int cs_checkLoopLimit(long position);

void cs_selectChoice(int index)
{
  choice *c = &pg_game_status.choices[index];

  // Options of the *restore_game menu
  if (sv_selectRestoreOption(c))
    return;

  // When the option body ends, continue after the choice block
  if (pg_game_status.choiceContextsCount == sizeof(pg_game_status.choiceContexts) / sizeof(choice_context)) {
    logWarning("Too many nested choices");
    memmove(&pg_game_status.choiceContexts[0], &pg_game_status.choiceContexts[1], sizeof(choice_context) * (pg_game_status.choiceContextsCount - 1));
    pg_game_status.choiceContextsCount--;
  }
  choice_context *ctx = &pg_game_status.choiceContexts[pg_game_status.choiceContextsCount++];
  ctx->endPosition = pg_game_status.choiceEndPosition;
  ctx->indentation = c->indentation;
  ctx->gosubCount = pg_game_status.gosubCount;

  fseek(pg_game_status.fp, c->position, SEEK_SET);
  pg_game_status.fpPosition = c->position;
}

/* ---------------------------------------------------------------------------
 * Text substitution ("Text tricks")
 *
 *   ${expr}            -> value of expr
 *   $!{expr}           -> value of expr, first letter capitalized
 *   $!!{expr}          -> value of expr, all letters capitalized
 *   @{cond a|b}        -> a if cond is true, b if false
 *   @{num a|b|c...}    -> the num-th option (1-based)
 *   @!{...} / @!!{...} -> multireplace with capitalization
 *
 * Options of a multireplace can contain ${} substitutions.
 * ------------------------------------------------------------------------- */

void cs_sb_append(cs_strbuf *sb, const char *s, size_t n)
{
  if (!s || n == 0)
    return;

  if (sb->len + n + 1 > sb->cap) {
    size_t newCap = sb->cap ? sb->cap : 64;
    while (sb->len + n + 1 > newCap)
      newCap *= 2;
    char *tmp = realloc(sb->buf, newCap);
    if (tmp == NULL) {
      logWarning("Realloc failed");
      return;
    }
    sb->buf = tmp;
    sb->cap = newCap;
  }
  memcpy(sb->buf + sb->len, s, n);
  sb->len += n;
  sb->buf[sb->len] = '\0';
}

char *cs_strndup(const char *s, size_t n)
{
  char *res = malloc(n + 1);
  if (res == NULL)
    return NULL;
  memcpy(res, s, n);
  res[n] = '\0';
  return res;
}

/// @brief Case insensitive compare of the first n chars
static int cs_strnicmp(const char *s1, const char *s2, size_t n)
{
  for (size_t i = 0; i < n; i++) {
    int c1 = tolower((unsigned char)s1[i]);
    int c2 = tolower((unsigned char)s2[i]);
    if (c1 != c2)
      return c1 < c2 ? -1 : 1;
    if (c1 == '\0')
      return 0;
  }
  return 0;
}

/// @brief Find the char closing the one at position open ('{' -> '}', '(' -> ')'), handling nesting
/// @return The position of the closing char or -1
static int cs_findClosing(const char *text, int open)
{
  const char openChar = text[open];
  const char closeChar = openChar == '{' ? '}' : ')';
  int depth = 0;
  int inQuote = 0;
  for (int i = open; text[i]; i++) {
    if (openChar == '(' && text[i] == '"' && (i == 0 || text[i - 1] != '\\')) {
      inQuote = !inQuote;
      continue;
    }
    if (inQuote)
      continue;
    if (text[i] == openChar) {
      depth++;
    } else if (text[i] == closeChar) {
      depth--;
      if (depth == 0)
        return i;
    }
  }
  return -1;
}

/// @brief Evaluate a boolean condition supporting parentheses, and, or, not
/// @param condition The condition
/// @return 1 if true, 0 if false
int cs_evaluateBoolean(const char *condition)
{
  if (!condition)
    return 0;

  char *cond = cs_strndup(condition, strlen(condition));
  trim(cond);

  // Remove parentheses wrapping the whole condition
  int len = strlen(cond);
  while (len > 1 && cond[0] == '(' && cs_findClosing(cond, 0) == len - 1) {
    memmove(cond, cond + 1, len - 2);
    cond[len - 2] = '\0';
    trim(cond);
    len = strlen(cond);
  }

  // not(...)
  if (len > 3 && cs_strnicmp(cond, "not", 3) == 0 && (cond[3] == '(' || isspace((unsigned char)cond[3]))) {
    int res = !cs_evaluateBoolean(cond + 3);
    free(cond);
    return res;
  }

  // Search a top level "and"/"or"
  int depth = 0;
  int inQuote = 0;
  for (int i = 0; i < len; i++) {
    if (cond[i] == '"' && (i == 0 || cond[i - 1] != '\\'))
      inQuote = !inQuote;
    if (inQuote)
      continue;
    if (cond[i] == '(') {
      depth++;
      continue;
    }
    if (cond[i] == ')')
      depth--;
    // "and"/"or" after a space or a closing parenthesis: (a = 1)or b
    if (depth == 0 && (isspace((unsigned char)cond[i]) || cond[i] == ')')) {
      int opLen = 0;
      int isAnd = 0;
      if (cs_strnicmp(&cond[i + 1], "and", 3) == 0 && (isspace((unsigned char)cond[i + 4]) || cond[i + 4] == '(')) {
        opLen = 3;
        isAnd = 1;
      } else if (cs_strnicmp(&cond[i + 1], "or", 2) == 0 && (isspace((unsigned char)cond[i + 3]) || cond[i + 3] == '(')) {
        opLen = 2;
      }

      if (opLen) {
        // Keep the closing parenthesis in the left condition
        char *left = cs_strndup(cond, cond[i] == ')' ? i + 1 : i);
        char *right = cs_strndup(&cond[i + 1 + opLen], len - i - 1 - opLen);
        int l = cs_evaluateBoolean(left);
        int r = cs_evaluateBoolean(right);
        free(left);
        free(right);
        free(cond);
        return isAnd ? (l && r) : (l || r);
      }
    }
  }

  int res = cs_evaluateSimpleCondition(cond);
  free(cond);
  return res;
}

static void cs_capitalize(char *text, int mode)
{
  if (!text || mode == 0)
    return;

  if (mode == 1) {
    // First letter only
    for (int i = 0; text[i]; i++) {
      if (!isspace((unsigned char)text[i])) {
        text[i] = toupper((unsigned char)text[i]);
        break;
      }
    }
  } else {
    for (int i = 0; text[i]; i++)
      text[i] = toupper((unsigned char)text[i]);
  }
}

/// @brief Get the 0-based index of the option to use in a multireplace
/// @param condition The multireplace condition (variable name or expression)
/// @return The index or -1
static int cs_multireplaceIndex(const char *condition)
{
  char opr[3];
  int isSimpleValue = indexOf(condition, '(', 0) < 0 &&
                      indexOf(condition, ' ', 0) < 0 &&
                      cs_get_operator(condition, opr) < 0;

  if (isSimpleValue) {
    char value[4096];
    cs_evaluateExpression(condition, value);
    trim(value);
    if (stricmp(value, "true") == 0)
      return 0;
    if (stricmp(value, "false") == 0)
      return 1;
    int intValue;
    if (strlen(value) > 0 && is_number(value, &intValue))
      return intValue - 1;

    logWarning("Invalid multireplace value: %s=%s", condition, value);
    return -1;
  }

  return cs_evaluateBoolean(condition) ? 0 : 1;
}

/// @brief Evaluate the content of a multireplace @{cond opt1|opt2|...}
/// @param content The text between the braces
/// @return The selected option (expanded). Must be freed.
static char *cs_multireplace(const char *content)
{
  int len = strlen(content);
  int start = 0;
  while (start < len && isspace((unsigned char)content[start]))
    start++;

  // Condition
  int condEnd;
  if (content[start] == '(') {
    condEnd = cs_findClosing(content, start);
    if (condEnd < 0) {
      logWarning("Multireplace: missing ')' in %s", content);
      return cs_strndup("", 0);
    }
    condEnd++;
  } else {
    condEnd = start;
    while (condEnd < len && !isspace((unsigned char)content[condEnd]))
      condEnd++;
  }
  char *condition = cs_strndup(content + start, condEnd - start);

  // Skip the separator between condition and options
  int optStart = condEnd;
  if (optStart < len && isspace((unsigned char)content[optStart]))
    optStart++;

  int index = cs_multireplaceIndex(condition);
  logDebug("\tMultireplace %s -> option %i", condition, index + 1);
  free(condition);

  if (index < 0) {
    return cs_strndup("", 0);
  }

  // Find the option (split on '|' outside nested braces)
  int depth = 0;
  int current = 0;
  int partStart = optStart;
  for (int i = optStart; i <= len; i++) {
    if (i < len && content[i] == '{') {
      depth++;
    } else if (i < len && content[i] == '}') {
      depth--;
    } else if (i == len || (content[i] == '|' && depth == 0)) {
      if (current == index) {
        char *option = cs_strndup(content + partStart, i - partStart);
        char *res = cs_expandVariables(option);
        free(option);
        return res;
      }
      current++;
      partStart = i + 1;
    }
  }

  logWarning("Multireplace: option %i not found in %s", index + 1, content);
  return cs_strndup("", 0);
}

char *cs_expandVariables(const char *text)
{
  if (!text)
    return NULL;

  cs_strbuf sb = { NULL, 0, 0 };

  int textLen = strlen(text);
  int i = 0;
  while (i < textLen) {
    if (text[i] == '$' || text[i] == '@') {
      // Capitalization: $!{} $!!{} @!{} @!!{}
      int j = i + 1;
      int caps = 0;
      while (j < textLen && text[j] == '!' && caps < 2) {
        caps++;
        j++;
      }

      if (j < textLen && text[j] == '{') {
        int close = cs_findClosing(text, j);
        if (close > j) {
          char *content = cs_strndup(text + j + 1, close - j - 1);
          char *replacement = NULL;
          if (text[i] == '$') {
            trim(content);
            char value[4096];
            cs_evaluateExpression(content, value);
            replacement = cs_strndup(value, strlen(value));
          } else {
            replacement = cs_multireplace(content);
          }
          cs_capitalize(replacement, caps);
          logDebug("\tReplaced %.*s -> %s", close - i + 1, text + i, replacement);

          if (replacement)
            cs_sb_append(&sb, replacement, strlen(replacement));
          free(replacement);
          free(content);
          i = close + 1;
          continue;
        } else {
          logWarning("Missing '}' in %s", text + i);
        }
      }
    }

    cs_sb_append(&sb, &text[i], 1);
    i++;
  }

  if (sb.buf == NULL)
    return cs_strndup("", 0);
  return sb.buf;
}

/// @brief Exec the next line(s)
/// @return 1 if something was executed
int cs_exec_primitive()
{
  char line[4096];

  if (pg_game_status.gameEnded) {
    // Nothing more to execute: keep showing the last page
    pg_game_status.showPageBreak = 0;
    cs_clearChoices();
    if (pg_game_status.text == NULL && !pg_isStatsActive())
      cs_appendText("The End", 0);
    return 1;
  }

  if(pg_game_status.text != NULL) {
    free(pg_game_status.text);
    pg_game_status.text = NULL;
  }
  pg_freeTextSurface();

  pg_game_status.showPageBreak = 0;
  pg_game_status.inputTextActive = 0;
  pg_game_status.scrollY = 0;
  cs_clearChoices();
  // *restore_game menu closed, message to show at the start of the page
  sv_restoreGameNewPage();
  // Same as ChoiceScript: *looplimit counts the visits to a label in a page
  pg_game_status.labelVisitsCount = 0;

  if (pg_game_status.fpPosition == pg_game_status.fpSize) {
    logDebug("Reached EOF (file size=%i), going to the next scene", pg_game_status.fpSize);
    cs_goToNextScene();

    return 0;
  }

  int textStarted = 0;
  while (1) {
    // Position of the line (commands can move the file position)
    long currentLineStart = ftell(pg_game_status.fp);
    if (!fgets(line, sizeof(line), pg_game_status.fp))
      break;
    logDebug("line=%s", line);
    int lineIndentation = getIndentation(line);

    // End of the body of the selected option: continue after the choice block
    if (pg_game_status.choiceContextsCount > 0 && !cs_isBlank(line) && !isComment(line)) {
      choice_context *ctx = &pg_game_status.choiceContexts[pg_game_status.choiceContextsCount - 1];
      if (ctx->gosubCount == pg_game_status.gosubCount && lineIndentation <= ctx->indentation) {
        logDebug("End of option body -> %li", ctx->endPosition);
        pg_game_status.choiceContextsCount--;
        fseek(pg_game_status.fp, ctx->endPosition, SEEK_SET);
        pg_game_status.fpPosition = ctx->endPosition;
        continue;
      }
    }

    // Same as ChoiceScript: reaching an option line of a choice (e.g. after a *goto into an
    // option body) is the end of the option body: continue after the choice block
    if (!cs_isBlank(line) && !isComment(line)) {
      long endPosition = cs_findChoiceEnd(currentLineStart);
      if (endPosition >= 0) {
        logDebug("End of option body (option line) -> %li", endPosition);
        cs_clearChoiceContexts(pg_game_status.gosubCount);
        fseek(pg_game_status.fp, endPosition, SEEK_SET);
        pg_game_status.fpPosition = endPosition;
        continue;
      }
    }

    if (isComment(line))
      continue;

    trim(line);
    int lineLen = strlen(line);
    if (line[0] == '*') {
      // Command
      char command[256];
      cs_getCommand(line, command);
      logDebug("Command [%s]=%s", command, line);
      if (stricmp(command, "label") == 0) {
        // We can skip labels
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }
      if (stricmp(command, "params") == 0) {
        // params name1 name2...
        cs_manageParamsCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "hide_reuse") == 0 || stricmp(command, "disable_reuse") == 0 || stricmp(command, "allow_reuse") == 0) {
        // Default reuse of the options of the choices in this scene
        pg_game_status.choiceReuse = stricmp(command, "hide_reuse") == 0 ? reuse_hide :
                                     stricmp(command, "disable_reuse") == 0 ? reuse_disable : reuse_allow;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "looplimit") == 0) {
        // looplimit N: maximum number of visits to a label in a page
        char args[256];
        char value[1024];
        cs_getCommandArgs(line, args, sizeof(args));
        cs_evaluateExpression(args, value);
        int limit;
        if (strlen(value) > 0 && is_number(value, &limit) && limit >= 0)
          pg_game_status.loopLimit = limit;
        else
          logWarning("Invalid looplimit: %s", args);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "config") == 0) {
        // config variable value
        cs_manageConfigCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "restore_game") == 0) {
        // restore_game [cancel=label]: a choice between the saves of the previous episodes
        long newPosition = sv_manageRestoreGameCommand(line, lineIndentation);
        pg_game_status.fpPosition = newPosition;
        break;
      }

      if (stricmp(command, "save_game") == 0) {
        // save_game destination_scene: save the game for the next episode
        sv_manageSaveGameCommand(line);
        textStarted = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "save_checkpoint") == 0) {
        // save_checkpoint [slot]
        sv_manageSaveCheckpointCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "restore_checkpoint") == 0) {
        // restore_checkpoint [slot]: a new page starts at the checkpoint
        if (sv_restoreCheckpoint(line)) {
          textStarted = 0;
          sv_takeSnapshot();
        } else {
          pg_game_status.fpPosition = ftell(pg_game_status.fp);
        }
        continue;
      }

      if (stricmp(command, "rand") == 0) {
        // rand variable min max
        cs_manageRandCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "check_achievements") == 0) {
        // Same as ChoiceScript: a temporary variable choice_achieved_<name> for each achievement
        for (int i = 0; i < runningGame->achievementsCount; i++) {
          char name[300];
          snprintf(name, sizeof(name), "choice_achieved_%s", runningGame->achievements[i].name);
          cs_setTempVar(name, runningGame->achievements[i].achieved ? "true" : "false");
        }
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "achieve") == 0) {
        // achieve name
        sv_manageAchieveCommand(line);
        textStarted = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "feedback") == 0 || stricmp(command, "subscribe") == 0) {
        // Online features (rating request, newsletter subscription): nothing to do
        logDebug("*%s ignored", command);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "ending") == 0) {
        // End of the game
        cs_clearChoiceContexts(0);
        pg_game_status.gameEnded = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        break;
      }

      if (stricmp(command, "goto_scene") == 0) {
        // goto_scene scene [label]
        long newPosition = cs_manageGotoSceneCommand(line);
        logDebug("%s -> %li", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        if (!cs_checkLoopLimit(newPosition))
          break;

        continue;
      }

      if (stricmp(command, "bug") == 0) {
        logWarning("Bug found (%i)", pg_game_status.fpPosition);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "check_purchase") == 0) {
        // check_purchase name1 name2
        cs_manageCheckPurchaseCommand(line);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "input_text") == 0) {
        // input_text variable [long] [allow_blank]: wait for the text typed by the player
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        if (cs_manageInputTextCommand(line))
          break;
        continue;
      }

      if (stricmp(command, "input_number") == 0) {
        // input_number variable min max: wait for the number typed by the player
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        if (cs_manageInputNumberCommand(line))
          break;
        continue;
      }

      if (stricmp(command, "page_break") == 0) {
        // Same as ChoiceScript: a page break on an empty screen is ignored
        if (cs_isBlank(pg_game_status.text ? pg_game_status.text : "")) {
          logDebug("page_break ignored: empty screen");
          pg_game_status.fpPosition = ftell(pg_game_status.fp);
          continue;
        }
        // page_break [button text]
        char message[sizeof(pg_game_status.pageBreakMessage)];
        cs_getCommandArgs(line, message, sizeof(message));
        pg_game_status.showPageBreak = 1;
        snprintf(pg_game_status.pageBreakMessage, sizeof(pg_game_status.pageBreakMessage), "%s", strlen(message) ? message : "Next");
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        break;
      }

      if (stricmp(command, "stat_chart") == 0) {
        cs_manageStatChartCommand(lineIndentation);
        textStarted = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "link") == 0 || stricmp(command, "link_button") == 0) {
        // link URL [text], link_button URL [text]: the link cannot be opened here,
        // so the URL is shown (the text is ignored)
        char args[4096];
        cs_getCommandArgs(line, args, sizeof(args));
        trim(args);
        char *end = args;
        while (*end && *end != ' ' && *end != '\t')
          end++;
        *end = '\0';
        if (strlen(args) == 0) {
          logWarning("*link without URL: %s", line);
        } else {
          char *expanded = cs_expandVariables(args);
          const char *url = expanded ? expanded : args;
          // Same as ChoiceScript: printed as a line of text of the current paragraph
          cs_appendLineSeparator();
          cs_appendText(url, 0);
          textStarted = 1;
          free(expanded);
        }
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "line_break") == 0) {
        cs_appendText("\n", 0);
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "text_image") == 0 || stricmp(command, "image") == 0) {
        // Add the command to the text as it should be rendered with it (on its own line)
        cs_appendBlockLine(line);
        textStarted = 1;
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        continue;
      }

      if (stricmp(command, "finish") == 0 && pg_isStatsActive()) {
        // End of the stats scene
        pg_game_status.gameEnded = 1;
        break;
      }

      if (stricmp(command, "finish") == 0) {
        cs_clearChoiceContexts(0);
        cs_manageFinishCommand(line);
        pg_game_status.showPageBreak = 1;
        pg_game_status.fpPosition = pg_game_status.fpSize;
        break;
      }

      if (stricmp(command, "goto") == 0) {
        // goto: we leave the option body (if any)
        cs_clearChoiceContexts(pg_game_status.gosubCount);
        long newPosition = cs_manageGotoCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        if (!cs_checkLoopLimit(newPosition))
          break;

        continue;
      }

      if (stricmp(command, "gosub") == 0) {
        //gosub
        long newPosition = cs_manageGosubCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        if (!cs_checkLoopLimit(newPosition))
          break;

        continue;
      }

      if (stricmp(command, "gosub_scene") == 0) {
        //gosub_scene
        long newPosition = cs_manageGosubSceneCommand(line);
        logDebug("%s -> %i", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        if (!cs_checkLoopLimit(newPosition))
          break;

        continue;
      }

      if (stricmp(command, "return") == 0) {
        if (pg_game_status.gosubCount == 0) {
          logWarning("*return without *gosub");
          pg_game_status.fpPosition = ftell(pg_game_status.fp);
          continue;
        }
        cs_popGosub();
        pg_game_status.fpPosition = ftell(pg_game_status.fp);
        logDebug("%s -> %li", line, pg_game_status.fpPosition);
        continue;
      }

      if (stricmp(command, "set") == 0) {
        // set varName expression
        int idx = 5;
        int wordEnd = cs_indexOfWordEnd(line, idx);
        if (wordEnd < 0) {
          logWarning("Missing variable name");
          continue;
        }
        char varName[256];
        char value[4096];
        subString(line, varName, idx, wordEnd - idx);
        idx += strlen(varName);
        subString(line, value, idx + 1, lineLen - idx);
        cs_setVarValue(varName, value);
        continue;
      }

      if (stricmp(command, "if") == 0) {
        // if: continue in the first branch with a true condition
        long newPosition = cs_manageIfCommand(line, lineIndentation);
        logDebug("%s -> %li", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        continue;
      }

      if (stricmp(command, "else") == 0 || stricmp(command, "elseif") == 0 || stricmp(command, "elsif") == 0) {
        // Reached at the end of the executed branch: skip the remaining branches
        long newPosition = cs_skipElseChain(lineIndentation);
        logDebug("%s -> %li", line, newPosition);
        pg_game_status.fpPosition = newPosition;
        continue;
      }

      if (stricmp(command, "temp") == 0) {
        // temp varName value
        cs_manageTempCommand(line);
        continue;
      }

      if (stricmp(command, "choice") == 0 || stricmp(command, "fake_choice") == 0) {
        // choice/fake_choice
        long newPosition = cs_manageChoiceCommand();
        logDebug("Choice -> %i", newPosition);
        pg_game_status.fpPosition = newPosition;
        break;
      }

      logWarning("Unknown command: %s", command);
    } else {
      // Text
      if (lineLen) {
        // Expand variables now: following *set commands could change their values
        char *expanded = cs_expandVariables(line);
        const char *textLine = expanded ? expanded : line;
        if (strlen(textLine) > 0) {
          if (cs_needsNewLine(textLine) && !endsWith("\n", pg_game_status.text))
            cs_appendText("\n", 0);
          else
            cs_appendLineSeparator();
          cs_appendText(textLine, 0);
          textStarted = 1;
        }
        free(expanded);
      } else if(textStarted) {
        // New paragraph
        cs_appendParagraphBreak();
      }
      pg_game_status.fpPosition = ftell(pg_game_status.fp);
    }
  }
  if (ftell(pg_game_status.fp) == pg_game_status.fpSize) {
    pg_game_status.fpPosition = pg_game_status.fpSize;
  }

  if (pg_game_status.text != NULL) {
    // Remove trailing new lines (except one)
    int end = strlen(pg_game_status.text) - 1;
    for (int i = end; i >= 0; i--) {
      if (pg_game_status.text[i] == '\n') {
        pg_game_status.text[i] = '\0';
      } else {
        break;
      }
    }
  }

  // Nothing to show at the end of the file: go to the next scene
  // (a choice or a page break at the end of the file must be shown)
  if (pg_game_status.text == NULL && pg_game_status.choicesCount == 0 && !pg_game_status.showPageBreak &&
      !pg_game_status.gameEnded && pg_game_status.fpPosition == pg_game_status.fpSize) {
    logDebug("Reached EOF (file size=%i), going to the next scene", pg_game_status.fpSize);
    cs_goToNextScene();

    return 0;
  } else {
    logDebug("Text to render:\n%s", pg_game_status.text);
  }

  return 1;
}

void cs_exec()
{
  // The saves contain the state at the start of the page
  if (!pg_isStatsActive())
    sv_takeSnapshot();
  while (!cs_exec_primitive());
}

void cs_appendText(const char *text, int newLine)
{
  if (!text)
    return;

  const size_t textLen = strlen(text);
  const size_t oldLen = pg_game_status.text ? strlen(pg_game_status.text) : 0;
  // realloc(NULL, size) works as malloc
  char *tmp = realloc(pg_game_status.text, oldLen + textLen + (newLine ? 1 : 0) + 1);
  if (tmp == NULL) {
    // Keep the text already on the page
    logWarning("Realloc failed");
    return;
  }

  memcpy(tmp + oldLen, text, textLen);
  size_t len = oldLen + textLen;
  if (newLine)
    tmp[len++] = '\n';
  tmp[len] = '\0';
  pg_game_status.text = tmp;
}

void cs_appendLineSeparator()
{
  // Same as ChoiceScript: the lines of a paragraph are joined with a space,
  // unless the previous line ends with a hyphen or a dash
  const char *text = pg_game_status.text;
  if (text == NULL)
    return;
  int len = strlen(text);
  if (len == 0 || text[len - 1] == '\n' || text[len - 1] == ' ' || text[len - 1] == '-')
    return;
  // U+2011 - U+2014 (non-breaking hyphen, figure dash, en dash, em dash)
  if (len >= 3 && (unsigned char)text[len - 3] == 0xE2 && (unsigned char)text[len - 2] == 0x80 &&
      (unsigned char)text[len - 1] >= 0x91 && (unsigned char)text[len - 1] <= 0x94)
    return;
  cs_appendText(" ", 0);
}

void cs_appendParagraphBreak()
{
  // A paragraph break is a single empty line, even after several empty lines
  const char *text = pg_game_status.text;
  if (text == NULL)
    return;
  int len = strlen(text);
  int newLines = 0;
  while (newLines < len && newLines < 2 && text[len - 1 - newLines] == '\n')
    newLines++;
  if (len == 0 || newLines >= 2)
    return;
  cs_appendText(newLines == 1 ? "\n" : "\n\n", 0);
}

void cs_appendBlockLine(const char *line)
{
  if (pg_game_status.text != NULL && strlen(pg_game_status.text) > 0 && !endsWith("\n", pg_game_status.text))
    cs_appendText("\n", 0);
  cs_appendText(line, 1);
}

/// @brief Evaluate a stat value for *stat_chart (number formatted without decimals)
static void cs_statValue(const char *variable, char *value)
{
  cs_evaluateExpression(variable, value);
  trim(value);
}

void cs_manageStatChartCommand(int indentation)
{
  char line[4096];
  char type[32];
  long lineStart;
  int rowIndentation = -1;

  // Same as ChoiceScript: the chart starts a new paragraph
  if (pg_game_status.text != NULL && strlen(pg_game_status.text) > 0 && !endsWith("\n\n", pg_game_status.text))
    cs_appendText(endsWith("\n", pg_game_status.text) ? "\n" : "\n\n", 0);

  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation <= indentation)
      break;
    if (rowIndentation < 0)
      rowIndentation = lineIndentation;
    if (lineIndentation > rowIndentation)
      continue; // Definition of the previous row

    trim(line);
    int typeLen = 0;
    while (line[typeLen] && !isspace((unsigned char)line[typeLen]) && typeLen < (int)sizeof(type) - 1) {
      type[typeLen] = line[typeLen];
      typeLen++;
    }
    type[typeLen] = '\0';
    char *data = line + typeLen;
    while (isspace((unsigned char)*data))
      data++;

    char variable[1024];
    char label[1024];
    char value[1024];
    char row[4096];
    if (stricmp(type, "opposed_pair") == 0) {
      snprintf(variable, sizeof(variable), "%s", data);
      // Labels: indented lines, their definitions are indented more
      char labels[2][1024];
      int labelsCount = 0;
      int labelIndentation = -1;
      char subLine[4096];
      long subStart;
      while (cs_readLine(subLine, sizeof(subLine), &subStart)) {
        if (cs_isBlank(subLine) || isComment(subLine))
          continue;
        int subIndentation = getIndentation(subLine);
        if (subIndentation <= rowIndentation) {
          fseek(pg_game_status.fp, subStart, SEEK_SET);
          break;
        }
        if (labelIndentation < 0)
          labelIndentation = subIndentation;
        if (subIndentation > labelIndentation)
          continue; // Definition
        trim(subLine);
        if (labelsCount < 2) {
          // Labels longer than the buffer are truncated
          subLine[sizeof(labels[0]) - 1] = '\0';
          strcpy(labels[labelsCount++], subLine);
        }
      }

      const char *left = labelsCount == 2 ? labels[0] : variable;
      const char *right = labelsCount == 2 ? labels[1] : labelsCount == 1 ? labels[0] : "";
      char *leftExpanded = cs_expandVariables(left);
      char *rightExpanded = cs_expandVariables(right);
      cs_statValue(variable, value);
      double number = atof(value);
      snprintf(row, sizeof(row), "*stat_bar %s|%s: %s%%|%s: %g%%", value, leftExpanded, value, rightExpanded, 100 - number);
      free(leftExpanded);
      free(rightExpanded);
      cs_appendBlockLine(row);
      continue;
    }

    if (stricmp(type, "text") != 0 && stricmp(type, "percent") != 0) {
      logWarning("*stat_chart: invalid row %s", line);
      continue;
    }

    // variable [label], the variable can be an expression in parentheses
    int variableEnd;
    if (data[0] == '(') {
      variableEnd = cs_findClosing(data, 0);
      variableEnd = variableEnd < 0 ? (int)strlen(data) : variableEnd + 1;
    } else {
      variableEnd = 0;
      while (data[variableEnd] && !isspace((unsigned char)data[variableEnd]))
        variableEnd++;
    }
    snprintf(variable, sizeof(variable), "%.*s", variableEnd, data);
    const char *labelStart = data + variableEnd;
    while (isspace((unsigned char)*labelStart))
      labelStart++;
    snprintf(label, sizeof(label), "%s", strlen(labelStart) ? labelStart : variable);
    char *labelExpanded = cs_expandVariables(label);
    cs_statValue(variable, value);

    if (stricmp(type, "text") == 0) {
      snprintf(row, sizeof(row), "%s: %s", labelExpanded, value);
      cs_appendBlockLine(row);
    } else {
      snprintf(row, sizeof(row), "*stat_bar %s|%s: %s%%", value, labelExpanded, value);
      cs_appendBlockLine(row);
    }
    free(labelExpanded);
  }
  if (feof(pg_game_status.fp))
    clearerr(pg_game_status.fp);
  else
    fseek(pg_game_status.fp, lineStart, SEEK_SET);
}

int cs_indexOfWordEnd(const char *string, const int startFrom)
{
  if (!string)
      return -1;

  int idx = startFrom;
  while (u8nextchar(string, &idx)) {
    if (string[idx] == ' ' || string[idx] == '\t' || string[idx] == '\n' || string[idx] == ',' ||
        string[idx] == '.' || string[idx] == ';' || string[idx] == '!' || string[idx] == '?') {
      return idx;
    }
  }

  return -1;
}

void cs_getCommand(const char *line, char* command)
{
  if (!line || !command)
    return;

  // Same as ChoiceScript: the command name contains only letters, digits and underscores,
  // so "*if(condition)" is the command "if" followed by "(condition)"
  int idx = 0;
  if (line[0] == '*') {
    for (int i = 1; line[i] && idx < 255; i++) {
      if (!isalnum((unsigned char)line[i]) && line[i] != '_')
        break;
      command[idx++] = line[i];
    }
  }
  command[idx] = '\0';
}

int cs_needsNewLine(const char *text)
{
  if (!text)
    return 0;

  // Bullet point
  if (strlen(text) > 2 && text[0] == -30 && text[1] == -128 && text[2] == -94)
    return 1;
  return 0;
}

/// @brief Read a line from the current file, removing the line terminator
/// @param line The buffer
/// @param size The buffer size
/// @param lineStart Set to the position of the line start
/// @return 0 on EOF
int cs_readLine(char *line, int size, long *lineStart)
{
  *lineStart = ftell(pg_game_status.fp);
  if (!fgets(line, size, pg_game_status.fp))
    return 0;
  line[strcspn(line, "\r\n")] = '\0';
  return 1;
}

/// @brief Check if a line is empty or contains only whitespaces
int cs_isBlank(const char *line)
{
  for (int i = 0; line[i]; i++) {
    if (!isspace((unsigned char)line[i]))
      return 0;
  }
  return 1;
}

/// @brief Get the arguments of a command (the text after the command name)
/// @param line The trimmed line (*command args)
/// @param args The buffer for the arguments
/// @param size The buffer size
void cs_getCommandArgs(const char *line, char *args, int size)
{
  int i = 0;
  if (line[i] == '*')
    i++;
  // Command name: letters, digits and underscores (see cs_getCommand)
  while (line[i] && (isalnum((unsigned char)line[i]) || line[i] == '_'))
    i++;
  while (line[i] && isspace((unsigned char)line[i]))
    i++;
  snprintf(args, size, "%s", line + i);
  trim(args);
}

static int cs_isElseCommand(const char *command)
{
  return stricmp(command, "else") == 0 || stricmp(command, "elseif") == 0 || stricmp(command, "elsif") == 0;
}

/// @brief Skip the body of a false branch and search the next branch to execute
/// @param indentation The indentation of the *if/*elseif/*else lines
/// @return The position of the body of the first true *elseif or of the *else,
///         or the position of the first line after the if
static long cs_findNextBranch(int indentation)
{
  char line[4096];
  char command[256];
  char condition[4096];
  long lineStart;
  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation > indentation)
      continue; // Body of the false branch
    if (lineIndentation < indentation)
      break;

    trim(line);
    cs_getCommand(line, command);
    if (stricmp(command, "else") == 0) {
      return ftell(pg_game_status.fp);
    }
    if (stricmp(command, "elseif") == 0 || stricmp(command, "elsif") == 0) {
      cs_getCommandArgs(line, condition, sizeof(condition));
      int res = cs_evaluateCondition(condition, indentation);
      logDebug("\t*%s %s -> %s", command, condition, res ? "true" : "false");
      if (res)
        return ftell(pg_game_status.fp);
      continue;
    }
    break;
  }

  fseek(pg_game_status.fp, lineStart, SEEK_SET);
  return lineStart;
}

long cs_skipElseChain(int indentation)
{
  char line[4096];
  char command[256];
  long lineStart;
  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation > indentation)
      continue; // Body of a skipped branch
    if (lineIndentation < indentation)
      break;

    trim(line);
    cs_getCommand(line, command);
    if (cs_isElseCommand(command))
      continue;
    break;
  }

  fseek(pg_game_status.fp, lineStart, SEEK_SET);
  return lineStart;
}

long cs_manageIfCommand(const char *text, int indentation)
{
  char condition[4096];
  cs_getCommandArgs(text, condition, sizeof(condition));

  int res = cs_evaluateCondition(condition, indentation);
  logDebug("\t*if %s -> %s", condition, res ? "true" : "false");
  if (res)
    return ftell(pg_game_status.fp);

  return cs_findNextBranch(indentation);
}

int cs_get_operator(const char *condition, char *operator)
{
  // Operators: ['=', '!=', '>', '>=', '<', '<=']
  operator[0] = '\0';
  int len = strlen(condition);
  for (int i = 0; i < len; i++) {
    if (condition[i] == '!' && i + 1 < len && condition[i + 1] == '=') {
      strcpy(operator, "!=");
      return i;
    }

    if (condition[i] == '>' && i + 1 < len && condition[i + 1] == '=') {
      strcpy(operator, ">=");
      return i;
    }

    if (condition[i] == '<' && i + 1 < len && condition[i + 1] == '=') {
      strcpy(operator, "<=");
      return i;
    }

    if (condition[i] == '>') {
      strcpy(operator, ">");
      return i;
    }

    if (condition[i] == '<') {
      strcpy(operator, "<");
      return i;
    }

    if (condition[i] == '=') {
      strcpy(operator, "=");
      return i;
    }
  }

  return -1;
}

int cs_evaluateCondition(const char *condition, int indentation)
{
  // Supports parentheses, and, or, not
  return cs_evaluateBoolean(condition);
}

int cs_evaluateSimpleCondition(const char *condition)
{
  char operator[3];
  int res;

  int opIdx = cs_get_operator(condition, operator);
  if (opIdx < 0) {
    // just a variable name or a reference (boolean condition)
    char value[1024];
    cs_evaluateExpression(condition, value);
    trim(value);
    res = stricmp(value, "true") == 0;
    logDebug("\tCondition (%s) -> %s", condition, res ? "true" : "false");
    return res;
  }

  // expr OPT expr
  char left_expression[1024];
  subString(condition, left_expression, 0, opIdx);
  trim(left_expression);
  char right_expression[1024];
  const int opLen = strlen(operator);
  subString(condition, right_expression, opIdx + opLen, strlen(condition) - opIdx - opLen);
  trim(right_expression);

  char left_value[1024];
  char right_value[1024];

  cs_evaluateExpression(left_expression, left_value);
  int int_value;
  if (is_number(left_value, &int_value)) {
    sprintf(left_value, "%010i", int_value);
  }
  cs_evaluateExpression(right_expression, right_value);
  if (is_number(right_value, &int_value)) {
    sprintf(right_value, "%010i", int_value);
  }

  logDebug("\tleft=%s (%s), right=%s (%s), op='%s'", left_expression, left_value, right_expression, right_value, operator);
  if (stricmp(operator, "=") == 0) {
    res = stricmp(left_value, right_value) == 0 ? 1 : 0;
  } else if (stricmp(operator, "!=") == 0) {
    res = stricmp(left_value, right_value) != 0 ? 1 : 0;
  } else if (stricmp(operator, ">") == 0) {
    res = stricmp(left_value, right_value) > 0 ? 1 : 0;
  } else if (stricmp(operator, ">=") == 0) {
    res = stricmp(left_value, right_value) >= 0 ? 1 : 0;
  } else if (stricmp(operator, "<") == 0) {
    res = stricmp(left_value, right_value) < 0 ? 1 : 0;
  } else if (stricmp(operator, "<=") == 0) {
    res = stricmp(left_value, right_value) <= 0 ? 1 : 0;
  } else {
    logWarning("\tUnknown operator: %s", operator);
    return 0;
  }

  logDebug("\tCondition (%s) -> %s", condition, res ? "true" : "false");
  return res;
}

/* ---------------------------------------------------------------------------
 * Arithmetic expressions: numbers, variables, parentheses, + - * / modulo
 * ------------------------------------------------------------------------- */

static int cs_arithExpr(const char **p, double *out);

static void cs_arithSkipSpaces(const char **p)
{
  while (**p && isspace((unsigned char)**p))
    (*p)++;
}

static int cs_arithFactor(const char **p, double *out)
{
  cs_arithSkipSpaces(p);
  if (**p == '(') {
    (*p)++;
    if (!cs_arithExpr(p, out))
      return 0;
    cs_arithSkipSpaces(p);
    if (**p != ')')
      return 0;
    (*p)++;
    return 1;
  }

  if (**p == '-') {
    (*p)++;
    if (!cs_arithFactor(p, out))
      return 0;
    *out = -*out;
    return 1;
  }

  if (isdigit((unsigned char)**p) || **p == '.') {
    char *end;
    *out = strtod(*p, &end);
    *p = end;
    return 1;
  }

  if (isalpha((unsigned char)**p) || **p == '_') {
    char name[256];
    int len = 0;
    while ((isalnum((unsigned char)**p) || **p == '_') && len < (int)sizeof(name) - 1) {
      name[len++] = **p;
      (*p)++;
    }
    name[len] = '\0';

    // Functions: round(expr)
    const char *afterName = *p;
    cs_arithSkipSpaces(p);
    if (**p == '(' && stricmp(name, "round") == 0) {
      double arg;
      if (!cs_arithFactor(p, &arg))
        return 0;
      *out = floor(arg + 0.5); // Same as JavaScript Math.round
      return 1;
    }
    *p = afterName;

    variable *var = cs_getVar(name);
    if (var == NULL || var->value == NULL)
      return 0;
    char *end;
    *out = strtod(var->value, &end);
    if (end == var->value) {
      logWarning("Variable %s is not a number (%s)", name, var->value);
      return 0;
    }
    return 1;
  }

  return 0;
}

static int cs_arithTerm(const char **p, double *out)
{
  if (!cs_arithFactor(p, out))
    return 0;

  while (1) {
    cs_arithSkipSpaces(p);
    char op = **p;
    if (op == '*' || op == '/') {
      (*p)++;
    } else if (cs_strnicmp(*p, "modulo", 6) == 0 && !isalnum((unsigned char)(*p)[6]) && (*p)[6] != '_') {
      op = '%';
      *p += 6;
    } else {
      return 1;
    }

    double right;
    if (!cs_arithFactor(p, &right))
      return 0;
    if (op == '*') {
      *out *= right;
    } else if (right == 0) {
      logWarning("Division by zero");
      return 0;
    } else if (op == '/') {
      *out /= right;
    } else {
      *out = fmod(*out, right);
    }
  }
}

static int cs_arithExpr(const char **p, double *out)
{
  if (!cs_arithTerm(p, out))
    return 0;

  while (1) {
    cs_arithSkipSpaces(p);
    char op = **p;
    if (op != '+' && op != '-')
      return 1;
    (*p)++;

    double right;
    if (!cs_arithTerm(p, &right))
      return 0;
    *out = op == '+' ? *out + right : *out - right;
  }
}

/// @brief Evaluate an arithmetic expression
/// @param expression The expression
/// @param result The result
/// @return 1 on success
int cs_evaluateArithmetic(const char *expression, double *result)
{
  const char *p = expression;
  if (!cs_arithExpr(&p, result))
    return 0;
  cs_arithSkipSpaces(&p);
  return *p == '\0';
}

/// @brief Find a top level '&' (string concatenation) outside quotes and parentheses
static int cs_findConcatenation(const char *expression)
{
  int depth = 0;
  int inQuote = 0;
  for (int i = 0; expression[i]; i++) {
    if (expression[i] == '"' && (i == 0 || expression[i - 1] != '\\'))
      inQuote = !inQuote;
    if (inQuote)
      continue;
    if (expression[i] == '(')
      depth++;
    else if (expression[i] == ')')
      depth--;
    else if (expression[i] == '&' && depth == 0)
      return i;
  }
  return -1;
}

/// @brief Resolve the name of a reference {expr}: the value of expr is the name of the variable
/// @param content The text between the braces
/// @param name Buffer for the variable name (or the value if it is a number or a boolean)
static void cs_evaluateReferenceName(const char *content, char *name, int size)
{
  char *inner = cs_strndup(content, strlen(content));
  trim(inner);
  char value[1024];
  cs_evaluateExpression(inner, value);
  free(inner);
  trim(value);
  snprintf(name, size, "%s", value);
}

/// @brief Replace the references {expr} with the value of the referenced variable
/// @return The new expression (must be freed) or NULL on error
static char *cs_resolveReferences(const char *expression)
{
  cs_strbuf sb = { NULL, 0, 0 };
  int inQuote = 0;
  int len = strlen(expression);
  for (int i = 0; i < len; i++) {
    if (expression[i] == '"' && (i == 0 || expression[i - 1] != '\\'))
      inQuote = !inQuote;

    if (!inQuote && expression[i] == '{') {
      int close = cs_findClosing(expression, i);
      if (close < 0) {
        logWarning("Missing '}' in %s", expression);
        free(sb.buf);
        return NULL;
      }
      char *content = cs_strndup(expression + i + 1, close - i - 1);
      char name[1024];
      cs_evaluateReferenceName(content, name, sizeof(name));
      free(content);

      int intValue;
      if ((strlen(name) > 0 && is_number(name, &intValue)) || stricmp(name, "true") == 0 || stricmp(name, "false") == 0) {
        // Same as ChoiceScript: numbers and booleans are their own value
        cs_sb_append(&sb, name, strlen(name));
      } else {
        variable *var = cs_getVar(name);
        if (var == NULL || var->value == NULL) {
          logWarning("Reference {%s}: variable not found", name);
          free(sb.buf);
          return NULL;
        }
        if (var->type == string)
          cs_sb_append(&sb, "\"", 1);
        cs_sb_append(&sb, var->value, strlen(var->value));
        if (var->type == string)
          cs_sb_append(&sb, "\"", 1);
      }
      i = close;
      continue;
    }
    cs_sb_append(&sb, &expression[i], 1);
  }
  if (sb.buf == NULL)
    return cs_strndup("", 0);
  return sb.buf;
}

int cs_evaluateExpression(const char *expression, char *value)
{
  value[0] = '\0';
  if (!expression)
    return 0;

  int expLen = strlen(expression);

  // References: {var_name_expression}
  if (strchr(expression, '{') != NULL && !(expLen > 1 && expression[0] == '"' && expression[expLen - 1] == '"')) {
    char *resolved = cs_resolveReferences(expression);
    if (resolved == NULL)
      return 0;
    int res = 0;
    if (strchr(resolved, '{') == NULL || strcmp(resolved, expression) != 0)
      res = cs_evaluateExpression(resolved, value);
    free(resolved);
    return res;
  }

  // String concatenation: "text" & var
  int concat = cs_findConcatenation(expression);
  if (concat > 0) {
    char *left = cs_strndup(expression, concat);
    char *right = cs_strndup(expression + concat + 1, expLen - concat - 1);
    trim(left);
    trim(right);
    char leftValue[1024];
    char rightValue[1024];
    int res = cs_evaluateExpression(left, leftValue) & cs_evaluateExpression(right, rightValue);
    // value is at least 1024 bytes
    size_t leftLen = strlen(leftValue);
    memcpy(value, leftValue, leftLen + 1);
    for (size_t i = 0; rightValue[i] && leftLen < 1023; i++)
      value[leftLen++] = rightValue[i];
    value[leftLen] = '\0';
    free(left);
    free(right);
    return res;
  }
  if (expLen > 1 && expression[0] == '"' && expression[expLen - 1] == '"') {
    // String constant (can contain ${} substitutions)
    subString(expression, value, 1, expLen - 2);
    if (indexOf(value, '{', 0) >= 0) {
      char *expanded = cs_expandVariables(value);
      if (expanded) {
        strncpy(value, expanded, 1023);
        value[1023] = '\0';
        free(expanded);
      }
    }

    return 1;
  }

  int int_value;
  if (expLen > 2 && expression[0] == '%' && (expression[1] == '+' || expression[1] == '-')) {
    if (is_number(expression + 2, &int_value)) {
      // Fairmath: %+10
      strcpy(value, expression);
      return 1;
    } else {
      // Fairmath: %+var_name
      variable *var = cs_getVar(expression + 2);
      if (var != NULL) {
        strncpy(value, expression, 2);
        strcat(value, var->value);

        return 1;
      }
    }
  }

  if (is_number(expression, &int_value)) {
    // Number constant
    strcpy(value, expression);
    return 1;
  }

  // Boolean value
  if (stricmp(expression, "true") == 0 || stricmp(expression, "false") == 0) {
    strcpy(value, expression);
    return 1;
  }

  // Arithmetic expression: 3 * (var + 1)
  if (strpbrk(expression, "+-*/()") != NULL || strstr(expression, " modulo ") != NULL) {
    double result;
    if (cs_evaluateArithmetic(expression, &result)) {
      if (fabs(result - round(result)) < 1e-9)
        sprintf(value, "%li", (long)round(result));
      else
        sprintf(value, "%g", result);
      return 1;
    }
  }

  // Variable name
  variable *var = cs_getVar(expression);
  if (var != NULL) {
    strcpy(value, var->value);
    return 1;
  }

  logWarning("Expression not evaluated: %s", expression);
  return 0;
}

long cs_manageGotoCommand(const char *text)
{
  int start = indexOfWordBreak(text, 0);
  if (start < 0) {
    logWarning("goto without label name");
    return ftell(pg_game_status.fp);
  }

  char labelName[256];
  subString(text, labelName, start + 1, strlen(text) - start - 1);

  label *lbl = cs_get_label(labelName);
  if (lbl == NULL) {
    logWarning("Label %s not found", labelName);
    return ftell(pg_game_status.fp);
  }
  fseek(pg_game_status.fp, lbl->position, SEEK_SET);
  return lbl->position;
}

/// @brief Open a scene file and make it the current one
/// @param sceneName The scene name (file name without extension)
/// @param closeCurrent Set to 1 to close the current file
/// @return 1 on success
int cs_openScene(const char *sceneName, int closeCurrent)
{
  char filePath[512];
  snprintf(filePath, sizeof(filePath), "%s/scenes/%s.txt", runningGame->path, sceneName);
  FILE *fp = fopen(filePath, "rb");
  if (!fp) {
    logError("Failed to open file %s", filePath);
    return 0;
  }
  logInfo("Opened file %s", filePath);

  if (closeCurrent && pg_game_status.fp)
    fclose(pg_game_status.fp);
  pg_game_status.fp = fp;
  cs_clear_labels();
  pg_clear_image_cache();

  fseek(pg_game_status.fp, 0L, SEEK_END);
  pg_game_status.fpSize = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0L, SEEK_SET);

  // Skip the UTF8 BOM
  unsigned char bom[3];
  if (fread(bom, 1, 3, pg_game_status.fp) != 3 || bom[0] != 0xEF || bom[1] != 0xBB || bom[2] != 0xBF)
    fseek(pg_game_status.fp, 0L, SEEK_SET);
  pg_game_status.fpPosition = ftell(pg_game_status.fp);

  snprintf(pg_game_status.sceneName, sizeof(pg_game_status.sceneName), "%s", sceneName);
  // Same as ChoiceScript: these settings are temporary values of the scene
  pg_game_status.choiceReuse = reuse_allow;
  pg_game_status.loopLimit = 1000;
  // Scenes not in the scene list (e.g. used with *gosub_scene) keep the current index
  for (int i = 0; i < runningGame->scenesCount; i++) {
    if (stricmp(runningGame->scenes[i], sceneName) == 0) {
      pg_game_status.sceneIndex = i;
      break;
    }
  }
  return 1;
}

int cs_goToNextScene()
{
  if (pg_isStatsActive()) {
    // End of the stats scene
    pg_game_status.gameEnded = 1;
    return 0;
  }

  if (pg_game_status.sceneIndex + 1 >= runningGame->scenesCount) {
    logInfo("No next scene: game ended");
    pg_game_status.gameEnded = 1;
    return 0;
  }

  cs_clearTempVars();
  cs_clearChoiceContexts(0);
  cs_clearGosubStack();
  if (!cs_openScene(runningGame->scenes[pg_game_status.sceneIndex + 1], 1)) {
    pg_game_status.gameEnded = 1;
    return 0;
  }
  return 1;
}

/// @brief Split command arguments in tokens: words, "strings", (expressions), {references}
/// @param text The arguments
/// @param tokens The tokens (must be freed)
/// @param maxTokens The maximum number of tokens
/// @return The number of tokens
static int cs_tokenizeArgs(const char *text, char **tokens, int maxTokens)
{
  int count = 0;
  int len = strlen(text);
  int i = 0;
  while (i < len) {
    while (i < len && isspace((unsigned char)text[i]))
      i++;
    if (i >= len)
      break;

    int start = i;
    if (text[i] == '"') {
      i++;
      while (i < len && !(text[i] == '"' && text[i - 1] != '\\'))
        i++;
      if (i < len)
        i++;
    } else if (text[i] == '(' || text[i] == '{') {
      int close = cs_findClosing(text, i);
      i = close < 0 ? len : close + 1;
    } else {
      while (i < len && !isspace((unsigned char)text[i]))
        i++;
    }

    if (count < maxTokens) {
      tokens[count++] = cs_strndup(text + start, i - start);
    } else {
      logWarning("Too many arguments: %s", text);
      break;
    }
  }
  return count;
}

static void cs_freeTokens(char **tokens, int count)
{
  for (int i = 0; i < count; i++)
    free(tokens[i]);
}

void cs_clearParams()
{
  for (int i = 0; i < pg_game_status.paramsCount; i++) {
    free(pg_game_status.params[i]);
    pg_game_status.params[i] = NULL;
  }
  pg_game_status.paramsCount = 0;
}

/// @brief Evaluate the arguments of *gosub, *gosub_scene, *goto_scene (used by *params)
static void cs_setParams(char **tokens, int count)
{
  cs_clearParams();
  for (int i = 0; i < count && i < (int)(sizeof(pg_game_status.params) / sizeof(char*)); i++) {
    char value[4096];
    cs_evaluateExpression(tokens[i], value);
    pg_game_status.params[pg_game_status.paramsCount++] = cs_strndup(value, strlen(value));
    logDebug("\tparam_%i=%s", i + 1, value);
  }
}

static void cs_freeVariables(variable *vars, int count)
{
  if (vars == NULL)
    return;
  for (int i = 0; i < count; i++) {
    free(vars[i].name);
    free(vars[i].value);
  }
  free(vars);
}

/// @brief Create or set a temporary variable (the type is detected from the value)
void cs_setTempVar(const char *name, const char *value)
{
  variable *var = NULL;
  for (int i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, name) == 0) {
      var = &pg_game_status.tempVariables[i];
      break;
    }
  }
  if (var == NULL) {
    var = cs_add_temp_var();
    if (var == NULL)
      return;
    var->name = cs_strndup(name, strlen(name));
  }

  int intValue;
  if (stricmp(value, "true") == 0 || stricmp(value, "false") == 0)
    var->type = boolean;
  else if (strlen(value) > 0 && is_number(value, &intValue))
    var->type = number;
  else
    var->type = string;

  free(var->value);
  var->value = cs_strndup(value, strlen(value));
  logDebug("\tTEMPVAR %s=%s", var->name, var->value);
}

/// @brief Push a gosub frame
/// @param isScene Set to 1 for *gosub_scene: the current file and temporary variables are saved
/// @return The frame or NULL
static gosub_info *cs_pushGosub(int isScene)
{
  if (pg_game_status.gosubCount + 1 >= (int)(sizeof(pg_game_status.gosub) / sizeof(gosub_info))) {
    logWarning("Too many nested gosub");
    return NULL;
  }

  pg_game_status.gosubCount++;
  gosub_info *frame = &pg_game_status.gosub[pg_game_status.gosubCount];
  memset(frame, 0, sizeof(gosub_info));
  frame->position = ftell(pg_game_status.fp);
  if (isScene) {
    frame->fp = pg_game_status.fp;
    frame->fpSize = pg_game_status.fpSize;
    frame->sceneIndex = pg_game_status.sceneIndex;
    snprintf(frame->sceneName, sizeof(frame->sceneName), "%s", pg_game_status.sceneName);
    // The scene called with gosub_scene has its own temporary variables
    frame->tempVariables = pg_game_status.tempVariables;
    frame->tempVariablesCount = pg_game_status.tempVariablesCount;
    pg_game_status.tempVariables = NULL;
    pg_game_status.tempVariablesCount = 0;
    frame->choiceReuse = pg_game_status.choiceReuse;
    frame->loopLimit = pg_game_status.loopLimit;
  }
  return frame;
}

/// @brief Pop the last gosub frame, restoring the file and the temporary variables of a *gosub_scene
static void cs_popGosub()
{
  gosub_info *frame = &pg_game_status.gosub[pg_game_status.gosubCount];
  if (frame->fp != NULL) {
    if (pg_game_status.fp)
      fclose(pg_game_status.fp);
    pg_game_status.fp = frame->fp;
    pg_game_status.fpSize = frame->fpSize;
    pg_game_status.sceneIndex = frame->sceneIndex;
    snprintf(pg_game_status.sceneName, sizeof(pg_game_status.sceneName), "%s", frame->sceneName);
    cs_clear_labels();

    cs_clearTempVars();
    pg_game_status.tempVariables = frame->tempVariables;
    pg_game_status.tempVariablesCount = frame->tempVariablesCount;
    pg_game_status.choiceReuse = frame->choiceReuse;
    pg_game_status.loopLimit = frame->loopLimit;
  }
  fseek(pg_game_status.fp, frame->position, SEEK_SET);
  memset(frame, 0, sizeof(gosub_info));
  pg_game_status.gosubCount--;
  // Remove the choice contexts of the subroutine
  cs_clearChoiceContexts(pg_game_status.gosubCount + 1);
}

void cs_clearGosubStack()
{
  for (int i = 1; i <= pg_game_status.gosubCount; i++) {
    gosub_info *frame = &pg_game_status.gosub[i];
    if (frame->fp != NULL)
      fclose(frame->fp);
    cs_freeVariables(frame->tempVariables, frame->tempVariablesCount);
    memset(frame, 0, sizeof(gosub_info));
  }
  pg_game_status.gosubCount = 0;
}

long cs_manageGosubCommand(const char *text)
{
  // *gosub label arg1 arg2...
  char args[4096];
  char *tokens[34];
  cs_getCommandArgs(text, args, sizeof(args));
  int count = cs_tokenizeArgs(args, tokens, 34);
  if (count == 0) {
    logWarning("gosub without label name");
    return ftell(pg_game_status.fp);
  }

  label *lbl = cs_get_label(tokens[0]);
  if (lbl == NULL) {
    logWarning("Label %s not found", tokens[0]);
    cs_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }
  long position = lbl->position;

  cs_setParams(tokens + 1, count - 1);
  cs_freeTokens(tokens, count);
  if (cs_pushGosub(0) == NULL)
    return ftell(pg_game_status.fp);

  fseek(pg_game_status.fp, position, SEEK_SET);
  return position;
}

long cs_manageGosubSceneCommand(const char *text)
{
  // *gosub_scene scene [label] [arg1 arg2...]
  char args[4096];
  char *tokens[34];
  cs_getCommandArgs(text, args, sizeof(args));
  int count = cs_tokenizeArgs(args, tokens, 34);
  if (count == 0) {
    logWarning("gosub_scene without scene name");
    return ftell(pg_game_status.fp);
  }

  // Evaluate the arguments with the variables of the current scene
  cs_setParams(tokens + 2, count > 2 ? count - 2 : 0);

  if (cs_pushGosub(1) == NULL) {
    cs_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }
  if (!cs_openScene(tokens[0], 0)) {
    // Restore the caller (the current file was not replaced)
    gosub_info *frame = &pg_game_status.gosub[pg_game_status.gosubCount];
    frame->fp = NULL;
    pg_game_status.tempVariables = frame->tempVariables;
    pg_game_status.tempVariablesCount = frame->tempVariablesCount;
    frame->tempVariables = NULL;
    pg_game_status.gosubCount--;
    cs_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }

  long position = pg_game_status.fpPosition;
  if (count > 1) {
    label *lbl = cs_get_label(tokens[1]);
    if (lbl == NULL)
      logWarning("Label %s not found in scene %s", tokens[1], tokens[0]);
    else
      position = lbl->position;
  }
  cs_freeTokens(tokens, count);

  fseek(pg_game_status.fp, position, SEEK_SET);
  return position;
}

long cs_manageGotoSceneCommand(const char *text)
{
  // *goto_scene scene [label] [arg1 arg2...]
  char args[4096];
  char *tokens[34];
  cs_getCommandArgs(text, args, sizeof(args));
  int count = cs_tokenizeArgs(args, tokens, 34);
  if (count == 0) {
    logWarning("goto_scene without scene name");
    return ftell(pg_game_status.fp);
  }

  cs_setParams(tokens + 2, count > 2 ? count - 2 : 0);

  if (pg_game_status.gosubCount > 0) {
    logWarning("*goto_scene after *gosub_scene without *return");
    cs_clearGosubStack();
  }
  cs_clearChoiceContexts(0);

  if (!cs_openScene(tokens[0], 1)) {
    cs_freeTokens(tokens, count);
    return ftell(pg_game_status.fp);
  }
  cs_clearTempVars();

  long position = pg_game_status.fpPosition;
  if (count > 1) {
    label *lbl = cs_get_label(tokens[1]);
    if (lbl == NULL)
      logWarning("Label %s not found in scene %s", tokens[1], tokens[0]);
    else
      position = lbl->position;
  }
  cs_freeTokens(tokens, count);

  fseek(pg_game_status.fp, position, SEEK_SET);
  return position;
}

static int cs_checkLoopLimit(long position)
{
  unsigned int hash = 5381;
  for (const char *c = pg_game_status.sceneName; *c; c++)
    hash = hash * 33 + (unsigned char)tolower((unsigned char)*c);

  label_visit *visit = NULL;
  for (int i = 0; i < pg_game_status.labelVisitsCount; i++) {
    if (pg_game_status.labelVisits[i].position == position && pg_game_status.labelVisits[i].sceneHash == hash) {
      visit = &pg_game_status.labelVisits[i];
      break;
    }
  }
  if (visit == NULL) {
    if (pg_game_status.labelVisitsCount >= (int)(sizeof(pg_game_status.labelVisits) / sizeof(label_visit)))
      return 1; // Table full: stop tracking new labels
    visit = &pg_game_status.labelVisits[pg_game_status.labelVisitsCount++];
    visit->sceneHash = hash;
    visit->position = position;
    visit->count = 0;
  }

  visit->count++;
  // Same as ChoiceScript: 0 means no limit
  if (pg_game_status.loopLimit > 0 && visit->count > pg_game_status.loopLimit) {
    logError("Infinite loop: position %li of scene %s visited too many times (%i)", position, pg_game_status.sceneName, pg_game_status.loopLimit);
    char message[512];
    snprintf(message, sizeof(message), "\n\n[b]Error:[/b] infinite loop in scene %s (a label was visited more than %i times). The game cannot continue.",
             pg_game_status.sceneName, pg_game_status.loopLimit);
    cs_appendText(message, 0);
    pg_game_status.gameEnded = 1;
    pg_game_status.showPageBreak = 0;
    return 0;
  }
  return 1;
}

/// @brief State of the random number generator (xorshift64*), saved with the game
uint64_t cs_randomState = 0;

void cs_randomSeed()
{
  if (cs_randomState == 0) {
    cs_randomState = ((uint64_t)time(NULL) << 32) ^ (uint64_t)clock() ^ 0x9E3779B97F4A7C15ULL;
    if (cs_randomState == 0)
      cs_randomState = 1;
  }
}

static double cs_random()
{
  cs_randomSeed();
  cs_randomState ^= cs_randomState >> 12;
  cs_randomState ^= cs_randomState << 25;
  cs_randomState ^= cs_randomState >> 27;
  uint64_t value = cs_randomState * 0x2545F4914F6CDD1DULL;
  // 53 random bits -> [0, 1)
  return (value >> 11) * (1.0 / 9007199254740992.0);
}

/// @brief The values of *config: in the Choice of Games apps they come from a remote configuration
static const char *cs_remoteConfig[][2] = {
  // Skip the description of the game shown at startup
  { "skip_description", "true" },
};

void cs_manageConfigCommand(const char *text)
{
  // Same as *set: *config variable value
  char args[4096];
  cs_getCommandArgs(text, args, sizeof(args));
  int nameEnd = 0;
  if (args[0] == '{') {
    nameEnd = cs_findClosing(args, 0);
    nameEnd = nameEnd < 0 ? (int)strlen(args) : nameEnd + 1;
  } else {
    while (args[nameEnd] && !isspace((unsigned char)args[nameEnd]))
      nameEnd++;
  }
  char *name = cs_strndup(args, nameEnd);
  const char *value = args + nameEnd;
  while (isspace((unsigned char)*value))
    value++;
  if (strlen(name) == 0 || strlen(value) == 0) {
    logWarning("Invalid config instruction: %s", text);
    free(name);
    return;
  }

  if (!cs_setVarValue(name, value)) {
    logWarning("*config: non-existent variable %s", name);
    free(name);
    return;
  }

  // Then the remote configuration (if any) replaces the value
  const char *varName = name;
  char refName[1024];
  if (name[0] == '{') {
    char *content = cs_strndup(name + 1, strlen(name) - 2);
    cs_evaluateReferenceName(content, refName, sizeof(refName));
    free(content);
    varName = refName;
  }
  for (size_t i = 0; i < sizeof(cs_remoteConfig) / sizeof(cs_remoteConfig[0]); i++) {
    if (stricmp(cs_remoteConfig[i][0], varName) == 0) {
      variable *var = cs_getVar(varName);
      if (var != NULL) {
        cs_assignVar(var, cs_remoteConfig[i][1]);
        logDebug("*config %s: remote value %s", varName, cs_remoteConfig[i][1]);
      }
      break;
    }
  }
  free(name);
}

void cs_manageRandCommand(const char *text)
{
  char args[4096];
  char *tokens[8];
  cs_getCommandArgs(text, args, sizeof(args));
  int count = cs_tokenizeArgs(args, tokens, 8);
  if (count != 3) {
    logWarning("Invalid rand statement, expected three args: varname min max (%s)", text);
    cs_freeTokens(tokens, count);
    return;
  }

  // The variable can be a reference: *rand {name} 1 6
  char name[1024];
  int nameLen = strlen(tokens[0]);
  if (nameLen > 1 && tokens[0][0] == '{' && tokens[0][nameLen - 1] == '}') {
    char *content = cs_strndup(tokens[0] + 1, nameLen - 2);
    cs_evaluateReferenceName(content, name, sizeof(name));
    free(content);
  } else {
    cs_copyString(name, sizeof(name), tokens[0]);
  }
  variable *var = cs_getVar(name);

  char minValue[1024];
  char maxValue[1024];
  cs_evaluateExpression(tokens[1], minValue);
  cs_evaluateExpression(tokens[2], maxValue);
  trim(minValue);
  trim(maxValue);
  cs_freeTokens(tokens, count);
  if (var == NULL) {
    logWarning("*rand: non-existent variable %s", name);
    return;
  }

  char *end1, *end2;
  double minimum = strtod(minValue, &end1);
  double maximum = strtod(maxValue, &end2);
  if (end1 == minValue || end2 == maxValue || *end1 || *end2) {
    logWarning("*rand: min and max must be numbers (%s, %s)", minValue, maxValue);
    return;
  }
  if (maximum < minimum) {
    logWarning("*rand: min must be less than max: %s > %s", minValue, maxValue);
    return;
  }

  // Same as ChoiceScript: an integer between min and max (included) if both are integers
  double result;
  int intValue;
  if (maximum == minimum)
    result = minimum;
  else if (is_number(minValue, &intValue) && is_number(maxValue, &intValue) && !strchr(minValue, '.') && !strchr(maxValue, '.'))
    result = minimum + floor(cs_random() * (maximum - minimum + 1));
  else
    result = minimum + cs_random() * (maximum - minimum);

  char value[64];
  cs_formatNumber(result, value);
  cs_assignVar(var, value);
  var->type = number;
  logDebug("*rand %s %s %s -> %s", name, minValue, maxValue, value);
}

void cs_manageParamsCommand(const char *text)
{
  // *params name1 name2...: name the arguments of gosub, they are also available as param_1, param_2...
  char args[4096];
  char *tokens[34];
  char name[32];
  cs_getCommandArgs(text, args, sizeof(args));
  int count = cs_tokenizeArgs(args, tokens, 34);

  int i;
  for (i = 0; i < count; i++) {
    if (i >= pg_game_status.paramsCount) {
      logWarning("No parameter passed for %s", tokens[i]);
      break;
    }
    cs_setTempVar(tokens[i], pg_game_status.params[i]);
    snprintf(name, sizeof(name), "param_%i", i + 1);
    cs_setTempVar(name, pg_game_status.params[i]);
  }
  for (; i < pg_game_status.paramsCount; i++) {
    snprintf(name, sizeof(name), "param_%i", i + 1);
    cs_setTempVar(name, pg_game_status.params[i]);
  }
  snprintf(name, sizeof(name), "%i", pg_game_status.paramsCount);
  cs_setTempVar("param_count", name);
  cs_freeTokens(tokens, count);
}

void cs_manageTempCommand(const char *text)
{
  // *temp name [value]
  char args[4096];
  cs_getCommandArgs(text, args, sizeof(args));
  int nameEnd = 0;
  while (args[nameEnd] && !isspace((unsigned char)args[nameEnd]))
    nameEnd++;
  if (nameEnd == 0) {
    logWarning("*temp without variable name");
    return;
  }

  char *name = cs_strndup(args, nameEnd);
  char *rawValue = args + nameEnd;
  while (isspace((unsigned char)*rawValue))
    rawValue++;

  char value[4096] = "";
  if (*rawValue)
    cs_evaluateExpression(rawValue, value);
  cs_setTempVar(name, value);

  // A quoted value is always a string ("123")
  if (rawValue[0] == '"') {
    variable *var = cs_getVar(name);
    if (var != NULL)
      var->type = string;
  }
  free(name);
}

/// @brief Add an option to the current choice
/// @param line The option line (trimmed)
/// @param start The position of the '#'
static void cs_addChoiceOption(const char *line, int start, int indentation, int reusable, int disableReuse, int selectable)
{
  pg_game_status.choicesCount++;
  if (pg_game_status.choices == NULL) {
    pg_game_status.choices = malloc(sizeof(choice) * pg_game_status.choicesCount);
  } else {
    choice *tmp = realloc(pg_game_status.choices, sizeof(choice) * pg_game_status.choicesCount);
    if (tmp != NULL) {
      pg_game_status.choices = tmp;
      tmp = NULL;
    } else {
      logWarning("Realloc failed");
      pg_game_status.choicesCount--;
      return;
    }
  }

  choice *choice = &pg_game_status.choices[pg_game_status.choicesCount - 1];

  start++;
  choice->reusable = reusable;
  choice->disable_reuse = disableReuse;
  choice->selectable = selectable;
  choice->indentation = indentation;
  choice->text = malloc(strlen(line) - start + 1);
  choice->position = ftell(pg_game_status.fp);
  snprintf(choice->id, sizeof(choice->id), "%.40s_%li", pg_game_status.sceneName, choice->position);
  choice->used = 0;
  for (int i = 0; i < pg_game_status.used_choices_count; i++) {
    if (strcmp(choice->id, pg_game_status.used_choices[i]) == 0) {
      choice->used = 1;
      break;
    }
  }
  subString(line, choice->text, start, strlen(line) - start);
  char *expanded = cs_expandVariables(choice->text);
  if (expanded) {
    free(choice->text);
    choice->text = expanded;
  }
  logDebug("\tChoice: %s (pos=%li, reusable=%i, selectable=%i)", choice->text, choice->position, choice->reusable, choice->selectable);
}

/// @brief The option lines found while parsing the current choice
static long cs_parsedOptionLines[256];
static int cs_parsedOptionLinesCount = 0;

static unsigned int cs_sceneHash(const char *sceneName)
{
  unsigned int hash = 5381;
  for (const char *c = sceneName; *c; c++)
    hash = hash * 33 + (unsigned char)tolower((unsigned char)*c);
  return hash;
}

static void cs_addChoiceEnd(long position, long endPosition)
{
  unsigned int hash = cs_sceneHash(pg_game_status.sceneName);
  for (int i = 0; i < pg_game_status.choiceEndsCount; i++) {
    choice_end *ce = &pg_game_status.choiceEnds[i];
    if (ce->sceneHash == hash && ce->position == position) {
      ce->endPosition = endPosition;
      return;
    }
  }
  int max = sizeof(pg_game_status.choiceEnds) / sizeof(choice_end);
  if (pg_game_status.choiceEndsCount == max) {
    // Forget the oldest
    memmove(&pg_game_status.choiceEnds[0], &pg_game_status.choiceEnds[1], sizeof(choice_end) * (max - 1));
    pg_game_status.choiceEndsCount--;
  }
  choice_end *ce = &pg_game_status.choiceEnds[pg_game_status.choiceEndsCount++];
  ce->sceneHash = hash;
  ce->position = position;
  ce->endPosition = endPosition;
}

long cs_findChoiceEnd(long position)
{
  unsigned int hash = cs_sceneHash(pg_game_status.sceneName);
  for (int i = 0; i < pg_game_status.choiceEndsCount; i++) {
    if (pg_game_status.choiceEnds[i].sceneHash == hash && pg_game_status.choiceEnds[i].position == position)
      return pg_game_status.choiceEnds[i].endPosition;
  }
  return -1;
}

static void cs_parseChoiceBlock(int blockIndentation);

/// @brief Parse the options inside an *if/*elseif/*else of a choice
/// @param parentIndentation The indentation of the *if line
static void cs_parseChoiceNestedBlock(int parentIndentation)
{
  char line[4096];
  long bodyStart = ftell(pg_game_status.fp);
  long lineStart;
  int nestedIndentation = -1;
  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;
    nestedIndentation = getIndentation(line);
    break;
  }
  fseek(pg_game_status.fp, bodyStart, SEEK_SET);

  if (nestedIndentation > parentIndentation)
    cs_parseChoiceBlock(nestedIndentation);
}

/// @brief Parse the options of a choice at the given indentation.
/// Lines with a greater indentation are option bodies, a line with a lower indentation ends the block.
/// On return the file is positioned at the start of the line ending the block.
static void cs_parseChoiceBlock(int blockIndentation)
{
  char line[4096];
  char command[256];
  long lineStart;
  // 0 = not in an *if chain, 1 = a branch was already used, 2 = no branch used yet
  int ifChain = 0;

  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;

    int lineIndentation = getIndentation(line);
    if (lineIndentation < blockIndentation)
      break;
    if (lineIndentation > blockIndentation)
      continue; // Option body

    trim(line);
    int lineLen = strlen(line);
    command[0] = '\0';
    if (line[0] == '*')
      cs_getCommand(line, command);

    // Option line: reaching it at the end of an option body ends the choice
    if (stricmp(command, "label") != 0 && cs_parsedOptionLinesCount < (int)(sizeof(cs_parsedOptionLines) / sizeof(long)))
      cs_parsedOptionLines[cs_parsedOptionLinesCount++] = lineStart;

    if (cs_isElseCommand(command)) {
      if (ifChain == 2) {
        int res = 1;
        if (stricmp(command, "else") != 0) {
          char condition[4096];
          cs_getCommandArgs(line, condition, sizeof(condition));
          res = cs_evaluateCondition(condition, blockIndentation);
        }
        logDebug("\t%s -> %s", line, res ? "true" : "false");
        if (res) {
          ifChain = 1;
          cs_parseChoiceNestedBlock(blockIndentation);
        }
      } else if (ifChain == 0) {
        logWarning("*%s without *if in choice", command);
      }
      continue;
    }
    ifChain = 0;

    // Option modifiers: *hide_reuse, *disable_reuse, *allow_reuse, *selectable_if (cond), *if (cond)
    int start = 0;
    // Default of the scene, an option modifier overrides it
    int reusable = pg_game_status.choiceReuse == reuse_hide ? 0 : 1;
    int disableReuse = pg_game_status.choiceReuse == reuse_disable ? 1 : 0;
    int selectable = 1;
    int visible = 1;
    int isBlockIf = 0;
    int unsupported = 0;
    while (start < lineLen && line[start] == '*') {
      cs_getCommand(line + start, command);
      int next = start + 1 + strlen(command);
      while (next < lineLen && isspace((unsigned char)line[next]))
        next++;

      if (stricmp(command, "hide_reuse") == 0) {
        reusable = 0;
        disableReuse = 0;
      } else if (stricmp(command, "disable_reuse") == 0) {
        reusable = 1;
        disableReuse = 1;
      } else if (stricmp(command, "allow_reuse") == 0) {
        reusable = 1;
        disableReuse = 0;
      } else if (stricmp(command, "selectable_if") == 0 || stricmp(command, "if") == 0) {
        // Inline condition: *if (cond) #option, the condition must be enclosed in parentheses
        int condEnd = line[next] == '(' ? cs_findClosing(line, next) : -1;
        if (condEnd < 0 || indexOf(line, '#', condEnd) < 0) {
          if (stricmp(command, "if") == 0 && start == 0)
            isBlockIf = 1;
          else
            unsupported = 1;
          break;
        }

        char *condition = cs_strndup(line + next, condEnd - next + 1);
        int res = cs_evaluateCondition(condition, blockIndentation);
        logDebug("\t*%s %s -> %s", command, condition, res ? "true" : "false");
        free(condition);
        if (stricmp(command, "if") == 0) {
          visible = visible && res;
        } else {
          selectable = selectable && res;
        }

        next = condEnd + 1;
        while (next < lineLen && isspace((unsigned char)line[next]))
          next++;
      } else {
        unsupported = 1;
        break;
      }
      start = next;
    }

    if (isBlockIf) {
      // *if block containing options
      char condition[4096];
      cs_getCommandArgs(line, condition, sizeof(condition));
      int res = cs_evaluateCondition(condition, blockIndentation);
      logDebug("\t%s -> %s", line, res ? "true" : "false");
      if (res) {
        ifChain = 1;
        cs_parseChoiceNestedBlock(blockIndentation);
      } else {
        ifChain = 2;
      }
      continue;
    }

    if (unsupported) {
      logWarning("Unsupported command in choice: %s", command);
      continue;
    }

    if (start < lineLen && line[start] == '#') {
      if (!visible) {
        logDebug("\tChoice hidden: %s", line);
        continue;
      }
      cs_addChoiceOption(line, start, blockIndentation, reusable, disableReuse, selectable);
    }
  }

  fseek(pg_game_status.fp, lineStart, SEEK_SET);
}

long cs_manageChoiceCommand()
{
  char line[4096];
  long lineStart;
  long start = ftell(pg_game_status.fp);
  int choiceIndentation = -1;

  pg_game_status.choicesCount = 0;
  cs_parsedOptionLinesCount = 0;
  // The options indentation is the indentation of the first line
  while (cs_readLine(line, sizeof(line), &lineStart)) {
    if (cs_isBlank(line) || isComment(line))
      continue;
    choiceIndentation = getIndentation(line);
    break;
  }
  fseek(pg_game_status.fp, start, SEEK_SET);

  if (choiceIndentation >= 0)
    cs_parseChoiceBlock(choiceIndentation);

  if (pg_game_status.choicesCount == 0) {
    logWarning("Empty choice");
  }
  pg_game_status.choiceEndPosition = ftell(pg_game_status.fp);
  for (int i = 0; i < cs_parsedOptionLinesCount; i++)
    cs_addChoiceEnd(cs_parsedOptionLines[i], pg_game_status.choiceEndPosition);
  return pg_game_status.choiceEndPosition;
}

void cs_manageFinishCommand(const char *text)
{
  int lineLen = strlen(text);
  if (lineLen > 7) {
    if (lineLen - 8 > sizeof(pg_game_status.pageBreakMessage)) {
      logWarning("Finish message too long");
      strcpy(pg_game_status.pageBreakMessage, "Next");
    } else {
      subString(text, pg_game_status.pageBreakMessage, 8, lineLen - 8);
    }
  } else {
    strcpy(pg_game_status.pageBreakMessage, "Next Chapter");
  }
}

void cs_manageCheckPurchaseCommand(const char *text)
{
  int start = 16;
  int end;
  while ((end = indexOfWordBreak(text, start)) >= 0) {
    char word[256];
    subString(text, word, start, end - start);
    start = end + 1;

    char var_name[300];
    sprintf(var_name, "choice_purchased_%s", word);
    // Always se the var value to "true"
    addGameVar(runningGame, boolean, var_name, "true");
    logDebug("check_purchase %s=%s", var_name, "true");
  };
  // All the products checked are purchased
  addGameVar(runningGame, boolean, "choice_purchased_everything", "true");
}

label* cs_get_label(const char *labelName)
{
  if (pg_game_status.labelsCount < 0)
    cs_read_labels();

  int i = 0;
  while (i < pg_game_status.labelsCount) {
    if (stricmp(pg_game_status.labels[i].name, labelName) == 0)
      return &pg_game_status.labels[i];
    i++;
  }
  return NULL;
}

variable* cs_getVar(const char *varName)
{
  int i = 0;
  for (i = 0; i < pg_game_status.tempVariablesCount; i++) {
    if (stricmp(pg_game_status.tempVariables[i].name, varName) == 0) {
      return &pg_game_status.tempVariables[i];
    }
  }

  // Same as ChoiceScript: choice_saved_checkpoint[_slot] is false until the checkpoint is saved
  if (startsWith("choice_saved_checkpoint", varName)) {
    variable *var = getGameVarPrimitive(varName, runningGame);
    if (var != NULL)
      return var;
    static char falseName[] = "choice_saved_checkpoint";
    static char falseValue[] = "false";
    static variable falseVar = { falseName, falseValue, boolean };
    return &falseVar;
  }

  // Same as ChoiceScript on a device: not the web or Steam version, no purchases, ads or prerelease
  static const char *falseSystemVars[] = {
    "choice_is_web", "choice_is_steam", "choice_is_ios_app", "choice_purchase_supported",
    "choice_prerelease", "choice_is_advertising_supported", NULL
  };
  for (int i = 0; falseSystemVars[i] != NULL; i++) {
    if (stricmp(varName, falseSystemVars[i]) == 0) {
      variable *var = getGameVarPrimitive(varName, runningGame);
      if (var != NULL)
        return var;
      static char systemValue[] = "false";
      static variable systemVar = { NULL, systemValue, boolean };
      systemVar.name = (char *)falseSystemVars[i];
      strcpy(systemValue, "false");
      return &systemVar;
    }
  }

  return getGameVar(varName, runningGame);
}

/// @brief Format a number as ChoiceScript does (no decimals for integer values)
static void cs_formatNumber(double number, char *value)
{
  if (fabs(number - round(number)) < 1e-9)
    sprintf(value, "%li", (long)round(number));
  else
    sprintf(value, "%g", number);
}

static int cs_manageInputTextCommand(const char *line)
{
  char args[1024];
  cs_getCommandArgs(line, args, sizeof(args));

  char name[256] = "";
  int allowBlank = 0;
  int isLong = 0;
  int count = 0;
  char *token = strtok(args, " \t");
  while (token != NULL) {
    if (count == 0) {
      // The variable can be a reference: *input_text {name}
      int len = strlen(token);
      if (len > 1 && token[0] == '{' && token[len - 1] == '}') {
        char *content = cs_strndup(token + 1, len - 2);
        cs_evaluateReferenceName(content, name, sizeof(name));
        free(content);
      } else {
        cs_copyString(name, sizeof(name), token);
      }
    } else if (stricmp(token, "allow_blank") == 0) {
      allowBlank = 1;
    } else if (stricmp(token, "long") == 0) {
      isLong = 1;
    } else {
      logWarning("*input_text: unknown option %s", token);
    }
    count++;
    token = strtok(NULL, " \t");
  }

  if (strlen(name) == 0) {
    logWarning("*input_text: missing variable (%s)", line);
    return 0;
  }
  if (cs_getVar(name) == NULL) {
    logWarning("*input_text: non-existent variable %s", name);
    return 0;
  }

  cs_copyString(pg_game_status.inputTextVariable, sizeof(pg_game_status.inputTextVariable), name);
  pg_game_status.inputTextAllowBlank = allowBlank;
  pg_game_status.inputTextLong = isLong;
  pg_game_status.inputNumber = 0;
  pg_game_status.inputTextActive = 1;
  pg_game_status.inputTextOpened = 0;
  return 1;
}

/// @brief Check if a string is an integer as ChoiceScript does (no leading zeros, no "-0")
static int cs_isIntString(const char *text)
{
  const char *p = text;
  if (*p == '-')
    p++;
  if (!isdigit((unsigned char)*p))
    return 0;
  if (*p == '0' && (p[1] != '\0' || p != text))
    return 0;
  for (; *p; p++) {
    if (!isdigit((unsigned char)*p))
      return 0;
  }
  return 1;
}

static int cs_manageInputNumberCommand(const char *line)
{
  char args[4096];
  char *tokens[8];
  cs_getCommandArgs(line, args, sizeof(args));
  int count = cs_tokenizeArgs(args, tokens, 8);
  if (count != 3) {
    logWarning("Invalid input_number statement, expected three args: varname min max (%s)", line);
    cs_freeTokens(tokens, count);
    return 0;
  }

  // The variable can be a reference: *input_number {name} 1 10
  char name[256];
  int nameLen = strlen(tokens[0]);
  if (nameLen > 1 && tokens[0][0] == '{' && tokens[0][nameLen - 1] == '}') {
    char *content = cs_strndup(tokens[0] + 1, nameLen - 2);
    cs_evaluateReferenceName(content, name, sizeof(name));
    free(content);
  } else {
    cs_copyString(name, sizeof(name), tokens[0]);
  }

  char minValue[1024];
  char maxValue[1024];
  cs_evaluateExpression(tokens[1], minValue);
  cs_evaluateExpression(tokens[2], maxValue);
  trim(minValue);
  trim(maxValue);
  cs_freeTokens(tokens, count);

  if (cs_getVar(name) == NULL) {
    logWarning("*input_number: non-existent variable %s", name);
    return 0;
  }
  char *end1, *end2;
  double minimum = strtod(minValue, &end1);
  double maximum = strtod(maxValue, &end2);
  if (end1 == minValue || end2 == maxValue || *end1 || *end2) {
    logWarning("*input_number: min and max must be numbers (%s, %s)", minValue, maxValue);
    return 0;
  }
  if (minimum > maximum) {
    logWarning("*input_number: minimum %s should not be greater than maximum %s", minValue, maxValue);
    return 0;
  }

  cs_copyString(pg_game_status.inputTextVariable, sizeof(pg_game_status.inputTextVariable), name);
  pg_game_status.inputTextAllowBlank = 0;
  pg_game_status.inputTextLong = 0;
  pg_game_status.inputNumber = 1;
  pg_game_status.inputNumberMin = minimum;
  pg_game_status.inputNumberMax = maximum;
  // Same as ChoiceScript: integer limits require an integer number
  pg_game_status.inputNumberIntRequired = cs_isIntString(minValue) && cs_isIntString(maxValue);
  pg_game_status.inputTextActive = 1;
  pg_game_status.inputTextOpened = 0;
  return 1;
}

const char *cs_validateInputNumber(const char *text)
{
  static char message[128];
  char *end;
  double value = strtod(text, &end);
  if (*text == '\0' || end == text || *end != '\0')
    return "Please type in a number.";
  if (pg_game_status.inputNumberIntRequired && !cs_isIntString(text))
    return "Please type in an integer number.";

  char limit[64];
  if (value < pg_game_status.inputNumberMin) {
    cs_formatNumber(pg_game_status.inputNumberMin, limit);
    snprintf(message, sizeof(message), "Please use a number greater than or equal to %s", limit);
    return message;
  }
  if (value > pg_game_status.inputNumberMax) {
    cs_formatNumber(pg_game_status.inputNumberMax, limit);
    snprintf(message, sizeof(message), "Please use a number less than or equal to %s", limit);
    return message;
  }
  return NULL;
}

void cs_setInputText(const char *text)
{
  pg_game_status.inputTextActive = 0;

  char *value = cs_strndup(text, strlen(text));
  trim(value);
  variable *var = cs_getVar(pg_game_status.inputTextVariable);
  if (var != NULL && pg_game_status.inputNumber) {
    char numberValue[64];
    cs_formatNumber(strtod(value, NULL), numberValue);
    cs_assignVar(var, numberValue);
    var->type = number;
  } else if (var != NULL) {
    cs_assignVar(var, value);
    var->type = string;
  } else {
    logWarning("*input_text: non-existent variable %s", pg_game_status.inputTextVariable);
  }
  free(value);
}

/// @brief Assign an already evaluated value to a variable
static void cs_assignVar(variable *var, const char *value)
{
  char *tmp = cs_strndup(value, strlen(value));
  free(var->value);
  var->value = tmp;
  logDebug("Variable set %s=%s", var->name, var->value);
}

int cs_setVarValue(const char *varName, const char *value)
{
  char refName[1024];
  int nameLen = strlen(varName);
  if (nameLen > 1 && varName[0] == '{' && varName[nameLen - 1] == '}') {
    // *set {expr} value: the variable name is the value of expr
    char *content = cs_strndup(varName + 1, nameLen - 2);
    cs_evaluateReferenceName(content, refName, sizeof(refName));
    free(content);
    varName = refName;
  }

  variable *var = cs_getVar(varName);
  if (var == NULL)
    return 0;

  char expression[4096];
  snprintf(expression, sizeof(expression), "%s", value);
  trim(expression);
  int len = strlen(expression);
  char evaluated[4096];

  // Fairmath: *set var %+10, *set var %-10
  if (len > 2 && expression[0] == '%' && (expression[1] == '+' || expression[1] == '-')) {
    char *rest = expression + 2;
    double current = atof(var->value ? var->value : "0");
    double percent;
    char rightValue[1024];
    cs_evaluateExpression(rest, rightValue);
    percent = atof(rightValue);
    // Same as ChoiceScript (fairAdd): increase rounded down (max 99), decrease rounded up (min 1)
    double delta = expression[1] == '+' ? percent : -percent;
    double result;
    if (delta > 0) {
      result = floor(current + delta * (100 - current) / 100);
      if (result > 99)
        result = 99;
    } else {
      result = ceil(current - (-delta) * current / 100);
      if (result < 1)
        result = 1;
    }
    cs_formatNumber(result, evaluated);
    cs_assignVar(var, evaluated);
    return 1;
  }

  // Relative change: *set var +1, *set var - 2, *set var *(a + b), *set var &"text"
  if (len > 0 && strchr("+-*/&", expression[0]) != NULL) {
    char op = expression[0];
    char *rest = expression + 1;
    while (isspace((unsigned char)*rest))
      rest++;

    if (op == '&') {
      // String concatenation
      char rightValue[4096];
      cs_evaluateExpression(rest, rightValue);
      snprintf(evaluated, sizeof(evaluated), "%s%s", var->value ? var->value : "", rightValue);
      cs_assignVar(var, evaluated);
      return 1;
    }

    char rightValue[1024];
    cs_evaluateExpression(rest, rightValue);
    char *end;
    double right = strtod(rightValue, &end);
    if (end == rightValue) {
      logWarning("*set %s %s: invalid number %s", varName, value, rightValue);
      return 0;
    }
    double current = atof(var->value ? var->value : "0");
    double result;
    switch (op) {
      case '+': result = current + right; break;
      case '-': result = current - right; break;
      case '*': result = current * right; break;
      default:
        if (right == 0) {
          logWarning("*set %s %s: division by zero", varName, value);
          return 0;
        }
        result = current / right;
        break;
    }
    cs_formatNumber(result, evaluated);
    cs_assignVar(var, evaluated);
    return 1;
  }

  // Absolute value. Variables are dynamically typed (as in ChoiceScript): the type follows the value
  char opr[3];
  int isBooleanExpression = expression[0] != '"' &&
                            (cs_get_operator(expression, opr) >= 0 ||
                             cs_strnicmp(expression, "not", 3) == 0 ||
                             strstr(expression, " and ") != NULL || strstr(expression, " or ") != NULL ||
                             strstr(expression, ")and") != NULL || strstr(expression, ")or") != NULL);
  if (isBooleanExpression) {
    // Boolean expression: *set var (a > b), *set var not(a), *set var a and b
    cs_assignVar(var, cs_evaluateBoolean(expression) ? "true" : "false");
    var->type = boolean;
    return 1;
  }

  if (!cs_evaluateExpression(expression, evaluated)) {
    logWarning("*set %s %s: value not evaluated", varName, value);
    return 0;
  }
  cs_assignVar(var, evaluated);

  int intValue;
  if (expression[0] == '"')
    var->type = string;
  else if (stricmp(evaluated, "true") == 0 || stricmp(evaluated, "false") == 0)
    var->type = boolean;
  else if (strlen(evaluated) > 0 && is_number(evaluated, &intValue))
    var->type = number;
  else
    var->type = string;
  return 1;
}

variable* cs_add_temp_var()
{
  if (pg_game_status.tempVariables == NULL) {
    pg_game_status.tempVariables = malloc(sizeof(variable) * (pg_game_status.tempVariablesCount + 1));
  } else {
    variable *tmp = realloc(pg_game_status.tempVariables, sizeof(variable) * (pg_game_status.tempVariablesCount + 1));
    if (tmp != NULL) {
      pg_game_status.tempVariables = tmp;
      tmp = NULL;
    } else {
      logWarning("Realloc failed");
      return NULL;
    }
  }

  variable *res = &pg_game_status.tempVariables[pg_game_status.tempVariablesCount++];
  res->name = NULL;
  res->value = NULL;
  return res;
}

int cs_delete_temp_var(int index)
{
  if (index < 0 || index >= pg_game_status.tempVariablesCount)
    return 0;

  variable *var = &pg_game_status.tempVariables[index];
  free(var->name);
  var->name = NULL;
  free(var->value);
  var->value =  NULL;

  if (pg_game_status.tempVariablesCount == 1) {
    free(pg_game_status.tempVariables);
    pg_game_status.tempVariables = NULL;
    pg_game_status.tempVariablesCount = 0;

    return 1;
  }

  for (int i = index; i < pg_game_status.tempVariablesCount - 1; i++) {
    pg_game_status.tempVariables[i] = pg_game_status.tempVariables[i + 1];
  }
  pg_game_status.tempVariablesCount--;

  variable *tmp = realloc(pg_game_status.tempVariables, sizeof(variable) * (pg_game_status.tempVariablesCount));
  if (tmp != NULL) {
    pg_game_status.tempVariables = tmp;
    tmp = NULL;
  } else {
    logWarning("Realloc failed");
    return 0;
  }

  return 1;
}

void cs_clearTempVars()
{
  if (pg_game_status.tempVariables != NULL) {
    for (int i = 0; i < pg_game_status.tempVariablesCount; i++) {
      free(pg_game_status.tempVariables[i].name);
      pg_game_status.tempVariables[i].name = NULL;
      free(pg_game_status.tempVariables[i].value);
      pg_game_status.tempVariables[i].value = NULL;
    }
    free(pg_game_status.tempVariables);
    pg_game_status.tempVariables = NULL;
  }
  pg_game_status.tempVariablesCount = 0;
}

void cs_clearChoiceContexts(int gosubCount)
{
  while (pg_game_status.choiceContextsCount > 0 &&
         pg_game_status.choiceContexts[pg_game_status.choiceContextsCount - 1].gosubCount >= gosubCount) {
    pg_game_status.choiceContextsCount--;
  }
}

void cs_clearChoices()
{
  if (pg_game_status.choices != NULL) {
    for (int i = 0; i < pg_game_status.choicesCount; i++) {
      free(pg_game_status.choices[i].text);
      pg_game_status.choices[i].text = NULL;
    }
    free(pg_game_status.choices);
    pg_game_status.choices = NULL;
  }
  pg_game_status.choicesCount = 0;
}

void cs_clear_labels()
{
  if (pg_game_status.labels == NULL) {
    pg_game_status.labelsCount = -1;
    return;
  }

  for (int i = 0; i < pg_game_status.labelsCount; i++) {
    label* lbl = &pg_game_status.labels[i];
    free(lbl->name);
  }

  free(pg_game_status.labels);
  pg_game_status.labels = NULL;
  pg_game_status.labelsCount = -1;
}

void cs_read_labels()
{
  logDebug("Reading all labels");
  long pos = ftell(pg_game_status.fp);
  fseek(pg_game_status.fp, 0, SEEK_SET);
  pg_game_status.labelsCount = 0;
  char line[4096];
  int lineStartPosition = 0;
  long nextLineStartPosition = 0;
  while (fgets(line, sizeof(line), pg_game_status.fp)) {
    lineStartPosition = nextLineStartPosition;
    nextLineStartPosition = ftell(pg_game_status.fp);

    trim(line);
    if (startsWithWord("*label", line)) {
      pg_game_status.labelsCount++;
      if (pg_game_status.labels == NULL) {
        pg_game_status.labels = malloc(sizeof(label) * pg_game_status.labelsCount);
      } else {
        label *tmp = realloc(pg_game_status.labels, sizeof(label) * pg_game_status.labelsCount);
        if (tmp != NULL) {
          pg_game_status.labels = tmp;
          tmp = NULL;
        } else {
          logWarning("Realloc failed");
          break;
        }
      }

      pg_game_status.labels[pg_game_status.labelsCount - 1].name = malloc(strlen(line) - 7 + 1);
      subString(line, pg_game_status.labels[pg_game_status.labelsCount - 1].name, 7, strlen(line) - 7);
      pg_game_status.labels[pg_game_status.labelsCount - 1].position = lineStartPosition;

      logDebug("\tLabel name=%s, position=%li", pg_game_status.labels[pg_game_status.labelsCount - 1].name, pg_game_status.labels[pg_game_status.labelsCount - 1].position);
    }
  }
  fseek(pg_game_status.fp, pos, SEEK_SET);
}

/// @brief Copy a string, truncated to the buffer size
void cs_copyString(char *dst, size_t size, const char *src)
{
  size_t len = strlen(src);
  if (len >= size)
    len = size - 1;
  memcpy(dst, src, len);
  dst[len] = '\0';
}
