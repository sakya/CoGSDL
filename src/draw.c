#include "CoGSDL.h"

void drawHeader();
void drawFooter();
void drawAppIcon();

#define TARGET_FPS 60

static int fps = 0;
static Uint64 fpsStart = 0;
static int fpsCount = 0;
static double header_angle = 0;

int draw()
{
  const Uint64 freq = SDL_GetPerformanceFrequency();
  const Uint64 frameTicks = freq / TARGET_FPS;
  // Absolute time of the next frame: a frame that waits too long is
  // compensated by the following one, so the average stays at TARGET_FPS
  Uint64 nextFrame = SDL_GetPerformanceCounter();
  fpsStart = nextFrame;
  fpsCount = 0;

  while (_Globals.quit == 0) {

    SDL_SetRenderDrawColor(_Globals.renderer, _theme->backgroundColor.r, _theme->backgroundColor.g, _theme->backgroundColor.b, 255);
    SDL_RenderClear(_Globals.renderer);

    drawHeader();
    drawFooter();
    switch (_Globals.status)
    {
      case MainMenu:
        drawMainMenu();
        break;
      case About:
        drawAbout();
        break;
      case GameSelect:
        drawGameSelectMenu();
        break;
      case Game:
        drawGame();
        break;
      case GameChoice:
        drawGameChoice();
        break;
      case GameMenu:
        drawGameMenu();
        break;
      case GameStats:
        drawGameStats();
        break;
      case Settings:
        drawSettings();
        break;
      case OnScreenKeyboard:
        drawOsk();
        break;
    }
    manageEvents();
    presentFrame();

    // Cap to TARGET_FPS
    nextFrame += frameTicks;
    Uint64 now = SDL_GetPerformanceCounter();
    if (now < nextFrame) {
      Uint32 toWait = (Uint32)((nextFrame - now) * 1000 / freq);
      if (toWait > 0)
        SDL_Delay(toWait);
    } else if (now - nextFrame > frameTicks) {
      // More than a frame late (e.g. loading a scene): don't try to catch up
      nextFrame = now;
    }

    // Fps: frames counted over (at least) one second, rounded
    fpsCount++;
    now = SDL_GetPerformanceCounter();
    if (now - fpsStart >= freq) {
      fps = (int)((fpsCount * freq + (now - fpsStart) / 2) / (now - fpsStart));
      fpsStart = now;
      fpsCount = 0;
    }
  }

  return 0;
}

void presentFrame()
{
  SDL_RenderPresent(_Globals.renderer);
  sdlCacheEndFrame();
#ifdef MIYOO
  // Copy the whole frame to the screen
  SDL_UpdateTexture(_Globals.screenTexture, NULL, _Globals.screenSurface->pixels, _Globals.screenSurface->pitch);
  SDL_RenderCopy(_Globals.screenRenderer, _Globals.screenTexture, NULL, NULL);
  SDL_RenderPresent(_Globals.screenRenderer);
#endif
}

void drawHeader()
{
  SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);

  SDL_Rect rect = { 0, 0, _Globals.windowWidth, HEADER_HEIGHT };
  SDL_RenderFillRect(_Globals.renderer, &rect);

  // Texts vertically centered in the header
  const int textY = sdlTextCenterY(_Globals.defaultFont, 0, HEADER_HEIGHT);

  if (runningGame != NULL && (_Globals.status == Game || _Globals.status == GameMenu || _Globals.status == GameStats || _Globals.status == GameChoice || _Globals.status == OnScreenKeyboard)) {
    if (runningGameIcon != NULL) {
      sdlBlitImage(runningGameIcon, 0, 0, HEADER_HEIGHT, HEADER_HEIGHT);
    } else {
      drawAppIcon();
    }
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, runningGame->title, 35, textY, 0, 0);
  } else if (_Globals.status != MainMenu && _Globals.status != About) {
    drawAppIcon();
    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, "CoGSDL v" VERSION, 35, textY, 0, 0);
  }

  if (_Globals.show_fps && fps > 0) {
    char fpsStr[20];
    int width;
    snprintf(fpsStr, sizeof(fpsStr), "Fps: %i", fps);
    TTF_MeasureUTF8(_Globals.defaultFont, fpsStr, _Globals.windowWidth, &width, NULL);

    sdlWriteText(_Globals.defaultFont, _theme->titleTextColor, fpsStr, _Globals.windowWidth - width - 2, textY, 0, 0);
  }
}

void drawFooter()
{
  SDL_SetRenderDrawColor(_Globals.renderer, _theme->headerBackgroundColor.r, _theme->headerBackgroundColor.g, _theme->headerBackgroundColor.b, 255);

  SDL_Rect rect = { 0, _Globals.windowHeight - HEADER_HEIGHT, _Globals.windowWidth, HEADER_HEIGHT };
  SDL_RenderFillRect(_Globals.renderer, &rect);
}

/* The rotating app icon in the header.
 * The icon is rotated here instead of using SDL_RenderCopyEx: the software renderer (Miyoo
 * Mini) places the rotated image at integer coordinates of a bounding box whose size changes
 * with the angle, so the center moves up to 2 pixels. Here each frame is computed around the
 * exact center (bilinear sampling, 2x2 supersampling) and drawn without rotation */

#define APP_ICON_SIZE 26
/// @brief Scale of the source image compared to the drawn icon (for the supersampling)
#define APP_ICON_SOURCE_SCALE 2

