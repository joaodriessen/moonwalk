#pragma once
#include <SDL.h>
#include <set>

// Physical shortcuts stay keys; printable text is emitted only by SDL_TEXTINPUT.
// Remember suppressed downs so a later modifier/mode change cannot leak their up.
class LocalTextInputPolicy {
public:
    void setEnabled(bool enabled) { m_Enabled = enabled; if (!enabled) m_Composing = false; }
    void setComposing(bool composing) { m_Composing = m_Enabled && composing; }
    bool enabled() const { return m_Enabled; }
    void focusLost() { m_Suppressed.clear(); m_Composing = false; }
    bool suppress(SDL_Scancode scan, SDL_Keycode key, unsigned modifiers, bool down) {
        if (!down) return m_Suppressed.erase(scan) != 0;
        if (!m_Enabled) return false;
        // The local IME owns editing, candidate navigation and commit/cancel
        // keys while a preedit exists. Never edit the remote document too.
        const bool compositionEdit = scan == SDL_SCANCODE_BACKSPACE || scan == SDL_SCANCODE_DELETE ||
                scan == SDL_SCANCODE_LEFT || scan == SDL_SCANCODE_RIGHT ||
                scan == SDL_SCANCODE_UP || scan == SDL_SCANCODE_DOWN ||
                scan == SDL_SCANCODE_HOME || scan == SDL_SCANCODE_END ||
                scan == SDL_SCANCODE_PAGEUP || scan == SDL_SCANCODE_PAGEDOWN ||
                scan == SDL_SCANCODE_RETURN || scan == SDL_SCANCODE_KP_ENTER ||
                scan == SDL_SCANCODE_ESCAPE || scan == SDL_SCANCODE_TAB;
        if (m_Composing && compositionEdit) {
            m_Suppressed.insert(scan);
            return true;
        }
        const bool shortcut = (modifiers & (KMOD_CTRL | KMOD_GUI | KMOD_ALT)) != 0;
        if (shortcut && !(modifiers & KMOD_MODE)) return false;
        const bool printable = (key >= 0x20 && key < SDLK_SCANCODE_MASK && key != 0x7f) ||
                (scan >= SDL_SCANCODE_A && scan <= SDL_SCANCODE_0) ||
                (scan >= SDL_SCANCODE_MINUS && scan <= SDL_SCANCODE_SLASH) ||
                scan == SDL_SCANCODE_NONUSBACKSLASH || scan == SDL_SCANCODE_SPACE;
        if (!printable) return false;
        m_Suppressed.insert(scan);
        return true;
    }
private:
    bool m_Enabled = false;
    bool m_Composing = false;
    std::set<SDL_Scancode> m_Suppressed;
};
