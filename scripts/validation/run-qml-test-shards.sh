#!/usr/bin/env bash
# Runs each tst_*.qml as its own bounded-concurrency process so the QML test
# wall time is no longer serialized by one single-threaded QtQuickTest run.
#
# Isolation per shard: a dedicated dbus session, a dedicated Xvfb display
# (xvfb-run -a) with its own window manager — window activation and focus are
# display-global, so concurrent shards must never share one X server — and a
# dedicated private XDG namespace. Style selection stays process-global and
# comes from the caller's environment, unchanged.
#
# Usage:
#   run-qml-test-shards.sh --binary PATH --tests-dir DIR --output DIR \
#       [--jobs N] [--wm NAME]
#
# The caller's environment (QT_QUICK_CONTROLS_STYLE, QT_QPA_PLATFORM,
# QT_QUICK_BACKEND, expectation variables, …) is inherited by every shard.
# Exit is nonzero if any shard fails, is missing its Totals line, or reports
# failures; per-shard logs land in --output as shard-<name>.log plus a
# combined shards-combined.log for warning auditing.
set -euo pipefail

binary=
tests_dir=
output=
jobs=4
wm=${STARLIGHT_X11_WM:-}
while (($#)); do
    case $1 in
        --binary) binary=${2:?--binary requires a path}; shift 2 ;;
        --tests-dir) tests_dir=${2:?--tests-dir requires a directory}; shift 2 ;;
        --output) output=${2:?--output requires a directory}; shift 2 ;;
        --jobs) jobs=${2:?--jobs requires an integer}; shift 2 ;;
        --wm) wm=${2:?--wm requires a command}; shift 2 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done
