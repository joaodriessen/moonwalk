#pragma once

#include <QByteArray>
#include <QString>

struct PlasmaDesktopGateInputs {
    QByteArray adaptiveShellOverride;
    QString platformProfileOverride;
    QString activeSession;
    QByteArray qpaPlatform;
    bool runningWindowManager = false;
    bool normalGuiRequest = false;
    bool styleAvailable = false;
    bool desktopBuildSupported = false;
};

class PlasmaDesktopGate final
{
public:
    static bool isNormalGuiArgumentShape(int argumentCount);
    static bool isWindowedDesktopQpa(const QByteArray& qpaPlatform,
                                     bool runningWindowManager);
    static bool shouldActivate(const PlasmaDesktopGateInputs& inputs);
};
