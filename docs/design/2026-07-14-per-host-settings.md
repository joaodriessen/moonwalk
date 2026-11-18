# Per-host settings and migration

Moonlight Qt originally stored stream settings in one shared profile. Moonwalk
uses a profile for each host so that changing settings for one computer leaves
the others alone. Pairing and application preferences keep their existing
storage.

## What is stored

`StreamingPreferences` defines the connection keys in `k_PerHostKeys` in
`app/settings/streamingpreferences.cpp`. That table is used to load, save, and
migrate the per-host values. Read it when adding a setting.

- Connection values go under `hostsettings/<uuid>/`. They include the picture,
  codec, bitrate, display, and input choices. The UUID remains the key when
  the host is renamed or its address changes.
- App-wide settings such as language, launcher window mode, and automatic
  discovery use the main settings store.
- User presets, the last healthy settings, and learned performance are
  separate from the currently selected settings.

The global connection values become a fallback template. `reload()` captures
that template after loading and migrating the profile. On host selection,
`setCurrentHost()` overlays the stored values for that UUID and uses the
template for missing keys. It emits the change signals for the affected
controls. Using a snapshot as the fallback keeps values from the previous
host from leaking into the next one.

## Starting a stream

The launcher selects the host profile before constructing the session.
Each session owns a copy of its settings, so later launcher changes cannot
alter an in-flight request. An explicit CLI option is carried through host
selection and applied to that connection. A retry can use a temporary copy
with safe or last-working values. A user can explicitly keep the safe settings.

Host selection changes the shared preferences on the GUI thread. Keep that
thread boundary and the session snapshot if you add another launch path.

## Existing profiles

The profile's `starlightPrefsVersion` tracks these migrations independently
of upstream defaults:

| Version | Change |
| --- | --- |
| 1 | Reserves the original migration marker. The old forced VSync and pacing correction was removed so imported choices are honored. |
| 2 | Seeds missing connection keys for known host UUIDs. Uses the recovery host list when present and leaves existing per-host keys intact. |
| 3 | Enables adaptive bitrate for profiles already using automatic bitrate. A chosen manual bitrate is preserved. |
| 4 | Introduces Automatic and Manual stream modes, carrying over meaningful overrides and normalizing obsolete stream options. |
| 5 | Adds Automatic and Manual modes for display and input. Existing overrides determine the initial mode. |

A native Linux build can copy a Moonlight profile on first launch if no
Moonwalk profile exists. The import leaves the existing Moonlight file intact.
Flatpak uses its sandboxed configuration. See [Settings](../guides/settings.md)
for the current interface.
