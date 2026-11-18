#!/usr/bin/env bash
# Historical migration harness. Requires a prepared private COW seed and Ubuntu
# image; not the contributor build path. Prefer ./dev and docs/development.md.
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
warning_allowlist="$script_dir/pagewide-warning-allowlist.txt"
qml_warning_allowlist="$script_dir/pagewide-qml-warning-allowlist.txt"

check_warning_logs() {
    local normalized qml_normalized unexpected log line pending_busy
    normalized=$(mktemp "${TMPDIR:-/tmp}/starlight-warning-lines.XXXXXX")
    qml_normalized=$(mktemp "${TMPDIR:-/tmp}/starlight-qml-warning-lines.XXXXXX")
    unexpected=$(mktemp "${TMPDIR:-/tmp}/starlight-unexpected-warnings.XXXXXX")
    trap 'rm -f "$normalized" "$qml_normalized" "$unexpected"' RETURN
    for log in "$@"; do
        [[ -f $log ]] || continue
        pending_busy=
        while IFS= read -r line; do
            if [[ $line == *'warning: '* ]]; then
                printf '%s\n' "${line#*warning: }" >>"$normalized"
            fi
            if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*QQmlExpression:\ Expression\ file://.*/app/gui/starlight/SLBusy\.qml:16:9\ depends\ on\ (non-bindable|non-NOTIFYable)\ properties:$ ]]; then
                [[ -z $pending_busy ]] || printf 'unpaired QML warning: %s\n' "$pending_busy" >>"$qml_normalized"
                pending_busy=$line
                continue
            fi
            if [[ -n $pending_busy ]]; then
                if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*QQuickMaterialBusyIndicator_QML_[0-9]+::color$ ]]; then
                    printf '%s\n' 'SLBusy.qml:16:9 BusyIndicator color pair' >>"$qml_normalized"
                    pending_busy=
                    continue
                fi
                printf 'unpaired QML warning: %s\n' "$pending_busy" >>"$qml_normalized"
                pending_busy=
            fi
            if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*qt\.qpa\.services:\ Failed\ to\ register\ with\ host\ portal\ QDBusError ]]; then
                printf '%s\n' 'nested-session host portal registration noise' >>"$qml_normalized"
                continue
            fi
            if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*WindowEffects::test_.*file://.*/app/gui/starlight/PinnedStackView\.qml:[1-9][0-9]*:5:\ QML\ StackView:\ push:\ file://.*/tests/qml/fixtures/missing-quit-segue\.qml.*No\ such\ file\ or\ directory$ ]]; then
                printf '%s\n' 'WindowEffects deliberate missing quit segue push failure' >>"$qml_normalized"
                continue
            fi
            if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*WindowEffects::test_.*QQmlComponent:\ Component\ is\ not\ ready$ ]]; then
                printf '%s\n' 'WindowEffects deliberate component-not-ready failure' >>"$qml_normalized"
                continue
            fi
            if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*WindowEffects::test_.*QQmlComponent:\ Component\ destroyed\ while\ completion\ pending$ ]]; then
                printf '%s\n' 'WindowEffects deliberate destroyed-while-pending teardown' >>"$qml_normalized"
                continue
            fi
            if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*WindowEffects::test_.*file://.*/tests/qml/fixtures/FailingStreamSegue\.qml:[1-9][0-9]*:5:\ Required\ property\ requiredFailure\ was\ not\ initialized$ ]]; then
                printf '%s\n' 'WindowEffects deliberate failing segue required property' >>"$qml_normalized"
                continue
            fi
            if [[ $line =~ ^QWARN[[:space:]]*:[[:space:]].*file://.*/app/gui/main\.qml:[1-9][0-9]*:5:\ QML\ Settings:\ The\ Settings\ type\ from\ Qt\.labs\.settings\ is\ deprecated\ and\ will\ be\ removed\ in\ a\ future\ release\.\ Please\ use\ the\ one\ from\ QtCore\ instead\.$ ]]; then
                printf '%s\n' 'main.qml Qt.labs.settings deprecation' >>"$qml_normalized"
            elif [[ $line == QWARN* || $line == *'QQml'*[Ww]arning* || $line == *'qml:'*[Ww]arning* ]]; then
                printf 'unrecognized QML warning: %s\n' "$line" >>"$qml_normalized"
            fi
        done <"$log"
        [[ -z $pending_busy ]] || printf 'unpaired QML warning: %s\n' "$pending_busy" >>"$qml_normalized"
    done
    sort -u "$normalized" -o "$normalized"
    sort -u "$warning_allowlist" | comm -23 "$normalized" - >"$unexpected"
    if [[ -s $unexpected ]]; then
        echo 'unexpected compiler warning output:' >&2
        cat "$unexpected" >&2
        return 1
    fi
    sort -u "$qml_normalized" -o "$qml_normalized"
    sort -u "$qml_warning_allowlist" | comm -23 "$qml_normalized" - >"$unexpected"
    if [[ -s $unexpected ]]; then
        echo 'unexpected QML warning output:' >&2
        cat "$unexpected" >&2
        return 1
    fi
}

usage() {
    cat <<'EOF'
Usage: run-pagewide-stage-gates.sh --source DIR --stage N [--ubuntu-image NAME] [--managed-x11] [--start-at flatpak]

Run the Qt 6.4 Ubuntu floor, host Qt, AOT/symbol, and private-COW Flatpak
validation gates. N must be one non-negative integer. Logs and manifests are
written to artifacts/pagewide-stage-N (or retained there on failure). The
flatpak start point is a diagnostic only; it never records a full-stage pass.
EOF
}

source_dir=
stage=
ubuntu_image=ubuntu-base-24.04-amd64
managed_x11=true
warning_log=
start_at=${STARLIGHT_STAGE_START_AT:-all}
while (($#)); do
    case $1 in
        --help|-h) usage; exit 0 ;;
        --source) source_dir=${2:?--source requires a directory}; shift 2 ;;
        --stage) stage=${2:?--stage requires an integer}; shift 2 ;;
        --ubuntu-image) ubuntu_image=${2:?--ubuntu-image requires a value}; shift 2 ;;
        --managed-x11) managed_x11=true; shift ;;
        --start-at) start_at=${2:?--start-at requires a value}; shift 2 ;;
        --check-warning-log) warning_log=${2:?--check-warning-log requires a file}; shift 2 ;;
        --output) echo '--output is derived from --stage; use artifacts/pagewide-stage-N' >&2; exit 2 ;;
        --*) echo "unknown option: $1" >&2; exit 2 ;;
        *) echo "unexpected argument: $1" >&2; exit 2 ;;
    esac
