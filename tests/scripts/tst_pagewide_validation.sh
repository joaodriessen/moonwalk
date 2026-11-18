#!/usr/bin/env bash
set -euo pipefail

# This catches a selector regression that dispatches the aggregate QML suite,
# a wrong test file, or starts a test process after selector validation fails.
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
qml_runner="$repo/scripts/validation/run-private-qml-test.sh"
plasma_runner="$repo/scripts/validation/run-private-plasma-page-test.sh"
stage_runner="$repo/scripts/validation/run-pagewide-stage-gates.sh"
cuda_renderer="$repo/app/streaming/video/ffmpeg-renderers/cuda.cpp"
plvk_renderer="$repo/app/streaming/video/ffmpeg-renderers/plvk.cpp"

temp=$(mktemp -d)
cleanup() {
    rm -rf "$temp"
}
trap cleanup EXIT

source="$temp/source"
tools="$temp/tools"
record="$temp/record"
mkdir -p "$source/tests/qml" "$tools"
printf '%s\n' 'import QtQuick' >"$source/tests/qml/tst_pairing.qml"
printf '%s\n' 'import QtQuick' >"$source/tests/qml/tst_another.qml"

cat >"$tools/qmake6" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
exit 0
EOF

cat >"$tools/make" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
cat >tst_qmlcharacterization <<'TESTER'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$*" >>"$PAGEWIDE_TEST_RECORD"
TESTER
chmod +x tst_qmlcharacterization
EOF
chmod +x "$tools/qmake6" "$tools/make"

for script in run-private-qml-test.sh run-private-plasma-page-test.sh \
              run-pagewide-stage-gates.sh; do
    test -x "$repo/scripts/validation/$script"
    "$repo/scripts/validation/$script" --help >/dev/null
done
rg -q 'QML_DISABLE_DISK_CACHE=1' "$qml_runner"
rg -q 'chmod 0700' "$qml_runner"
rg -q 'tar -C "\$source_dir" --exclude=.git --exclude=.superpowers --exclude=artifacts' "$stage_runner"
rg -q 'run_private_builder_runtime' "$stage_runner"
rg -q 'task11-flatpak/state-round4' "$stage_runner"
rg -q 'cp -a --reflink=always "\$flatpak_seed"' "$stage_runner"
rg -q 'flatpak_user_dir="\$flatpak_root/user-install"' "$stage_runner"
rg -Fq 'FLATPAK_USER_DIR="$flatpak_install" flatpak --user info --show-location org.freedesktop.Sdk/x86_64/25.08' "$stage_runner"
rg -Fq 'FLATPAK_USER_DIR="$flatpak_install" flatpak --user info --show-location org.flatpak.Builder' "$stage_runner"
rg -Fq 'builder_runtime_files="$flatpak_user_dir/$builder_runtime_relative/files"' "$stage_runner"
rg -Fq 'builder_app_files="$flatpak_user_dir/$builder_app_relative/files"' "$stage_runner"
rg -q -- '--disable-download' "$stage_runner"
rg -Fq 'find "$root" -xdev -print0 | LC_ALL=C sort -z' "$stage_runner"
rg -Fq 'find "$root" -xdev -type f -print0 | LC_ALL=C sort -z' "$stage_runner"
rg -Fq 'printf '\''missing\0%s\0'\'' "$root"' "$stage_runner"
rg -Fq 'xargs -0 -r -n 1024 stat --printf' "$stage_runner"
rg -Fq 'STARLIGHT_STAGE_MANIFEST_HASH_PARTS' "$stage_runner"
rg -Fq -- '--process-slot-var=STARLIGHT_STAGE_MANIFEST_HASH_SLOT' "$stage_runner"
if rg -Fq 'xargs -0 -r -n 256 -P "$build_jobs" sha256sum -z --' "$stage_runner"; then
    echo 'parallel sha256sum workers must not share the manifest pipeline' >&2
    exit 1
