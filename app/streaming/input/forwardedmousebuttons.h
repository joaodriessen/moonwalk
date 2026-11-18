#pragma once

#include <array>

// Only physical mouse buttons that reached the host are owned here. Gamepad and
// touch emulation retain their separate lifetimes and background-input policy.
class ForwardedMouseButtons
{
public:
    void pressed(int button) { if (valid(button)) m_Down[button] = true; }
    bool released(int button)
    {
        if (!valid(button) || !m_Down[button]) return false;
        m_Down[button] = false;
        return true;
    }
    template<typename Release> void releaseAll(Release release)
    {
        for (int button = 1; button < int(m_Down.size()); ++button) {
            if (released(button)) release(button);
        }
    }
private:
    static bool valid(int button) { return button > 0 && button < 6; }
    std::array<bool, 6> m_Down{};
};
