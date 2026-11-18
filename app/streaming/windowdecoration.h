#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// Who draws the stream window's title bar on Wayland. The desktop's own frame
// is the only one that truly matches it, so Starlight only draws one where the
// compositor has none to offer.
namespace WindowDecoration {
struct Policy {
    bool preferClientSide = false;
    bool forceClientSide = false;
};

// These compositors accept server-side decorations and then draw no title bar.
// GNOME needs no entry: it offers no server-side mode, so SDL always uses libdecor.
inline Policy policyForDesktop(const QString& currentDesktop)
{
    const auto desktops = currentDesktop.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const auto& desktop : desktops) {
        for (const char* captionless : {"Hyprland", "river", "dwl", "niri"}) {
            if (desktop.compare(QLatin1String(captionless), Qt::CaseInsensitive) == 0) return {true, true};
        }
    }
    return {};
}

// The GTK header libdecor draws, restyled in the current libadwaita's metrics and
// colours because the Adwaita inside GTK 3 is a decade behind the desktop around it.
inline QByteArray headerStyleSheet(bool dark)
{
    const QByteArray ink = dark ? "#ffffff" : "rgba(0,0,6,.8)";
    const QByteArray faded = dark ? "rgba(255,255,255,.5)" : "rgba(0,0,6,.4)";
    const QByteArray tint = dark ? "255,255,255" : "0,0,6";
    return "headerbar.titlebar.default-decoration {"
           " min-height: 46px; padding: 0 7px; margin: 0; border: none; box-shadow: none;"
           " border-radius: 15px 15px 0 0; font-weight: bold;"
           " background: " + QByteArray(dark ? "#2e2e32" : "#ffffff") + "; color: " + ink + "; }\n"
           "headerbar.titlebar.default-decoration:backdrop {"
           " background: " + QByteArray(dark ? "#222226" : "#fafafb") + "; color: " + faded + "; }\n"
           "headerbar.titlebar.default-decoration button.titlebutton {"
           " min-width: 24px; min-height: 24px; padding: 0; margin: 0; border: none; border-radius: 100%;"
           " box-shadow: none; -gtk-icon-shadow: none;"
           " background: rgba(" + tint + ",.1); color: " + ink + "; }\n"
           "headerbar.titlebar.default-decoration button.titlebutton:hover { background: rgba(" + tint + ",.15); }\n"
           "headerbar.titlebar.default-decoration button.titlebutton:active { background: rgba(" + tint + ",.3); }\n"
           "headerbar.titlebar.default-decoration button.titlebutton:backdrop {"
           " background: rgba(" + tint + ",.05); color: " + faded + "; }\n";
}

// Applies the next time a header is drawn. Does nothing and returns false unless
// libdecor's GTK plugin is the one in use. Call on the thread creating the window.
bool styleClientSideHeader(bool dark);
}