fi
rg -Fq 'sort -z -k1.67' "$stage_runner"
rg -q -- '--user --disable-rofiles-fuse --build-only' "$stage_runner"
rg -q 'path: /work/source' "$stage_runner"
rg -q -- 'flatpak build --readonly --unshare=network' "$stage_runner"
rg -q -- '--env=QT_QPA_PLATFORM=offscreen --env=QT_QUICK_BACKEND=software' "$stage_runner"
rg -Fq 'check_warning_logs "$artifact/host-app.log" "$artifact/host-tests.log" "$artifact/ubuntu-app.log" "$artifact/ubuntu-tests.log"' "$stage_runner"
rg -Fq 'run_artifact=$(mktemp -d "$artifact_dir/.pagewide-stage-$stage.run.XXXXXX")' "$stage_runner"
rg -Fq 'mv "$canonical_artifact" "$canonical_artifact.superseded.$run_id"' "$stage_runner"
rg -Fq 'mv "$run_artifact" "$canonical_artifact"' "$stage_runner"
rg -Fq 'mv "$run_artifact" "$canonical_artifact.failed.$run_id"' "$stage_runner"
rg -q 'exec /app/bin/flatpak-builder "\$@"' "$stage_runner"
rg -q -- '--tmpfs / --dir /usr --ro-bind "\$builder_runtime_files" /usr' "$stage_runner"
rg -q -- '--symlink usr/bin /bin --symlink usr/lib /lib --symlink usr/lib64 /lib64' "$stage_runner"
rg -Fq 'printf "/app/lib\\n" >/etc/ld.so.conf' "$stage_runner"
rg -q -- '/sbin/ldconfig' "$stage_runner"
rg -q -- '--ro-bind "\$builder_app_files" /app' "$stage_runner"
rg -q -- '--unshare-net' "$stage_runner"
rg -q -- '--setenv PATH /app/bin:/usr/bin:/bin' "$stage_runner"
rg -q -- '--setenv LD_LIBRARY_PATH /app/lib:/usr/lib/x86_64-linux-gnu' "$stage_runner"
rg -Fq 'test ! -e "$XDG_RUNTIME_DIR/flatpak-info"' "$stage_runner"
rg -q 'direct-runtime-flatpak-info=absent' "$stage_runner"
rg -q 'namespace-sdk-stat=' "$stage_runner"
rg -q 'builder-xdg-sdk-stat=' "$stage_runner"
rg -q 'builder-home-sdk-stat=' "$stage_runner"
rg -q 'flatpak --user info --arch=x86_64 org.kde.Sdk//6.10' "$stage_runner"
rg -q 'bwrap --unshare-user --uid' "$stage_runner"
rg -Fq 'CUDA_MEMCPY2D cu2d{};' "$cuda_renderer"
awk '
    /#pragma GCC diagnostic push/ { pushed = 1 }
    pushed && /#pragma GCC diagnostic ignored "-Wdeprecated-declarations"/ { ignored = 1 }
    ignored && /vkDeviceContext->lock_queue = lockQueue;/ { lock = 1 }
    lock && /vkDeviceContext->unlock_queue = unlockQueue;/ { unlock = 1 }
    unlock && /#pragma GCC diagnostic pop/ { popped = 1 }
    END { exit pushed && ignored && lock && unlock && popped ? 0 : 1 }
' "$plvk_renderer"

manifest_probe_root="$temp/manifest-root"
manifest_probe_home="$temp/manifest-home"
mkdir -p "$manifest_probe_root/nested"
mkdir -p "$manifest_probe_home"
printf '%s\n' original >"$manifest_probe_root/nested/leaf"
weird_leaf=$manifest_probe_root/nested/$'weird\nfilename'
printf '%s\n' odd >"$weird_leaf"
HOME="$manifest_probe_home" STARLIGHT_STAGE_JOBS=3 STARLIGHT_STAGE_MANIFEST_POLICY_PROBE=1 \
STARLIGHT_STAGE_MANIFEST_EXTRA_ROOT="$manifest_probe_root" \
STARLIGHT_STAGE_MANIFEST_PROBE_ROOT="$manifest_probe_root" \
STARLIGHT_STAGE_MANIFEST_PROBE_BEFORE="$temp/manifest-before" \
STARLIGHT_STAGE_MANIFEST_PROBE_AFTER="$temp/manifest-after" \
STARLIGHT_STAGE_MANIFEST_PROBE_REPEAT="$temp/manifest-repeat" \
    "$stage_runner" --source "$source" --stage 97 --managed-x11
if cmp -s "$temp/manifest-before" "$temp/manifest-after"; then
    echo 'recursive manifest did not detect a nested leaf mutation' >&2
    exit 1
fi
cmp -s "$temp/manifest-after" "$temp/manifest-repeat"

