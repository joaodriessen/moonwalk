#!/usr/bin/env bash
set -euo pipefail

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
fixture_root=$(mktemp -d /tmp/starlight-live-launcher-test.XXXXXX)
cleanup() { rm -rf -- "$fixture_root"; }
trap cleanup EXIT

mkdir -p "$fixture_root/host-config" "$fixture_root/profile/config" \
         "$fixture_root/profile/cache" "$fixture_root/profile/data"
printf '[KDE]\nwidgetStyle=TestStyle\n' > "$fixture_root/host-config/kdeglobals"
touch "$fixture_root/starlight"
chmod +x "$fixture_root/starlight"

output=$(
    STARLIGHT_HOST_XDG_CONFIG_HOME="$fixture_root/host-config" \
    XDG_CURRENT_DESKTOP=KDE XDG_SESSION_DESKTOP=plasma \
    XDG_SESSION_TYPE=wayland WAYLAND_DISPLAY=wayland-test DISPLAY=:99 \
    DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/test-bus \
    "$source_dir/scripts/validation/run-live-plasma-preview.sh" \
        --dry-run "$fixture_root/starlight" "$fixture_root/profile"
)

grep -Fx "XDG_CONFIG_HOME=$fixture_root/profile/config" <<<"$output"
grep -Fx "XDG_CACHE_HOME=$fixture_root/profile/cache" <<<"$output"
grep -Fx "XDG_DATA_HOME=$fixture_root/profile/data" <<<"$output"
grep -Fx "KDEGLOBALS=$fixture_root/host-config/kdeglobals" <<<"$output"
grep -Fx 'KDEGLOBALS_READ_ONLY=1' <<<"$output"
grep -Fx 'KDEGLOBALS_LIVE_DIRECTORY_BIND=1' <<<"$output"
grep -Fx 'STARLIGHT_ADAPTIVE_SHELL=1' <<<"$output"
grep -Fx 'QT_QPA_PLATFORM=wayland' <<<"$output"
grep -Fx 'WAYLAND_DISPLAY=wayland-test' <<<"$output"
grep -Fx 'XDG_CURRENT_DESKTOP=KDE' <<<"$output"

[[ -L "$fixture_root/profile/config/kdeglobals" ]]
[[ $(readlink "$fixture_root/profile/config/kdeglobals") == .host-kde-config/kdeglobals ]]
[[ $(stat -c %a "$fixture_root/host-config/kdeglobals") == 644 ]]

rg -F -- '--ro-bind "$host_config" "$host_mirror"' \
    "$source_dir/scripts/validation/run-live-plasma-preview.sh"
