#pragma once

#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#ifdef Q_OS_LINUX
#include <QDBusVariant>
#endif

#include <atomic>
#include <cstdint>

#include "SDL_compat.h"
#include "backend/nvhttp.h"

class NvComputer;

// The host supplies only shape and hotspot. SDL continues to position the
// pointer locally, so mouse movement never waits for the network or video.
#define SDL_CODE_CURSOR_SHAPE 107

class CursorSync : public QThread
{
    Q_OBJECT
public:
    explicit CursorSync(NvComputer *computer, double displayScale, double coordinateScale = 0);
    ~CursorSync() override;

    void setFocused(bool focused);
    void setDisplayScale(double displayScale, double coordinateScale = 0);
    void stopAndWait();

    // SDL thread only. Takes ownership of an event payload, even on failure.
    void applyAndDeleteFrame(void *payload);
    void resetCursor();

protected:
    void run() override;

#ifdef Q_OS_LINUX
private slots:
    void cursorSettingChanged(const QString& nameSpace, const QString& key, const QDBusVariant& value);
#endif
private:
    void park(unsigned long milliseconds);

    NvComputer *m_Computer;
    std::atomic<double> m_VisibilityScale;
    std::atomic<double> m_AssetScale;
    std::atomic<double> m_LocalCursorExtent {0};
#ifdef Q_OS_LINUX
    bool m_EnvironmentCursorPreference = false;
    int m_LocalCursorSize = 24;
    QString m_LocalCursorTheme;
#endif
    NvHTTP::CancelToken m_Cancel;
    std::atomic<bool> m_StopRequested {false};
    std::atomic<bool> m_Focused {false};
    std::atomic<std::uint64_t> m_FocusGeneration {0};
    std::atomic<bool> m_EventPending {false};
    QMutex m_WakeMutex;
    QWaitCondition m_Wake;
    SDL_Cursor *m_CurrentCursor = nullptr; // SDL thread only
};