# sha256sum workers share stdout when xargs runs them in parallel.  A helper
# that deliberately splits each NUL record makes a direct shared pipeline
# nondeterministic: a manifest implementation must collect each worker's
# output separately before the final sort.
parallel_manifest_root="$temp/parallel-manifest-root"
parallel_manifest_tools="$temp/parallel-manifest-tools"
parallel_manifest_home="$temp/parallel-manifest-home"
parallel_manifest_tmp="$temp/parallel-manifest-tmp"
parallel_manifest_path_component=$(printf '%120s' '' | tr ' ' x)
parallel_manifest_dir="$parallel_manifest_root/$parallel_manifest_path_component"
mkdir -p "$parallel_manifest_dir/nested" "$parallel_manifest_tools" "$parallel_manifest_home" "$parallel_manifest_tmp"
printf '%s\n' original >"$parallel_manifest_dir/nested/leaf"
for index in $(seq 1 1025); do
    : >"$parallel_manifest_dir/$(printf '%s-%0120d' "$parallel_manifest_path_component" "$index")"
done
cat >"$parallel_manifest_tools/sha256sum" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
payload=$(mktemp)
trap 'rm -f "$payload"' EXIT
for path in "$@"; do
    [[ $path == -* ]] && continue
    printf 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  %s\0' "$path" >>"$payload"
done
sleep 0.05
cat "$payload"
EOF
chmod +x "$parallel_manifest_tools/sha256sum"
TMPDIR="$parallel_manifest_tmp" PATH="$parallel_manifest_tools:$PATH" HOME="$parallel_manifest_home" STARLIGHT_STAGE_JOBS=3 \
STARLIGHT_STAGE_MANIFEST_POLICY_PROBE=1 \
STARLIGHT_STAGE_MANIFEST_EXTRA_ROOT="$parallel_manifest_root" \
STARLIGHT_STAGE_MANIFEST_PROBE_ROOT="$parallel_manifest_dir" \
STARLIGHT_STAGE_MANIFEST_PROBE_BEFORE="$temp/parallel-manifest-before" \
STARLIGHT_STAGE_MANIFEST_PROBE_AFTER="$temp/parallel-manifest-after" \
STARLIGHT_STAGE_MANIFEST_PROBE_REPEAT="$temp/parallel-manifest-repeat" \
    "$stage_runner" --source "$source" --stage 98 --managed-x11
if ! LC_ALL=C tr '\0' '\n' <"$temp/parallel-manifest-after" | \
        awk -v root="$parallel_manifest_dir" '
            $0 ~ "^a{64}  " root "/" { hashes++ }
            END { exit hashes == 1026 ? 0 : 1 }
        '; then
    echo 'parallel sha256sum output corrupted the protected manifest records' >&2
    exit 1
fi
cmp -s "$temp/parallel-manifest-after" "$temp/parallel-manifest-repeat"
if find "$parallel_manifest_tmp" -mindepth 1 -print -quit | grep -q .; then
    echo 'parallel manifest worker temporary files leaked' >&2
    exit 1
fi

wm_tools="$temp/wm-tools"
mkdir -p "$wm_tools"
printf '#!/bin/bash\nexit 0\n' >"$wm_tools/openbox"
chmod +x "$wm_tools/openbox"
PATH="$wm_tools" STARLIGHT_PLASMA_WM_POLICY_PROBE=1 /bin/bash "$plasma_runner" >"$temp/wm-openbox"
grep -Fx 'plasma-x11-wm=openbox' "$temp/wm-openbox"
rm "$wm_tools/openbox"
printf '#!/bin/bash\nexit 0\n' >"$wm_tools/kwin_x11"
chmod +x "$wm_tools/kwin_x11"
PATH="$wm_tools" STARLIGHT_PLASMA_WM_POLICY_PROBE=1 /bin/bash "$plasma_runner" >"$temp/wm-kwin"
grep -Fx 'plasma-x11-wm=kwin_x11' "$temp/wm-kwin"
rm "$wm_tools/kwin_x11"
if PATH="$wm_tools" STARLIGHT_PLASMA_WM_POLICY_PROBE=1 /bin/bash "$plasma_runner" >"$temp/wm-none" 2>&1; then
    echo 'managed X11 WM policy unexpectedly accepted no supported WM' >&2
    exit 1
fi
grep -Fx 'managed X11 requires openbox or kwin_x11' "$temp/wm-none"

