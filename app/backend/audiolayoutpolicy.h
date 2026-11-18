#pragma once

#include <QString>

namespace AudioLayoutPolicy {
inline int parseChannelLimit(const QString &value)
{
    bool ok = false;
    const int channels = value.toInt(&ok);
    return ok && (channels == 2 || channels == 6 || channels == 8) ? channels : 0;
}

inline int effectiveChannels(int requested, int hostLimit)
{
    if (hostLimit == 2 && requested > 2) return 2;
    if (hostLimit == 6 && requested > 6) return 6;
    return requested; // An absent/unrecognized capability preserves older hosts.
}
}
