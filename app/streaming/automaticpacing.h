#pragma once

#include <QtGlobal>

// Whether to pace frames to the display, from how decoded frames actually arrive. Frames
// that come in bunches reach the desktop two to a refresh, which drops one and makes the
// picture skip; pacing holds each for its own refresh, at the cost of about one refresh of
// delay. So it is on only while frames are bunching: at least a tenth of them in two
// seconds landing within half a frame of the one before. It goes off after half a minute
// of steady arrival, and never changes twice within half a minute. A window that can't be
// seen tells nothing about the picture, so its frames don't count.
class AutomaticPacing {
public:
    static constexpr quint64 WindowUs = 2000000;
    static constexpr quint64 SteadyForUs = 30000000;
    static constexpr quint64 HoldUs = 30000000;

    explicit AutomaticPacing(int fps = 60, bool active = false)
        : m_IntervalUs(1000000 / quint64(qMax(fps, 1))), m_Active(active) {}

    bool active() const { return m_Active; }
    // How long frames were watched with the window visible.
    quint64 visibleUs() const { return m_VisibleUs; }

    // Returns true when the decision changed.
    bool observe(quint64 arrivalUs, bool visible)
    {
        if (!visible) {
            m_PreviousUs = m_WindowStartUs = m_SteadySinceUs = 0;
            m_Frames = m_Bunched = 0;
            return false;
        }
        if (m_PreviousUs && arrivalUs > m_PreviousUs) {
            const quint64 gapUs = arrivalUs - m_PreviousUs;
            ++m_Frames;
            if (gapUs * 2 < m_IntervalUs) ++m_Bunched;
            m_VisibleUs += qMin<quint64>(gapUs, 100000);
        }
        m_PreviousUs = arrivalUs;
        if (!m_WindowStartUs) m_WindowStartUs = arrivalUs;
        if (arrivalUs - m_WindowStartUs < WindowUs) return false;

        const bool enough = m_Frames >= 20;
        const bool bunching = enough && m_Bunched * 10 >= m_Frames;
        const bool steady = enough && m_Bunched * 20 < m_Frames;
        const quint64 windowStartUs = m_WindowStartUs;
        m_WindowStartUs = arrivalUs;
        m_Frames = m_Bunched = 0;

        const bool mayChange = !m_ChangedUs || arrivalUs - m_ChangedUs >= HoldUs;
        if (bunching) {
            m_SteadySinceUs = 0;
            if (!m_Active && mayChange) return change(true, arrivalUs);
        }
        else if (steady) {
            if (!m_SteadySinceUs) m_SteadySinceUs = windowStartUs;
            if (m_Active && mayChange && arrivalUs - m_SteadySinceUs >= SteadyForUs) return change(false, arrivalUs);
        }
        else if (enough) {
            m_SteadySinceUs = 0;
        }
        return false;
    }

private:
    bool change(bool active, quint64 nowUs)
    {
        m_Active = active;
        m_ChangedUs = nowUs;
        m_SteadySinceUs = 0;
        return true;
    }

    quint64 m_IntervalUs;
    bool m_Active;
    quint64 m_PreviousUs = 0;
    quint64 m_WindowStartUs = 0;
    quint64 m_SteadySinceUs = 0;
    quint64 m_ChangedUs = 0;
    quint64 m_VisibleUs = 0;
    quint64 m_Frames = 0;
    quint64 m_Bunched = 0;
};