# Exercise the real private Plasma runner, not a helper-only simulation: its
# private root must be completely removed after the managed X11 session exits.
plasma_cleanup_parent="$temp/plasma-cleanup"
mkdir -p "$plasma_cleanup_parent"
TMPDIR="$plasma_cleanup_parent" "$plasma_runner" --source "$repo" \
    "$repo/tests/qml/fixtures/plasma-breeze/kdeglobals" tst_plasmaadapter >/dev/null
if find "$plasma_cleanup_parent" -mindepth 1 -maxdepth 1 -type d \
        -name 'starlight-plasma.*' -print -quit | grep -q .; then
    echo 'private Plasma runner left a private root behind' >&2
    exit 1
fi

rg -q -- '--bind "\$XDG_DATA_HOME" /xdg/data' "$stage_runner"
rg -q -- '--bind "\$flatpak_user_dir" /xdg/data/flatpak' "$stage_runner"
rg -q -- '--bind "\$flatpak_user_dir" "\$builder_user_install_target"' "$stage_runner"
rg -q -- '--tmpfs /tmp' "$stage_runner"
rg -q 'install -d -m 0700 "\$XDG_DATA_HOME/flatpak"' "$stage_runner"
rg -q -- '--disable-download' "$stage_runner"
rg -Fq -- '--jobs="$build_jobs"' "$stage_runner"
if rg -Fq -- '--jobs=2' "$stage_runner"; then
    echo 'Flatpak Builder must inherit STARLIGHT_STAGE_JOBS rather than a fixed low-priority cap' >&2
    exit 1
fi
awk '
    /--bind "\$XDG_DATA_HOME" \/xdg\/data/ { saw_outer_xdg = 1 }
    /--bind "\$flatpak_user_dir" \/xdg\/data\/flatpak/ { if (!saw_outer_xdg) exit 1; saw_outer_flatpak = 1 }
    /--bind "\$flatpak_user_dir" "\$builder_user_install_target"/ { if (!saw_outer_flatpak) exit 1; found = 1 }
    END { exit found ? 0 : 1 }
' "$stage_runner"
rg -Fq '"$HOME/.local/share/flatpak/runtime/org.kde.Sdk/x86_64/6.10/active"' "$stage_runner"
awk '
    /bwrap .*--ro-bind "\$noble_root"/ { saw_bwrap = 1; saw_xdg_dir = 0 }
    saw_bwrap && /--dir \/tmp\/xdg --dir \/tmp\/xdg\/config --dir \/tmp\/xdg\/cache --dir \/tmp\/xdg\/data --dir \/tmp\/xdg\/runtime/ { saw_xdg_dir = 1 }
    saw_bwrap && /--bind "\$XDG_CONFIG_HOME" \/tmp\/xdg\/config/ {
        if (!saw_xdg_dir) exit 1
        checks++
        saw_bwrap = 0
    }
    END { exit checks == 2 ? 0 : 1 }
' "$stage_runner"

artifact_probe_source="$temp/artifact-probe-source"
artifact_probe_home="$temp/artifact-probe-home"
mkdir -p "$artifact_probe_source/artifacts/pagewide-stage-0" \
    "$artifact_probe_source/.superpowers/sdd/2026-07-29-pagewide-desktop-native" \
    "$artifact_probe_home"
printf '%s\n' stale >"$artifact_probe_source/artifacts/pagewide-stage-0/stale.txt"
HOME="$artifact_probe_home" STARLIGHT_STAGE_ARTIFACT_POLICY_PROBE=1 \
    "$stage_runner" --source "$artifact_probe_source" --stage 0 --managed-x11
test -f "$artifact_probe_source/artifacts/pagewide-stage-0/run-metadata.txt"
test -f "$artifact_probe_source/artifacts/pagewide-stage-0/artifact-policy-probe.txt"
test ! -e "$artifact_probe_source/artifacts/pagewide-stage-0/stale.txt"
test "$(find "$artifact_probe_source/artifacts" -maxdepth 1 -type d -name 'pagewide-stage-0.superseded.*' | wc -l)" -eq 1
test "$(find "$artifact_probe_source/artifacts" -maxdepth 1 -type d -name '.pagewide-stage-0.run.*' | wc -l)" -eq 0
if HOME="$artifact_probe_home" STARLIGHT_STAGE_ARTIFACT_POLICY_PROBE=1 \
        STARLIGHT_STAGE_ARTIFACT_MISMATCH_PROBE=1 \
        "$stage_runner" --source "$artifact_probe_source" --stage 0 --managed-x11; then
    echo 'cleanup-time protected mismatch unexpectedly succeeded' >&2
    exit 1
