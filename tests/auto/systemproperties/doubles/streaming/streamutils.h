#pragma once
#include "SDL_compat.h"
class StreamUtils {
public:
    static SDL_Window* createTestWindow() { return ProbeFixture::windowFails ? nullptr : new SDL_Window; }
    static bool getNativeDesktopMode(int, SDL_DisplayMode* mode, SDL_Rect* safe) {
        if (!ProbeFixture::displayPresent) return false;
        *mode = {3840,2160,144}; *safe = {0,0,3840,2160}; return true;
    }
};
