#pragma once

#include <QByteArray>

// SDL-thread-only guard for host clipboard responses that race a focus change.
// Hashes are of the complete local clipboard contents, including empty text.
class ClipboardFocusGuard
{
public:
    explicit ClipboardFocusGuard(const QByteArray &initialHash) :
        m_LastFocusedHash(initialHash) {}

    void focusedClipboardChanged(const QByteArray &hash)
    {
        m_LastFocusedHash = hash;
    }

    void focusLost(const QByteArray &currentHash)
    {
        m_FocusLossHash = currentHash;
        m_ChangedAtFocusEdge = currentHash != m_LastFocusedHash;
        m_Protect = true;
    }

    void focusGained(const QByteArray &currentHash, quintptr generation)
    {
        m_LastFocusedHash = currentHash;
        m_MinGeneration = generation;
        m_Protect = false;
    }

    bool mayApply(const QByteArray &currentHash, quintptr generation = 0) const
    {
        return generation >= m_MinGeneration &&
               (m_Protect ? (!m_ChangedAtFocusEdge && currentHash == m_FocusLossHash)
                          : currentHash == m_LastFocusedHash);
    }

    bool isProtecting() const { return m_Protect; }

    void applied(const QByteArray &hash)
    {
        m_LastFocusedHash = hash;
        if (m_Protect) {
            m_FocusLossHash = hash;
            m_ChangedAtFocusEdge = false;
        }
    }

private:
    quintptr m_MinGeneration = 0;
    QByteArray m_LastFocusedHash;
    QByteArray m_FocusLossHash;
    bool m_ChangedAtFocusEdge = false;
    bool m_Protect = false;
};
