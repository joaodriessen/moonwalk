# Architecture

Moonwalk uses Moonlight Qt's engine and a Linux-focused Qt Quick interface.
The core protocol library, `moonlight-common-c`, is a pinned submodule. See
[Upstream source](upstream.md) for the fork point and how to bring in fixes.

| Code | Responsibility |
| --- | --- |
| `app/gui/main.qml` | Launcher navigation, window effects, and switching into a stream |
| `app/gui/adaptive/` | Shared host and Preferences workflows, with GNOME and Plasma surfaces |
| `app/settings/` | Global defaults, per-host settings, and presets |
| `app/backend/` | Discovery, pairing, host state, and requests to the host |
| `app/streaming/` | Stream planning, SDL input and presentation, decoding, audio, and host extensions |
| `moonlight-common-c/moonlight-common-c/` | Common streaming protocol implementation |

Moonwalk adds clipboard sync, cursor shapes, file transfer, live bitrate, and
desktop session cleanup to the engine. Gate each extension on what the host
advertises and allows for the paired device. Keep engine patches small and
test the C++ path they change.

## Desktop surfaces

`HostPageRoute` and `SettingsPageRoute` choose the host and settings views.
GNOME and the generic desktop route share the Adwaita-style pages. GNOME can
draw its launcher chrome. On other generic desktops the compositor owns the
frame. Plasma uses its own pages when the normal desktop and style checks
pass, including a Kirigami import check. `STARLIGHT_ADAPTIVE_SHELL=0` selects
the old `PcView` and `SettingsView` route.

`HostPageContext` and `SettingsPageContext` own state and actions. Their
facades expose what each page can read and do. Shared presentation lives under
`app/gui/adaptive/settings/`:

- `HostPresentation.qml` describes host status, the next action, the planned
  stream, and per-host settings.
- `AppSettingsModel.qml` declares application preferences.
- `HostDialogState.qml` supplies shared dialog text and actions.

Declare a setting once with `SettingRow`. `GnomeSettingsList` and
`PlasmaSettingsForm` render it using their desktop conventions. Use
`sensitive: false` and explain why when a visible setting is unavailable.
The parity tests cover controls shared across both layouts.

`StarlightAdwaita` is a Qt Quick Controls style built from the GNOME
components. Plasma uses the KDE desktop style when active. The stream status,
controls, and file-transfer panels use Qt Quick Controls, so the selected
style applies there too. `AppearanceProvider` reads desktop settings such
as color scheme, accent, contrast, fonts, and reduced motion.

## Settings and host requests

`StreamingPreferences` stores app-wide options separately from connection
settings keyed by host UUID. Stream startup takes a snapshot for the selected
host. Manual settings, presets, and learned performance have different
lifetimes. See [per-host settings](design/2026-07-14-per-host-settings.md).

The client asks for a mode. The host decides what to capture, what it can
encode, and what the paired device may control. Acknowledging a request is
different from measuring the decoded picture on the client. See
[settings ownership](design/settings-ownership.md) for the boundaries.

## Streaming

`Session::initialize()` accepts a `QWindow*` for launcher geometry and state.
`Session::exec()` runs the SDL loop on the main thread and pumps Qt events
for the stream controls and other work. See the
[main-thread contract](design/stream-main-thread-contract.md).

Pairing, discovery, app lists, stream launch, and optional HTTP features share
the host backend. The raw clipboard and cursor reads have a framing check:
responses with bodies require identity encoding and a finite `Content-Length`.
Bodyless 204 and 304 responses are allowed. The `nvpairing` tests cover this
path. Other host requests use their own response handling.

## Packaging

The [Flatpak](../flatpak/com.starlight.Starlight.yaml) is the primary Linux
package. It builds the app with ahead-of-time QML and includes SDL3 through
SDL2 compatibility, a Moonlight FFmpeg fork, video libraries, the legacy
Intel VA-API driver, and the GNOME Wayland decoration plugin. The Qt runtime
comes from `org.kde.Platform`. The package includes `vainfo` for checking
the VA-API drivers from inside the same sandbox.

The manifest leaves `LIBVA_DRIVER_NAME` unset so libva can choose a driver.
`QML_DISABLE_DISK_CACHE=1` avoids stale interpreted QML cache entries across
updates. See the manifest comments for why these settings are present.

The tree also carries Moonlight Qt's Windows, macOS, and Steam Link build
files. Moonwalk's packaging and release checks target Linux.

[Development](development.md) · [Compatibility](compatibility.md)
