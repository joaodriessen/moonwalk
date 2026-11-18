#pragma once
#include <QThread>
#include <atomic>
#include <cassert>
struct SDL_Window {};
struct SDL_Rect { int x, y, w, h; };
struct SDL_DisplayMode { int w, h, refresh_rate; };
constexpr int SDL_INIT_VIDEO = 1, SDL_LOG_CATEGORY_APPLICATION = 0;
#define SDL_assert assert
struct ProbeFixture {
    inline static bool initFails = false, windowFails = false;
    inline static bool displayPresent = false;
    inline static int currentRefreshRate = 60;
    inline static std::atomic<bool> hold{false}, entered{false}, usable{true}, returned{false};
    inline static std::atomic<bool> holdThreadExit{false}, threadExitEntered{false}, threadExitReturned{true};
    inline static int destroyed = 0, references = 0;
    inline static bool destroyedBeforeReturn = false;
    static void reset() { displayPresent = false; currentRefreshRate = 60; initFails = windowFails = false; hold = entered = returned = false; usable = true; holdThreadExit = threadExitEntered = false; threadExitReturned = true; destroyed = references = 0; destroyedBeforeReturn = false; }
};
inline int SDL_InitSubSystem(int) { if (ProbeFixture::initFails) return -1; ++ProbeFixture::references; return 0; }
inline void SDL_QuitSubSystem(int) { --ProbeFixture::references; }
inline void SDL_DestroyWindow(SDL_Window* window) { ++ProbeFixture::destroyed; if (!ProbeFixture::returned || !ProbeFixture::threadExitReturned) ProbeFixture::destroyedBeforeReturn = true; delete window; }
inline const char* SDL_GetError() { return "fixture video failure"; }
inline void SDL_LogError(int, const char*, ...) {}
inline void SDL_LogWarn(int, const char*, ...) {}
inline int SDL_GetNumVideoDisplays() { return ProbeFixture::displayPresent ? 1 : 0; }
inline const char* SDL_GetDisplayName(int) { return "fixture"; }
inline int SDL_GetDisplayBounds(int, SDL_Rect*) { return -1; }
inline int SDL_GetNumDisplayModes(int) { return ProbeFixture::displayPresent ? 1 : 0; }
inline int SDL_GetDisplayMode(int, int, SDL_DisplayMode* mode) { *mode = {3840,2160,144}; return 0; }
inline int SDL_GetCurrentDisplayMode(int, SDL_DisplayMode* mode) { *mode = {3840,2160,ProbeFixture::currentRefreshRate}; return 0; }
