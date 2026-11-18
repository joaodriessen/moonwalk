#pragma once
#include "SDL_compat.h"
#include <QSize>
#include <QString>
class SdlInputHandler {
public:
    static QString getUnmappedGamepads() { return {}; }
};
struct ProbeThreadExit {
    ProbeThreadExit() { ProbeFixture::threadExitReturned = false; }
    ~ProbeThreadExit() {
        ProbeFixture::threadExitEntered = true;
        while (ProbeFixture::holdThreadExit) QThread::msleep(1);
        ProbeFixture::threadExitReturned = true;
    }
};
class Session {
public:
    static bool getDecoderInfo(SDL_Window*, bool& hardware, bool& fullscreen, bool& hdr, QSize& maximum, bool& modern) {
        static thread_local ProbeThreadExit exit;
        Q_UNUSED(exit);
        ProbeFixture::entered = true;
        while (ProbeFixture::hold) QThread::msleep(1);
        const bool usable = ProbeFixture::usable;
        hardware = usable; fullscreen = false; hdr = usable; maximum = usable ? QSize(3840,2160) : QSize(); modern = usable;
        ProbeFixture::returned = true;
        return usable;
    }
};
