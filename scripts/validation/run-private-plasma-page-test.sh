#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: run-private-plasma-page-test.sh [--source DIR] KDEGLOBALS_OR_FIXTURE [tst_name ...]

Run selected QML characterization files under a private KDE configuration and
a managed X11 session. With no selectors, tst_plasmaadapter is used.
EOF
}

source_dir=
while (($#)); do
    case $1 in
        --help|-h) usage; exit 0 ;;
        --source) source_dir=${2:?--source requires a directory}; shift 2 ;;
        --*) echo "unknown option: $1" >&2; exit 2 ;;
        *) break ;;
    esac
done
if command -v openbox >/dev/null 2>&1; then
    plasma_x11_wm=openbox
elif command -v kwin_x11 >/dev/null 2>&1; then
    plasma_x11_wm=kwin_x11
else
    echo 'managed X11 requires openbox or kwin_x11' >&2
    exit 1
fi
if [[ ${STARLIGHT_PLASMA_WM_POLICY_PROBE:-0} == 1 ]]; then
    printf 'plasma-x11-wm=%s\n' "$plasma_x11_wm"
    exit 0
fi
(( $# >= 1 )) || { usage >&2; exit 2; }
if [[ -z $source_dir ]]; then
    source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
else
    source_dir=$(cd "$source_dir" && pwd -P)
fi

fixture=$1
shift
[[ -f $fixture ]] || fixture="$source_dir/$fixture/kdeglobals"
[[ -f $fixture ]] || { echo "KDE globals fixture does not exist: $fixture" >&2; exit 2; }
if (($# == 0)); then
    set -- tst_plasmaadapter
fi
requires_widget_style=false
for selector in "$@"; do
    [[ $selector =~ ^tst_[A-Za-z0-9_]+$ && -f "$source_dir/tests/qml/$selector.qml" ]] || {
        echo "QML test does not exist: $selector" >&2; exit 2;
    }
    [[ $selector == tst_plasmapages ]] && requires_widget_style=true
done
fixture_widget_style=$(awk -F= '$1 == "widgetStyle" { print $2; exit }' "$fixture")
if $requires_widget_style && [[ -z $fixture_widget_style ]]; then
    echo "KDE globals fixture has no widgetStyle for tst_plasmapages: $fixture" >&2
    exit 2
fi

private_root=$(mktemp -d "${TMPDIR:-/tmp}/starlight-plasma.XXXXXX")
session_pid=
# shellcheck disable=SC2329 # Invoked by the EXIT/HUP/INT/TERM trap below.
cleanup() {
    local status=$? cleanup_status target fstype
    cleanup_status=$status
    local -a private_fuse_mounts=()
    trap - EXIT HUP INT TERM

    if [[ -n $session_pid ]]; then
        kill -TERM "$session_pid" 2>/dev/null || true
        wait "$session_pid" 2>/dev/null || true
        session_pid=
    fi

    while IFS=' ' read -r target fstype; do
        [[ $target == "$private_root/"* && $fstype == fuse* ]] || continue
        private_fuse_mounts+=("$target")
    done < <(findmnt -R -n -r -o TARGET,FSTYPE -T "$private_root")
    if ((${#private_fuse_mounts[@]})); then
        while IFS=$'\t' read -r _ target; do
            if fusermount3 -u "$target" 2>/dev/null; then
                continue
            fi
            if ! findmnt -n --mountpoint "$target" >/dev/null 2>&1; then
                continue
            fi
            if ! fusermount3 -u -z "$target"; then
                echo "unable to detach private FUSE mount: $target" >&2
                cleanup_status=1
            fi
        done < <(
            for target in "${private_fuse_mounts[@]}"; do
                printf '%08d\t%s\n' "${#target}" "$target"
            done | sort -r
        )
    fi

    if ! rm -rf -- "$private_root"; then
        cleanup_status=1
    fi
    [[ ! -e $private_root && ! -L $private_root ]] || {
        echo "private Plasma root survived teardown: $private_root" >&2
        cleanup_status=1
    }
    return "$cleanup_status"
}
trap cleanup EXIT HUP INT TERM
export XDG_CONFIG_HOME="$private_root/config"
export XDG_CACHE_HOME="$private_root/cache"
export XDG_DATA_HOME="$private_root/data"
export XDG_RUNTIME_DIR="$private_root/runtime"
export QML_DISABLE_DISK_CACHE=1
export QT_QPA_PLATFORM=xcb
export QT_QUICK_BACKEND=software
export QT_QUICK_CONTROLS_STYLE=org.kde.desktop
if [[ -n $fixture_widget_style ]]; then
    export STARLIGHT_TEST_EXPECT_WIDGET_STYLE="$fixture_widget_style"
else
    unset STARLIGHT_TEST_EXPECT_WIDGET_STYLE
fi
export STARLIGHT_X11_WM="$plasma_x11_wm"
install -d -m 0700 "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"
chmod 0700 "$XDG_RUNTIME_DIR"
cp "$fixture" "$XDG_CONFIG_HOME/kdeglobals"

build="$private_root/build"
mkdir -p "$build/qml"
(
    cd "$build/qml"
    qmake6 "$source_dir/tests/qml/qml.pro"
    make -j"$(nproc)"
)
runner="$build/qml/tst_qmlcharacterization"
[[ -x $runner ]] || { echo "QML test executable was not built: $runner" >&2; exit 1; }

# XDG variables are exported before dbus-run-session so D-Bus activation stays private.
# shellcheck disable=SC2016 # The inner shell expands its own XDG and selector values.
dbus-run-session -- xvfb-run -a -s '-screen 0 1280x900x24' bash -c '
    set -euo pipefail
    "$STARLIGHT_X11_WM" >"$XDG_RUNTIME_DIR/window-manager.log" 2>&1 &
    wm_pid=$!
    cleanup() { kill -TERM "$wm_pid" 2>/dev/null || true; wait "$wm_pid" 2>/dev/null || true; }
    trap cleanup EXIT
    for _ in $(seq 1 80); do
        xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id" && break
        sleep 0.1
    done
    xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id"
    runner=$1
    source_dir=$2
    shift 2
    for selector in "$@"; do
        "$runner" -input "$source_dir/tests/qml/$selector.qml" -o -,txt
    done
' bash "$runner" "$source_dir" "$@" &
session_pid=$!
if wait "$session_pid"; then
    session_status=0
else
    session_status=$?
fi
session_pid=
exit "$session_status"
