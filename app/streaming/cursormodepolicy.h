#pragma once

// Skylight negotiates whether it captures the host cursor at launch/resume.
// Input mode can change in-session; that capture contract cannot. In particular
// do not add a second local pointer to a stream already containing the host one.
class CursorModePolicy
{
public:
    static bool absoluteInput(bool automatic, bool savedAbsolute, bool desktop) {
        return automatic ? desktop : savedAbsolute;
    }
    CursorModePolicy(bool localCursor = false, bool absoluteAtLaunch = false,
                     bool skylight = false, int legacyOverride = 0) :
        m_LocalCursor(localCursor || legacyOverride > 0),
        m_ClientCursor(m_LocalCursor && absoluteAtLaunch), m_Skylight(skylight) {}

    bool clientSideCursor() const { return m_ClientCursor; }
    bool initiallyShowLocalCursor() const { return m_LocalCursor && mayToggleLocalCursor(); }
    bool mayToggleLocalCursor() const { return !m_Skylight || m_ClientCursor; }
    bool syncShapes(bool absolute) const { return m_Skylight && m_ClientCursor && absolute; }
    bool needsReconnect(bool absolute) const { return m_Skylight && m_LocalCursor && absolute && !m_ClientCursor; }

private:
    bool m_LocalCursor;
    bool m_ClientCursor;
    bool m_Skylight;
};