fi
test ! -e "$artifact_probe_source/artifacts/pagewide-stage-0"
mismatch_artifact=$(find "$artifact_probe_source/artifacts" -maxdepth 1 -type d \
    -name 'pagewide-stage-0.failed.*' -print -quit)
test -n "$mismatch_artifact"
grep -Fx 'page-wide stage gate: INVALID (protected path changed during cleanup)' \
    "$mismatch_artifact/result.txt"

for phase_signal in earliest:TERM pre:INT body:TERM; do
    phase=${phase_signal%%:*}
    signal=${phase_signal##*:}
    interrupt_source="$temp/interrupt-$phase"
    interrupt_home="$temp/interrupt-home-$phase"
    interrupt_ready="$temp/interrupt-ready-$phase"
    mkdir -p "$interrupt_source/.superpowers/sdd/2026-07-29-pagewide-desktop-native" "$interrupt_home"
    HOME="$interrupt_home" STARLIGHT_STAGE_INTERRUPT_PROBE="$phase" STARLIGHT_STAGE_INTERRUPT_READY="$interrupt_ready" \
        setsid bash -c 'exec "$@"' bash "$stage_runner" --source "$interrupt_source" --stage 0 --managed-x11 &
    interrupt_pid=$!
    while [[ ! -e $interrupt_ready ]]; do sleep 0.05; done
    kill -"$signal" -- "-$interrupt_pid"
    set +e
    wait "$interrupt_pid"
    interrupt_status=$?
    set -e
    if [[ $interrupt_status -eq 0 ]]; then
        echo "interrupt probe unexpectedly succeeded: $phase/$signal" >&2
        exit 1
    fi
    test ! -e "$interrupt_source/artifacts/pagewide-stage-0"
    test "$(find "$interrupt_source/artifacts" -maxdepth 1 -type d -name '.pagewide-stage-0.run.*' | wc -l)" -eq 0
    interrupt_artifact=$(find "$interrupt_source/artifacts" -maxdepth 1 -type d \
        -name 'pagewide-stage-0.failed.*' -print -quit)
    test -n "$interrupt_artifact"
    grep -Fx 'page-wide stage gate: INVALID (cleanup failed)' "$interrupt_artifact/result.txt"
done

PATH="$tools:$PATH" PAGEWIDE_TEST_RECORD="$record" \
    "$qml_runner" --source "$source" tst_pairing
grep -Fx -- "-input $source/tests/qml/tst_pairing.qml -o -,txt" "$record"
test "$(wc -l <"$record")" -eq 1

rm -f "$record"
if PATH="$tools:$PATH" PAGEWIDE_TEST_RECORD="$record" \
        "$qml_runner" --source "$source" tst_does_not_exist; then
    echo "missing QML selector unexpectedly succeeded" >&2
    exit 1
fi
test ! -e "$record"

# This catches an allowlist that is too broad: only the two reviewed Qt/GCC
# header diagnostics may pass; a project warning remains a hard failure.
allowlisted_warning="$temp/allowlisted-warning.log"
allowlisted_qml_warning="$temp/allowlisted-qml-warning.log"
unexpected_compiler_warning="$temp/unexpected-compiler-warning.log"
unexpected_qml_warning="$temp/unexpected-qml-warning.log"
adjacent_qml_warning="$temp/adjacent-qml-warning.log"
relocated_qml_warning="$temp/relocated-qml-warning.log"
wrong_qml_warning_column="$temp/wrong-qml-warning-column.log"
wrong_qml_warning_file="$temp/wrong-qml-warning-file.log"
wrong_qml_warning_message="$temp/wrong-qml-warning-message.log"
new_qml_warning="$temp/new-qml-warning.log"
printf '%s\n' \
    "/usr/include/qt6/QtCore/qchar.h:48:31: warning: defining ‘QChar’, which previously failed to be complete in a SFINAE context [-Wsfinae-incomplete=]" \
    >"$allowlisted_warning"
printf '%s\n' 'app/gui/Page.cpp:12:7: warning: unexpected project regression' \
    >"$unexpected_compiler_warning"
printf '%s\n' \
    'QWARN  : qml_characterization::StyleBootstrap::test() QQmlExpression: Expression file:///private/app/gui/starlight/SLBusy.qml:16:9 depends on non-bindable properties:' \
    'QWARN  : qml_characterization::StyleBootstrap::test()     QQuickMaterialBusyIndicator_QML_619::color' \
    'QWARN  : qml_characterization::StyleBootstrap::test() QQmlExpression: Expression file:///private/app/gui/starlight/SLBusy.qml:16:9 depends on non-NOTIFYable properties:' \
    'QWARN  : qml_characterization::StyleBootstrap::test()     QQuickMaterialBusyIndicator_QML_620::color' \
    'QWARN  : qml_characterization::StyleBootstrap::test() file:///private/app/gui/main.qml:147:5: QML Settings: The Settings type from Qt.labs.settings is deprecated and will be removed in a future release. Please use the one from QtCore instead.' \
    >"$allowlisted_qml_warning"
printf '%s\n' 'QWARN  : tst_qmlcharacterization::pairing() qml: unexpected runtime warning' \
    >"$unexpected_qml_warning"
printf '%s\n' \
    'QWARN  : qml_characterization::StyleBootstrap::test() QQmlExpression: Expression file:///private/app/gui/starlight/SLBusy.qml:16:10 depends on non-bindable properties:' \
    'QWARN  : qml_characterization::StyleBootstrap::test()     QQuickMaterialBusyIndicator_QML_619::color' \
    >"$adjacent_qml_warning"
printf '%s\n' \
    'QWARN  : qml_characterization::StyleBootstrap::test() file:///private/app/gui/main.qml:153:5: QML Settings: The Settings type from Qt.labs.settings is deprecated and will be removed in a future release. Please use the one from QtCore instead.' \
    >"$relocated_qml_warning"
printf '%s\n' \
    'QWARN  : qml_characterization::StyleBootstrap::test() file:///private/app/gui/main.qml:153:6: QML Settings: The Settings type from Qt.labs.settings is deprecated and will be removed in a future release. Please use the one from QtCore instead.' \
    >"$wrong_qml_warning_column"
printf '%s\n' \
    'QWARN  : qml_characterization::StyleBootstrap::test() file:///private/app/gui/other.qml:153:5: QML Settings: The Settings type from Qt.labs.settings is deprecated and will be removed in a future release. Please use the one from QtCore instead.' \
    >"$wrong_qml_warning_file"
printf '%s\n' \
    'QWARN  : qml_characterization::StyleBootstrap::test() file:///private/app/gui/main.qml:153:5: QML Settings: The Settings type from Qt.labs.settings changed unexpectedly.' \
    >"$wrong_qml_warning_message"
printf '%s\n' \
    'QWARN  : qml_characterization::StyleBootstrap::test() qml: genuinely new runtime warning' \
    >"$new_qml_warning"
"$stage_runner" --check-warning-log "$allowlisted_warning"
"$stage_runner" --check-warning-log "$allowlisted_qml_warning"
"$stage_runner" --check-warning-log "$relocated_qml_warning"
"$stage_runner" --start-at flatpak --check-warning-log "$allowlisted_warning"
rg -Fq 'flatpak-only diagnostic: COMPLETE (full stage result: not evaluated)' "$stage_runner"
test "$(rg -c 'page-wide stage gate: PASS' "$stage_runner")" -eq 1
if "$stage_runner" --check-warning-log "$unexpected_compiler_warning"; then
    echo 'unexpected warning was accepted' >&2
    exit 1
fi
if "$stage_runner" --check-warning-log "$unexpected_qml_warning"; then
    echo 'unexpected QML warning was accepted' >&2
    exit 1
fi
if "$stage_runner" --check-warning-log "$adjacent_qml_warning"; then
    echo 'adjacent QML warning was accepted' >&2
    exit 1
fi
if "$stage_runner" --check-warning-log "$wrong_qml_warning_column"; then
    echo 'wrong QML warning column was accepted' >&2
    exit 1
fi
if "$stage_runner" --check-warning-log "$wrong_qml_warning_file"; then
    echo 'wrong QML warning file was accepted' >&2
    exit 1
fi
if "$stage_runner" --check-warning-log "$wrong_qml_warning_message"; then
    echo 'wrong QML warning message was accepted' >&2
    exit 1
fi
if "$stage_runner" --check-warning-log "$new_qml_warning"; then
    echo 'genuinely new QML warning was accepted' >&2
    exit 1
fi

printf '%s\n' 'page-wide validation wrapper contract: PASS'