/// @brief The icon scaled to APP_ICON_SIZE * APP_ICON_SOURCE_SCALE (ARGB8888)
static SDL_Surface *appIconSource = NULL;
/// @brief The image used to build appIconSource
static SDL_Surface *appIconSourceOf = NULL;
/// @brief The rotated icon of the current frame (ARGB8888 pixels)
static Uint32 appIconPixels[APP_ICON_SIZE * APP_ICON_SIZE];
static SDL_Texture *appIconTexture = NULL;

/// @brief Prepare the source image and the texture of the icon
/// @return 0 if the icon cannot be drawn
static int appIconPrepare()
{
  if (app_icon == NULL)
    return 0;
  if (appIconSource != NULL && appIconSourceOf == app_icon && appIconTexture != NULL)
    return 1;

  SDL_FreeSurface(appIconSource);
  appIconSource = NULL;
  const int size = APP_ICON_SIZE * APP_ICON_SOURCE_SCALE;
  SDL_Surface *converted = SDL_ConvertSurfaceFormat(app_icon, SDL_PIXELFORMAT_ARGB8888, 0);
  if (converted != NULL) {
    appIconSource = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_ARGB8888);
    if (appIconSource != NULL && SDL_SoftStretchLinear(converted, NULL, appIconSource, NULL) < 0) {
      SDL_FreeSurface(appIconSource);
      appIconSource = NULL;
    }
    SDL_FreeSurface(converted);
  }
  appIconSourceOf = app_icon;
  if (appIconSource == NULL) {
    logWarning("Cannot prepare the app icon: %s", SDL_GetError());
    return 0;
  }

  if (appIconTexture == NULL) {
    appIconTexture = SDL_CreateTexture(_Globals.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, APP_ICON_SIZE, APP_ICON_SIZE);
    if (appIconTexture == NULL) {
      logWarning("Cannot create the app icon texture: %s", SDL_GetError());
      return 0;
    }
    SDL_SetTextureBlendMode(appIconTexture, SDL_BLENDMODE_BLEND);
  }
  return 1;
}

/// @brief Bilinear sample of appIconSource (premultiplied alpha), transparent outside the image
static void appIconSample(float u, float v, float *a, float *r, float *g, float *b)
{
  const int size = appIconSource->w;
  const int x0 = (int)floorf(u);
  const int y0 = (int)floorf(v);
  const float fx = u - x0;
  const float fy = v - y0;
  for (int j = 0; j < 2; j++) {
    const int y = y0 + j;
    if (y < 0 || y >= size)
      continue;
    const Uint32 *row = (const Uint32 *)((const Uint8 *)appIconSource->pixels + y * appIconSource->pitch);
    const float wy = j ? fy : 1.0f - fy;
    for (int i = 0; i < 2; i++) {
      const int x = x0 + i;
      if (x < 0 || x >= size)
        continue;
      const float w = wy * (i ? fx : 1.0f - fx);
      const Uint32 p = row[x];
      const float pa = (float)(p >> 24) * w;
      *a += pa;
      *r += (float)((p >> 16) & 0xFF) * pa;
      *g += (float)((p >> 8) & 0xFF) * pa;
      *b += (float)(p & 0xFF) * pa;
    }
  }
}

/// @brief Compute the icon rotated by angle degrees (clockwise) around its center
static void appIconRotate(double angle)
{
  const float rad = (float)(angle * M_PI / 180.0);
  const float c = cosf(rad);
  const float s = sinf(rad);
  const float half = APP_ICON_SIZE / 2.0f;
  const float scale = APP_ICON_SOURCE_SCALE;
  const float sub = 1.0f / APP_ICON_SOURCE_SCALE;

  for (int y = 0; y < APP_ICON_SIZE; y++) {
    for (int x = 0; x < APP_ICON_SIZE; x++) {
      float a = 0, r = 0, g = 0, b = 0;
      // APP_ICON_SOURCE_SCALE x APP_ICON_SOURCE_SCALE samples in the pixel
      for (int sy = 0; sy < APP_ICON_SOURCE_SCALE; sy++) {
        for (int sx = 0; sx < APP_ICON_SOURCE_SCALE; sx++) {
          const float dx = x + (sx + 0.5f) * sub - half;
          const float dy = y + (sy + 0.5f) * sub - half;
          // Inverse rotation: the point of the image drawn here
          const float ix = dx * c + dy * s + half;
          const float iy = -dx * s + dy * c + half;
          appIconSample(ix * scale - 0.5f, iy * scale - 0.5f, &a, &r, &g, &b);
        }
      }
      const float samples = APP_ICON_SOURCE_SCALE * APP_ICON_SOURCE_SCALE;
      Uint32 pixel = 0;
      if (a > 0) {
        const Uint32 pa = (Uint32)(a / samples + 0.5f);
        pixel = ((pa > 255 ? 255 : pa) << 24) |
                ((Uint32)(r / a + 0.5f) << 16) | ((Uint32)(g / a + 0.5f) << 8) | (Uint32)(b / a + 0.5f);
      }
      appIconPixels[y * APP_ICON_SIZE + x] = pixel;
    }
  }
}

void drawAppIcon()
{
  if (appIconPrepare()) {
    appIconRotate(header_angle);
    SDL_UpdateTexture(appIconTexture, NULL, appIconPixels, APP_ICON_SIZE * sizeof(Uint32));
    // Vertically centered in the header
    SDL_Rect rect = { 2, (HEADER_HEIGHT - APP_ICON_SIZE) / 2, APP_ICON_SIZE, APP_ICON_SIZE };
    SDL_RenderCopy(_Globals.renderer, appIconTexture, NULL, &rect);
  }

  header_angle += 0.5;
  if (header_angle >= 360)
    header_angle = 0;
}