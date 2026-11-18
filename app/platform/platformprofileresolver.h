#pragma once

#include <memory>

#include <QString>

#include "platformprofile.h"

struct ProfileInputs {
    QString overrideValue;
    QString activeSession;
    bool hasKdeQuickControlsStyle = false;
};
Q_DECLARE_METATYPE(ProfileInputs)

class PlatformProfileResolver final
{
public:
    static std::unique_ptr<PlatformProfile> resolve(const ProfileInputs& inputs);
};