done
[[ $start_at == all || $start_at == flatpak ]] || {
    echo '--start-at must be flatpak' >&2
    exit 2
}
if [[ -n $warning_log ]]; then
    [[ -f $warning_log ]] || { echo "warning log does not exist: $warning_log" >&2; exit 2; }
    check_warning_logs "$warning_log"
    exit 0
fi
[[ -n $source_dir && -n $stage && $stage =~ ^[0-9]+$ ]] || { usage >&2; exit 2; }
source_dir=$(cd "$source_dir" && pwd -P)
[[ $managed_x11 == true ]] || { echo '--managed-x11 is required' >&2; exit 2; }
build_jobs=${STARLIGHT_STAGE_JOBS:-"$(nproc)"}
[[ $build_jobs =~ ^[1-9][0-9]*$ ]] || { echo 'STARLIGHT_STAGE_JOBS must be a positive integer' >&2; exit 2; }
artifact_dir="$source_dir/artifacts"
canonical_artifact="$artifact_dir/pagewide-stage-$stage"
manifest_hash_part_dirs=()

manifest() {
    local destination=$1 root hash_parts
    local -a manifest_roots=(
        "$HOME/.config/Starlight" "$HOME/.cache/Starlight" "$HOME/.local/share/Starlight"
        "$HOME/.qttest" "$HOME/.var/app/com.starlight.Starlight"
        "$HOME/.var/app/org.flatpak.Builder" "$HOME/.local/share/flatpak"
        "$source_dir/build-baseline"
    )
    if [[ -n ${STARLIGHT_STAGE_MANIFEST_EXTRA_ROOT:-} ]]; then
        manifest_roots+=("$STARLIGHT_STAGE_MANIFEST_EXTRA_ROOT")
    fi
    : >"$destination"
    while IFS= read -r -d '' root; do
        if [[ ! -e $root && ! -L $root ]]; then
            printf 'missing\0%s\0' "$root" >>"$destination"
            continue
        fi
        find "$root" -xdev -print0 | LC_ALL=C sort -z | \
            xargs -0 -r -n 1024 stat --printf='node\0%n\0%F\0%i\0%s\0%Y\0%a\0' \
            >>"$destination"
        hash_parts=$(mktemp -d "${TMPDIR:-/tmp}/starlight-stage-manifest-hashes.XXXXXX")
        manifest_hash_part_dirs+=("$hash_parts")
        if ! (
            export STARLIGHT_STAGE_MANIFEST_HASH_PARTS="$hash_parts"
            # shellcheck disable=SC2016 # The worker expands its own positional arguments and environment.
            find "$root" -xdev -type f -print0 | LC_ALL=C sort -z | \
                xargs -0 -r -n 256 -P "$build_jobs" \
                    --process-slot-var=STARLIGHT_STAGE_MANIFEST_HASH_SLOT \
                    bash -c '
                        sha256sum -z -- "$@" >>"$STARLIGHT_STAGE_MANIFEST_HASH_PARTS/worker-$STARLIGHT_STAGE_MANIFEST_HASH_SLOT"
                    ' bash
        ); then
            rm -rf "$hash_parts"
            return 1
        fi
        if ! (
            find "$hash_parts" -type f -print0 | LC_ALL=C sort -z | \
                xargs -0 -r cat | LC_ALL=C sort -z -k1.67
        ) >>"$destination"; then
            rm -rf "$hash_parts"
            return 1
        fi
        rm -rf "$hash_parts"
    done < <(printf '%s\0' "${manifest_roots[@]}")
}
if [[ ${STARLIGHT_STAGE_MANIFEST_POLICY_PROBE:-0} == 1 ]]; then
    probe_root=${STARLIGHT_STAGE_MANIFEST_PROBE_ROOT:?manifest probe root required}
    probe_before=${STARLIGHT_STAGE_MANIFEST_PROBE_BEFORE:?manifest probe before required}
    probe_after=${STARLIGHT_STAGE_MANIFEST_PROBE_AFTER:?manifest probe after required}
    probe_repeat=${STARLIGHT_STAGE_MANIFEST_PROBE_REPEAT:?manifest probe repeat required}
    manifest "$probe_before"
    printf 'changed\n' >"$probe_root/nested/leaf"
    manifest "$probe_after"
    if cmp -s "$probe_before" "$probe_after"; then
        echo 'manifest policy probe did not detect the nested mutation' >&2
        exit 1
    fi
    manifest "$probe_repeat"
    cmp -s "$probe_after" "$probe_repeat"
    exit 0
