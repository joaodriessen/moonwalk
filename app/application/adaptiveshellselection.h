#pragma once

#include <QByteArray>

class AdaptiveShellSelection
{
public:
    static AdaptiveShellSelection fromOverride(const QByteArray& value)
    {
        // The platform-native shell is the default; "0" is the explicit
        // legacy opt-out and anything unrecognized fails closed to legacy.
        if (value.isEmpty() || value == QByteArrayLiteral("1")) {
            return AdaptiveShellSelection(true, true);
        }

        if (value == QByteArrayLiteral("0")) {
            return AdaptiveShellSelection(true, false);
        }

        return AdaptiveShellSelection(false, false);
    }

    bool isValidOverride() const
    {
        return m_ValidOverride;
    }

    bool isEnabled() const
    {
        return m_Enabled;
    }

    bool shouldUseAdaptiveShell(bool eligible) const
    {
        return m_Enabled && eligible;
    }

private:
    AdaptiveShellSelection(bool validOverride, bool enabled)
        : m_ValidOverride(validOverride),
          m_Enabled(enabled)
    {
    }

    bool m_ValidOverride;
    bool m_Enabled;
};