[[ -x $binary && -d $tests_dir && -n $output ]] || {
    echo 'usage: run-qml-test-shards.sh --binary PATH --tests-dir DIR --output DIR [--jobs N] [--wm NAME]' >&2
    exit 2
}
[[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo '--jobs must be a positive integer' >&2; exit 2; }
[[ -n $wm ]] || { echo 'a window manager is required (--wm or STARLIGHT_X11_WM)' >&2; exit 2; }
binary=$(cd "$(dirname "$binary")" && pwd -P)/$(basename "$binary")
tests_dir=$(cd "$tests_dir" && pwd -P)
mkdir -p "$output"
output=$(cd "$output" && pwd -P)

mapfile -t shard_files < <(find "$tests_dir" -maxdepth 1 -name 'tst_*.qml' -print | sort)
((${#shard_files[@]} > 0)) || { echo "no tst_*.qml files in $tests_dir" >&2; exit 2; }

# Explicit per-shard display numbers: xvfb-run -a's lockfile probing races
# between simultaneous shards and cannot see abstract-socket X servers from
# concurrent container legs. Pick a randomized base per run, then walk it
# checking both lockfiles and abstract sockets before assigning.
display_taken() {
    [[ -e /tmp/.X$1-lock ]] && return 0
    grep -q "@/tmp/.X11-unix/X$1\b" /proc/net/unix 2>/dev/null
}
display_base=$((200 + RANDOM % 500))
declare -a shard_displays
next_display=$display_base
for _ in "${shard_files[@]}"; do
    while display_taken "$next_display"; do next_display=$((next_display + 1)); done
    shard_displays+=("$next_display")
    next_display=$((next_display + 1))
done

run_shard() {
    local file=$1 display=$2 name log root
    name=$(basename "$file" .qml)
    log="$output/shard-$name.log"
    root=$(mktemp -d "$output/.$name.xdg.XXXXXX")
    mkdir -p "$root/config" "$root/cache" "$root/data" "$root/runtime"
    chmod 700 "$root/runtime"
    local status=0
    XDG_CONFIG_HOME="$root/config" \
    XDG_CACHE_HOME="$root/cache" \
    XDG_DATA_HOME="$root/data" \
    XDG_RUNTIME_DIR="$root/runtime" \
    STARLIGHT_SHARD_WM="$wm" \
    dbus-run-session -- xvfb-run -n "$display" -s '-screen 0 1280x900x24' bash -c '
        set -euo pipefail
        # Word splitting is intentional: the WM may carry arguments.
        $STARLIGHT_SHARD_WM >/dev/null 2>&1 & wm_pid=$!
        trap "kill -TERM $wm_pid 2>/dev/null || true; wait $wm_pid 2>/dev/null || true" EXIT
        for _ in $(seq 1 80); do xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id" && break; sleep 0.1; done
        xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id"
        exec "$1" -input "$2"
    ' shard "$binary" "$file" >"$log" 2>&1 || status=$?
    # Session helpers (document portal, gvfs) FUSE-mount inside the private
    # runtime dir and can outlive their session by a beat; unmount
    # deepest-first and retry briefly. Leftovers are only warned about — the
    # shard root lives inside the caller's private tree, whose own cleanup
    # removes and verifies stragglers.
    local attempt
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        while IFS= read -r mount_point; do
            fusermount -u "$mount_point" 2>/dev/null \
                || umount "$mount_point" 2>/dev/null || true
        done < <(awk -v prefix="$root/" 'index($5, prefix) == 1 { print $5 }' \
                 /proc/self/mountinfo | sort -r)
        rm -rf "$root" 2>/dev/null && break
        sleep 0.3
    done
    [[ -e $root ]] && echo "shard $name left state for outer cleanup: $root" >&2
    return "$status"
}

declare -A shard_pid_name
failures=0
running=0
shard_index=0
# A failed shard's log would otherwise be lost: the caller's set -e aborts
# before shards-combined.log is emitted, and the private work tree is
# cleaned. Dump the failing shard's tail to stderr immediately so every
# failure is diagnosable from the retained leg log alone.
report_shard_failure() {
    local name=$1
    echo "shard failed: $name" >&2
    if [[ -f $output/shard-$name.log ]]; then
        tail -n 60 "$output/shard-$name.log" | sed "s/^/[$name] /" >&2
    fi
}
for file in "${shard_files[@]}"; do
    run_shard "$file" "${shard_displays[$shard_index]}" &
    shard_pid_name[$!]=$(basename "$file" .qml)
    shard_index=$((shard_index + 1))
    running=$((running + 1))
    if ((running >= jobs)); then
        if wait -n -p finished_pid; then :; else
            report_shard_failure "${shard_pid_name[$finished_pid]:-unknown}"
            failures=$((failures + 1))
        fi
        running=$((running - 1))
    fi
done
while ((running > 0)); do
    if wait -n -p finished_pid; then :; else
        report_shard_failure "${shard_pid_name[$finished_pid]:-unknown}"
        failures=$((failures + 1))
    fi
    running=$((running - 1))
done

total_passed=0
total_failed=0
total_skipped=0
: >"$output/shards-combined.log"
for file in "${shard_files[@]}"; do
    name=$(basename "$file" .qml)
    log="$output/shard-$name.log"
    cat "$log" >>"$output/shards-combined.log"
    totals=$(grep -E '^Totals: ' "$log" | tail -1 || true)
    if [[ ! $totals =~ ^Totals:\ ([0-9]+)\ passed,\ ([0-9]+)\ failed,\ ([0-9]+)\ skipped ]]; then
        echo "shard $name produced no Totals line" >&2
        failures=$((failures + 1))
        continue
    fi
    total_passed=$((total_passed + BASH_REMATCH[1]))
    total_failed=$((total_failed + BASH_REMATCH[2]))
    total_skipped=$((total_skipped + BASH_REMATCH[3]))
    printf 'shard %-28s %s\n' "$name" "$totals"
done
echo "Shard totals: $total_passed passed, $total_failed failed, $total_skipped skipped across ${#shard_files[@]} shards"
[[ $failures -eq 0 && $total_failed -eq 0 ]] || exit 1
