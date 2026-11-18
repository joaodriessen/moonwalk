#pragma once

#include <cstdint>
#include <mutex>

// Network/audio callbacks run on workers; only the SDL thread paints the overlay.
// Keep independent conditions here so clearing one never clears another.
class StreamStatus
{
public:
    enum class Network { None, Poor, Slow };
    enum class Notice { None, ClipboardEmpty, ClipboardTooLarge, ClipboardQueueFailed };
    struct Snapshot {
        Network network = Network::None;
        bool gamepadMouse = false;
        bool audioUnavailable = false;
        Notice notice = Notice::None;

        bool operator==(const Snapshot& other) const {
            return network == other.network && gamepadMouse == other.gamepadMouse &&
                    audioUnavailable == other.audioUnavailable && notice == other.notice;
        }
    };

    void setNetwork(Network value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Network = value;
    }
    void setGamepadMouse(bool value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_GamepadMouse = value;
    }
    void setAudioAvailable(bool available, uint32_t now) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_AudioAvailable && !available) m_AudioFailedAt = now;
        m_AudioAvailable = available;
    }
    void showClipboardNotice(Notice value, uint32_t now) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Notice = value;
        m_NoticeStartedAt = now;
    }
    Snapshot snapshot(uint32_t now, bool intentionallyMuted = false) const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return {m_Network, m_GamepadMouse,
                !intentionallyMuted && !m_AudioAvailable && uint32_t(now - m_AudioFailedAt) >= 500,
                uint32_t(now - m_NoticeStartedAt) < 4000 ? m_Notice : Notice::None};
    }

private:
    mutable std::mutex m_Mutex;
    Network m_Network = Network::None;
    bool m_GamepadMouse = false;
    bool m_AudioAvailable = true;
    uint32_t m_AudioFailedAt = 0;
    Notice m_Notice = Notice::None;
    uint32_t m_NoticeStartedAt = 0;
};