fi
failed=true
private_root=
run_artifact=
run_id=
artifact=
protected_before=
protected_after=
cleanup() {
    local status=$1 protected_changed=false
    trap - EXIT HUP INT TERM
    if [[ -z $run_artifact ]]; then
        mkdir -p "$artifact_dir"
        run_artifact=$(mktemp -d "$artifact_dir/.pagewide-stage-$stage.run.XXXXXX")
        run_id=${run_artifact##*.run.}
        artifact=$run_artifact
    fi
    if [[ -f $protected_before ]]; then manifest "$protected_after"; fi
    if [[ -f $protected_before ]] && ! cmp -s "$protected_before" "$protected_after"; then
        echo 'protected path changed' >&2
        status=1
        protected_changed=true
    fi
    for manifest_hash_part_dir in "${manifest_hash_part_dirs[@]}"; do
        rm -rf "$manifest_hash_part_dir"
    done
    if [[ -n $private_root && -d $private_root ]]; then rm -rf "$private_root"; fi
    [[ -z $private_root || ! -e $private_root ]] || status=1
    printf 'cleanup=%s\n' "$([[ -z $private_root || ! -e $private_root ]] && echo pass || echo fail)" >"$artifact/cleanup.txt"
    if ((status != 0)); then
        failed=true
        if [[ $protected_changed == true ]]; then
            printf '%s\n' 'page-wide stage gate: INVALID (protected path changed during cleanup)' >"$artifact/result.txt"
        else
            printf '%s\n' 'page-wide stage gate: INVALID (cleanup failed)' >"$artifact/result.txt"
        fi
    fi
    if [[ $failed == true ]]; then
        mv "$run_artifact" "$canonical_artifact.failed.$run_id" 2>/dev/null || status=1
    else
        mv "$run_artifact" "$canonical_artifact" 2>/dev/null || status=1
    fi
    exit "$status"
}
trap 'cleanup $?' EXIT
trap 'cleanup 129' HUP
trap 'cleanup 130' INT
trap 'cleanup 143' TERM
if [[ ${STARLIGHT_STAGE_INTERRUPT_PROBE:-} == earliest ]]; then
    : >"${STARLIGHT_STAGE_INTERRUPT_READY:?interrupt readiness path required}"
    sleep 30
fi
mkdir -p "$artifact_dir"
run_artifact=$(mktemp -d "$artifact_dir/.pagewide-stage-$stage.run.XXXXXX")
chmod 0700 "$run_artifact"
run_id=${run_artifact##*.run.}
if [[ -e $canonical_artifact ]]; then mv "$canonical_artifact" "$canonical_artifact.superseded.$run_id"; fi
artifact=$run_artifact
printf 'run-id=%s\nrun-started-utc=%s\n' "$run_id" "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >"$artifact/run-metadata.txt"
protected_before="$artifact/protected-before.manifest"
protected_after="$artifact/protected-after.manifest"
if [[ ${STARLIGHT_STAGE_INTERRUPT_PROBE:-} == pre ]]; then
    : >"${STARLIGHT_STAGE_INTERRUPT_READY:?interrupt readiness path required}"
    sleep 30
fi
manifest "$protected_before"

private_root=$(mktemp -d "$source_dir/.superpowers/sdd/2026-07-29-pagewide-desktop-native/stage-private-$stage.XXXXXX")
if [[ ${STARLIGHT_STAGE_INTERRUPT_PROBE:-} == body ]]; then
    : >"${STARLIGHT_STAGE_INTERRUPT_READY:?interrupt readiness path required}"
    sleep 30
fi

# Keeps the artifact lifecycle executable in the shell contract test without
# starting a machine-expensive validation matrix.
if [[ ${STARLIGHT_STAGE_ARTIFACT_POLICY_PROBE:-0} == 1 ]]; then
    printf '%s\n' 'artifact-policy-probe=pass' >"$artifact/artifact-policy-probe.txt"
    if [[ ${STARLIGHT_STAGE_ARTIFACT_MISMATCH_PROBE:-0} == 1 ]]; then
        install -d -m 0700 "$HOME/.config/Starlight"
        printf '%s\n' changed >"$HOME/.config/Starlight/cleanup-probe"
    fi
    failed=false
    exit 0
fi

export XDG_CONFIG_HOME="$private_root/config"
export XDG_CACHE_HOME="$private_root/cache"
export XDG_DATA_HOME="$private_root/data"
export XDG_RUNTIME_DIR="$private_root/runtime"
export QML_DISABLE_DISK_CACHE=1
export QT_QPA_PLATFORM=xcb
export QT_QUICK_BACKEND=software
export QT_QUICK_CONTROLS_STYLE=Material
install -d -m 0700 "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR"
chmod 0700 "$XDG_RUNTIME_DIR"

run_host() {
    local label=$1 work="$private_root/$1"
    mkdir -p "$work/app" "$work/tests"
    (
        cd "$work/app"
        qmake6 "$source_dir/moonlight-qt.pro" CONFIG+=qtquickcompiler
        make -j"$build_jobs"
        ldd -r app/starlight
        find . -type f \( -name '*qmlcache_loader*' -o -name '*.qmlc' \) -print | sort >"$artifact/$label-aot.txt"
        test -s "$artifact/$label-aot.txt"
    ) >"$artifact/$label-app.log" 2>&1
    (
        cd "$work/tests"
        qmake6 "$source_dir/tests/tests.pro"
        make -j"$build_jobs"
        # shellcheck disable=SC2016 # The contained X11 session expands its own variables.
        dbus-run-session -- xvfb-run -a -s '-screen 0 1280x900x24' bash -c '
            set -euo pipefail
            "$STARLIGHT_X11_WM" >"$XDG_RUNTIME_DIR/window-manager.log" 2>&1 & wm=$!
            trap "kill -TERM $wm 2>/dev/null || true; wait $wm 2>/dev/null || true" EXIT
            for _ in $(seq 1 80); do xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id" && break; sleep 0.1; done
            xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id"
            make -j1 sub-auto-check
        '
        # QML suites run sharded: each tst file gets its own dbus session,
        # Xvfb display, window manager, and private namespace; the driver
        # aggregates totals and emits every shard log for the warning audit.
        shard_status=0
        "$source_dir/scripts/validation/run-qml-test-shards.sh" \
            --binary "$work/tests/qml/tst_qmlcharacterization" \
            --tests-dir "$source_dir/tests/qml" \
            --output "$work/tests/qml-shards" --jobs 4 --wm "$STARLIGHT_X11_WM" \
            || shard_status=$?
        # Retain the combined shard log on failure too: the private work
        # tree is cleaned, so this artifact copy is the only diagnosis.
        cat "$work/tests/qml-shards/shards-combined.log"
        exit "$shard_status"
    ) >"$artifact/$label-tests.log" 2>&1
}

if [[ $start_at == all ]]; then
    if command -v openbox >/dev/null; then
        host_x11_wm=openbox
    elif command -v kwin_x11 >/dev/null; then
        host_x11_wm=kwin_x11
    else
        echo 'managed X11 requires openbox or kwin_x11' >&2
        exit 1
    fi
    export STARLIGHT_X11_WM=$host_x11_wm
    run_host host
fi

# The verified Ubuntu root from the prior release gate is copied privately; it is
# never used as a build target. A caller may override it for a newly provisioned root.
if [[ $start_at == all ]]; then
    noble_root=${STARLIGHT_UBUNTU_ROOT:-"$source_dir/.superpowers/sdd/2026-07-28-desktop-native-architecture/task11-noble-root"}
    [[ $ubuntu_image == ubuntu-base-24.04-amd64 && -d $noble_root ]] || {
        echo "Ubuntu root unavailable for $ubuntu_image; set STARLIGHT_UBUNTU_ROOT" >&2; exit 1;
    }
    grep -qx 'VERSION_ID="24.04"' "$noble_root/etc/os-release"
    if [[ -x $noble_root/usr/bin/openbox ]]; then
        ubuntu_x11_wm=openbox
    elif [[ -x $noble_root/usr/bin/kwin_x11 ]]; then
        ubuntu_x11_wm=kwin_x11
    else
        echo 'Ubuntu validation root has no supported X11 window manager' >&2
        exit 1
    fi
    bwrap --unshare-user --uid 0 --gid 0 --ro-bind "$noble_root" / --proc /proc --dev /dev \
        /usr/bin/qmake6 -query QT_VERSION | grep -Eq '^6\.4\.'
fi
run_noble() {
    local work="$private_root/ubuntu"
    mkdir -p "$work/app" "$work/tests"
    # shellcheck disable=SC2016 # Variables below are evaluated inside the Ubuntu root.
    bwrap --unshare-user --uid 0 --gid 0 --ro-bind "$noble_root" / --tmpfs /tmp \
        --dir /tmp/xdg --dir /tmp/xdg/config --dir /tmp/xdg/cache --dir /tmp/xdg/data --dir /tmp/xdg/runtime \
        --ro-bind "$source_dir" /src --bind "$work" /tmp/work \
        --bind "$XDG_CONFIG_HOME" /tmp/xdg/config --bind "$XDG_CACHE_HOME" /tmp/xdg/cache \
        --bind "$XDG_DATA_HOME" /tmp/xdg/data --bind "$XDG_RUNTIME_DIR" /tmp/xdg/runtime \
        --proc /proc --dev /dev \
        --setenv XDG_CONFIG_HOME /tmp/xdg/config --setenv XDG_CACHE_HOME /tmp/xdg/cache \
        --setenv XDG_DATA_HOME /tmp/xdg/data --setenv XDG_RUNTIME_DIR /tmp/xdg/runtime \
        --setenv QML_DISABLE_DISK_CACHE 1 --setenv QT_QPA_PLATFORM xcb \
        --setenv QT_QUICK_BACKEND software --setenv QT_QUICK_CONTROLS_STYLE Material \
        --setenv STARLIGHT_X11_WM "$ubuntu_x11_wm" \
        --setenv STARLIGHT_STAGE_JOBS "$build_jobs" \
        --chdir /tmp/work/app /bin/bash -lc '
            set -euo pipefail
            qmake6 /src/moonlight-qt.pro CONFIG+=qtquickcompiler
            make -j"$STARLIGHT_STAGE_JOBS"
            ldd -r app/starlight
            find . -type f \( -name "*qmlcache_loader*" -o -name "*.qmlc" \) -print | sort
        ' >"$artifact/ubuntu-app.log" 2>&1
    grep -q 'qmlcache' "$artifact/ubuntu-app.log"
    # shellcheck disable=SC2016 # Variables below are evaluated inside the Ubuntu root.
    bwrap --unshare-user --uid 0 --gid 0 --ro-bind "$noble_root" / --tmpfs /tmp \
        --dir /tmp/xdg --dir /tmp/xdg/config --dir /tmp/xdg/cache --dir /tmp/xdg/data --dir /tmp/xdg/runtime \
        --ro-bind "$source_dir" /src --bind "$work" /tmp/work \
        --bind "$XDG_CONFIG_HOME" /tmp/xdg/config --bind "$XDG_CACHE_HOME" /tmp/xdg/cache \
        --bind "$XDG_DATA_HOME" /tmp/xdg/data --bind "$XDG_RUNTIME_DIR" /tmp/xdg/runtime \
        --proc /proc --dev /dev \
        --setenv XDG_CONFIG_HOME /tmp/xdg/config --setenv XDG_CACHE_HOME /tmp/xdg/cache \
        --setenv XDG_DATA_HOME /tmp/xdg/data --setenv XDG_RUNTIME_DIR /tmp/xdg/runtime \
        --setenv QML_DISABLE_DISK_CACHE 1 --setenv QT_QPA_PLATFORM xcb \
        --setenv QT_QUICK_BACKEND software --setenv QT_QUICK_CONTROLS_STYLE Material \
        --setenv STARLIGHT_X11_WM "$ubuntu_x11_wm" \
        --setenv STARLIGHT_STAGE_JOBS "$build_jobs" \
        --chdir /tmp/work/tests /bin/bash -lc '
            set -euo pipefail
            qmake6 /src/tests/tests.pro
            make -j"$STARLIGHT_STAGE_JOBS"
            dbus-run-session -- xvfb-run -a -s "-screen 0 1280x900x24" bash -c '\''
                set -euo pipefail
                "$STARLIGHT_X11_WM" >"$XDG_RUNTIME_DIR/window-manager.log" 2>&1 & wm=$!
                trap "kill -TERM $wm 2>/dev/null || true; wait $wm 2>/dev/null || true" EXIT
                for _ in $(seq 1 80); do xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id" && break; sleep 0.1; done
                xprop -root _NET_SUPPORTING_WM_CHECK 2>/dev/null | grep -q "window id"
                make -j1 sub-auto-check
            '\''
            shard_status=0
            /src/scripts/validation/run-qml-test-shards.sh \
                --binary /tmp/work/tests/qml/tst_qmlcharacterization \
                --tests-dir /src/tests/qml \
                --output /tmp/work/tests/qml-shards --jobs 4 --wm "$STARLIGHT_X11_WM" \
                || shard_status=$?
            cat /tmp/work/tests/qml-shards/shards-combined.log
            exit "$shard_status"''
        ' >"$artifact/ubuntu-tests.log" 2>&1
}
if [[ $start_at == all ]]; then
    run_noble
    printf '%s\n' "ubuntu-root=$noble_root" >"$artifact/ubuntu.txt"
fi

# Builder is provided as a Flatpak on this machine. Its only writable mount is
# a private COW tree; --build-only deliberately forbids install and export.
run_flatpak() {
    local flatpak_root="$private_root/flatpak"
    local local_source="$flatpak_root/source"
    local local_manifest="$flatpak_root/manifest.yaml"
    local flatpak_install="$HOME/.local/share/flatpak"
    local flatpak_seed="${STARLIGHT_FLATPAK_SEED:-$source_dir/.superpowers/sdd/2026-07-28-desktop-native-architecture/task11-flatpak/state-round4}"
    local flatpak_user_dir="$flatpak_root/user-install"
    local builder_user_install_target='/home/builder/.local/share/flatpak'
    local builder_runtime_location builder_app_location builder_runtime_relative builder_app_relative
    local builder_runtime_files builder_app_files
    local sdk_relative='runtime/org.kde.Sdk/x86_64/6.10/active'
    local cow_sdk cow_sdk_stat namespace_sdk_stat builder_sdk_stat builder_home_sdk_stat
    [[ -d $flatpak_seed && -d $flatpak_install ]] || {
        echo 'required private Flatpak COW seed or user install is unavailable' >&2
        return 1
    }
    mkdir -p "$flatpak_root" "$local_source"
    install -d -m 0700 "$XDG_DATA_HOME/flatpak"
    builder_runtime_location=$(FLATPAK_USER_DIR="$flatpak_install" flatpak --user info --show-location org.freedesktop.Sdk/x86_64/25.08)
    builder_app_location=$(FLATPAK_USER_DIR="$flatpak_install" flatpak --user info --show-location org.flatpak.Builder)
    builder_runtime_relative=${builder_runtime_location#"$flatpak_install"/}
    builder_app_relative=${builder_app_location#"$flatpak_install"/}
    [[ $builder_runtime_relative != "$builder_runtime_location" && $builder_app_relative != "$builder_app_location" ]]
    cp -a --reflink=always "$flatpak_seed" "$flatpak_root/state"
    cp -a --reflink=always "$flatpak_install" "$flatpak_user_dir"
    [[ $flatpak_user_dir != "$flatpak_install" && $flatpak_root/state != "$flatpak_seed" ]]
    builder_runtime_files="$flatpak_user_dir/$builder_runtime_relative/files"
    builder_app_files="$flatpak_user_dir/$builder_app_relative/files"
    [[ -x $builder_app_files/bin/flatpak && -x $builder_app_files/bin/flatpak-builder ]]
    cow_sdk=$(readlink -f "$flatpak_user_dir/$sdk_relative")
    cow_sdk_stat=$(stat -Lc '%d:%i' "$cow_sdk")

    # Flatpak Builder's upstream runtime lookup deliberately calls the
    # session-bus HostCommand API when it finds flatpak-info. Run its COW
    # runtime directly, with no flatpak-info, so its bundled client stays in
    # this namespace and sees only the disposable installation.
    run_private_builder_runtime() {
        bwrap --unshare-user --uid "$(id -u)" --gid "$(id -g)" \
            --unshare-net --tmpfs / --dir /usr --ro-bind "$builder_runtime_files" /usr \
            --symlink usr/bin /bin --symlink usr/lib /lib --symlink usr/lib64 /lib64 --symlink usr/sbin /sbin \
            --dir /etc --dir /app --ro-bind "$builder_app_files" /app \
            --dir /work --bind "$flatpak_root" /work \
            --dir /xdg --dir /xdg/config --dir /xdg/cache --dir /xdg/data --dir /xdg/runtime \
            --bind "$XDG_CONFIG_HOME" /xdg/config --bind "$XDG_CACHE_HOME" /xdg/cache \
            --bind "$XDG_DATA_HOME" /xdg/data --bind "$XDG_RUNTIME_DIR" /xdg/runtime \
            --bind "$flatpak_user_dir" /xdg/data/flatpak \
            --dir /home --dir /home/builder --dir /home/builder/.local \
            --bind "$flatpak_user_dir" "$builder_user_install_target" \
            --tmpfs /tmp \
            --proc /proc --dev /dev \
            --setenv HOME /home/builder --setenv XDG_CONFIG_HOME /xdg/config \
            --setenv XDG_CACHE_HOME /xdg/cache --setenv XDG_DATA_HOME /xdg/data \
            --setenv XDG_RUNTIME_DIR /xdg/runtime --setenv FLATPAK_USER_DIR "$builder_user_install_target" \
            --setenv LD_LIBRARY_PATH /app/lib:/usr/lib/x86_64-linux-gnu \
            --setenv PATH /app/bin:/usr/bin:/bin \
            -- /bin/sh -c '
                printf "/app/lib\\n" >/etc/ld.so.conf
                /sbin/ldconfig
                exec "$@"
            ' sh "$@"
    }
    {
        printf 'cow-sdk=%s\n' "$cow_sdk"
        printf 'cow-sdk-stat=%s\n' "$cow_sdk_stat"
        # shellcheck disable=SC2016 # The contained namespace shell expands its own variables.
        run_private_builder_runtime /bin/sh -c '
            test ! -e "$XDG_RUNTIME_DIR/flatpak-info"
            printf "direct-runtime-flatpak-info=absent\\n"
            readlink -f "$XDG_DATA_HOME/flatpak/runtime/org.kde.Sdk/x86_64/6.10/active"
            printf "namespace-sdk-stat=%s\\n" "$(stat -Lc "%d:%i" "$XDG_DATA_HOME/flatpak/runtime/org.kde.Sdk/x86_64/6.10/active")"
            flatpak --user info --arch=x86_64 org.kde.Sdk//6.10
        '
    } >"$artifact/flatpak-user-install.log" 2>&1
    namespace_sdk_stat=$(sed -n 's/^namespace-sdk-stat=//p' "$artifact/flatpak-user-install.log")
    [[ $namespace_sdk_stat == "$cow_sdk_stat" ]]
    {
        printf 'cow-sdk-stat=%s\n' "$cow_sdk_stat"
        # shellcheck disable=SC2016 # The direct Builder shell expands its own XDG variables.
        run_private_builder_runtime /bin/sh -c '
                test ! -e "$XDG_RUNTIME_DIR/flatpak-info"
                printf "builder-direct-runtime=1\\n"
                printf "builder-xdg-data=%s\\n" "$XDG_DATA_HOME"
                readlink -f "$XDG_DATA_HOME/flatpak/runtime/org.kde.Sdk/x86_64/6.10/active"
                printf "builder-xdg-sdk-stat=%s\\n" "$(stat -Lc "%d:%i" "$XDG_DATA_HOME/flatpak/runtime/org.kde.Sdk/x86_64/6.10/active")"
                readlink -f "$HOME/.local/share/flatpak/runtime/org.kde.Sdk/x86_64/6.10/active"
                printf "builder-home-sdk-stat=%s\\n" "$(stat -Lc "%d:%i" "$HOME/.local/share/flatpak/runtime/org.kde.Sdk/x86_64/6.10/active")"
                flatpak --user info --arch=x86_64 org.kde.Sdk//6.10
            '
    } >"$artifact/flatpak-builder-user-install.log" 2>&1
    builder_sdk_stat=$(sed -n 's/^builder-xdg-sdk-stat=//p' "$artifact/flatpak-builder-user-install.log")
    builder_home_sdk_stat=$(sed -n 's/^builder-home-sdk-stat=//p' "$artifact/flatpak-builder-user-install.log")
    [[ $builder_sdk_stat == "$cow_sdk_stat" ]]
    [[ $builder_home_sdk_stat == "$cow_sdk_stat" ]]
    tar -C "$source_dir" --exclude=.git --exclude=.superpowers --exclude=artifacts \
        --exclude=build-baseline -cf - . | tar -xf - -C "$local_source"
    cp "$source_dir/flatpak/com.starlight.Starlight.yaml" "$local_manifest"
    perl -0pi -e '
        s{(?ms)(  - name: starlight\n.*?    sources:\n).*?(?=\n\n  # --- vainfo)}
         {$1 . "      - type: dir\n        path: /work/source\n        skip: [.git, .superpowers, artifacts, build-baseline, flatpak]\n"}e
         or die "unable to replace the Starlight Flatpak source\n";
    ' "$local_manifest"
    run_private_builder_runtime /app/bin/flatpak-builder --version \
        >"$artifact/flatpak-builder-version.log" 2>&1
    # shellcheck disable=SC2016 # The direct Builder shell expands its own XDG variables.
    run_private_builder_runtime /bin/sh -c '
            test ! -e "$XDG_RUNTIME_DIR/flatpak-info"
            printf "builder-direct-runtime=1\\n"
            exec /app/bin/flatpak-builder "$@"
        ' sh \
        --disable-download \
        --user --disable-rofiles-fuse --build-only --force-clean --jobs="$build_jobs" \
        --state-dir=/work/state /work/build /work/manifest.yaml \
        >"$artifact/flatpak-build.log" 2>&1
    run_private_builder_runtime /app/bin/flatpak build --readonly --unshare=network \
        --env=QT_QPA_PLATFORM=offscreen --env=QT_QUICK_BACKEND=software \
        --env=LIBGL_ALWAYS_SOFTWARE=1 --env=SDL_VIDEODRIVER=dummy \
        /work/build /app/bin/starlight --version >"$artifact/flatpak-smoke.log" 2>&1
}
run_flatpak

# Compiler/QML diagnostics are owned by Starlight's host and Ubuntu legs.
# Builder's log also contains immutable third-party Flatpak module output, so
# it is retained as evidence but is not mixed into the project warning policy.
if [[ $start_at == all ]]; then
    check_warning_logs "$artifact/host-app.log" "$artifact/host-tests.log" "$artifact/ubuntu-app.log" "$artifact/ubuntu-tests.log"
fi
failed=false
if [[ $start_at == all ]]; then
    printf '%s\n' 'page-wide stage gate: PASS' >"$artifact/result.txt"
else
    printf '%s\n' 'flatpak-only diagnostic: COMPLETE (full stage result: not evaluated)' >"$artifact/result.txt"
fi
