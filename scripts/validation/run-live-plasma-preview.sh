#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 [--dry-run] BINARY ISOLATED_ROOT" >&2
}

dry_run=false
if [[ ${1:-} == --dry-run ]]; then
    dry_run=true
    shift
fi
(( $# == 2 )) || { usage; exit 2; }

binary=$(realpath "$1")
isolated_root=$(realpath -m "$2")
[[ -x $binary ]] || { echo "Preview binary is not executable: $binary" >&2; exit 2; }
[[ ${XDG_SESSION_TYPE:-} == wayland ]] || {
    echo "Live Plasma preview requires the active Wayland session" >&2
    exit 2
}
[[ -n ${WAYLAND_DISPLAY:-} ]] || {
    echo "WAYLAND_DISPLAY is not set" >&2
    exit 2
}

host_config=${STARLIGHT_HOST_XDG_CONFIG_HOME:-${XDG_CONFIG_HOME:-$HOME/.config}}
host_config=$(realpath "$host_config")
kdeglobals="$host_config/kdeglobals"
[[ -f $kdeglobals && ! -L $kdeglobals ]] || {
    echo "Host KDE appearance file is missing or indirect: $kdeglobals" >&2
    exit 2
}

config_root="$isolated_root/config"
cache_root="$isolated_root/cache"
data_root="$isolated_root/data"
install -d -m 0700 "$config_root" "$cache_root" "$data_root"
mount_target="$config_root/kdeglobals"
host_mirror="$config_root/.host-kde-config"
install -d -m 0700 "$host_mirror"
if [[ -e $mount_target && ! -L $mount_target ]]; then
    [[ -f $mount_target && ! -s $mount_target ]] || {
        echo "Refusing to replace KDE appearance target: $mount_target" >&2
        exit 2
    }
    rm -f -- "$mount_target"
fi
if [[ -L $mount_target ]]; then
    [[ $(readlink "$mount_target") == .host-kde-config/kdeglobals ]] || {
        echo "Unexpected KDE appearance link: $mount_target" >&2
        exit 2
    }
else
    ln -s .host-kde-config/kdeglobals "$mount_target"
fi

if $dry_run; then
    printf '%s\n' \
        "XDG_CONFIG_HOME=$config_root" \
        "XDG_CACHE_HOME=$cache_root" \
        "XDG_DATA_HOME=$data_root" \
        "KDEGLOBALS=$kdeglobals" \
        "KDEGLOBALS_READ_ONLY=1" \
        "KDEGLOBALS_LIVE_DIRECTORY_BIND=1" \
        "STARLIGHT_ADAPTIVE_SHELL=1" \
        "QT_QPA_PLATFORM=wayland" \
        "WAYLAND_DISPLAY=$WAYLAND_DISPLAY" \
        "XDG_CURRENT_DESKTOP=${XDG_CURRENT_DESKTOP:-}" \
        "XDG_SESSION_DESKTOP=${XDG_SESSION_DESKTOP:-}" \
        "XDG_SESSION_TYPE=$XDG_SESSION_TYPE"
    exit 0
fi

exec bwrap \
    --dev-bind / / \
    --ro-bind "$host_config" "$host_mirror" \
    --chdir "$(pwd -P)" \
    --setenv XDG_CONFIG_HOME "$config_root" \
    --setenv XDG_CACHE_HOME "$cache_root" \
    --setenv XDG_DATA_HOME "$data_root" \
    --setenv STARLIGHT_ADAPTIVE_SHELL 1 \
    --setenv QT_QPA_PLATFORM wayland \
    --setenv QML_DISABLE_DISK_CACHE 1 \
    "$binary"
