#pragma once

#include <QByteArray>
#include <QtGlobal>

class MaterialStyleBootstrap final
{
public:
    static void applyLegacyDefaults(bool materialStyleSelected)
    {
        if (!materialStyleSelected) {
            return;
        }

        // The legacy application contract always used the dark Material
        // baseline for Controls outside the main window's semantic palette.
        qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", "Dark");

        setDefault("QT_QUICK_CONTROLS_MATERIAL_ACCENT", "Purple");
        setDefault("QT_QUICK_CONTROLS_MATERIAL_VARIANT", "Dense");
        setDefault("QT_QUICK_CONTROLS_MATERIAL_PRIMARY", "#3F51B5");
    }

private:
    static void setDefault(const char* name, const QByteArray& value)
    {
        if (!qEnvironmentVariableIsSet(name)) {
            qputenv(name, value);
        }
    }
};
