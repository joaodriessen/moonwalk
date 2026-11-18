#!/usr/bin/env bash
# Compatibility entry point: cached builds, isolated test state.
set -euo pipefail
source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
if [[ ${1:-} == --source ]]; then
    source_dir=$(cd "${2:?--source requires a directory}" && pwd -P)
    shift 2
fi
if (( $# == 0 )); then
    echo 'Select a QML test; use --help for usage.' >&2
    exit 2
fi
if [[ ${1:-} == --help || ${1:-} == -h ]]; then
    echo 'Usage: run-private-qml-test.sh [--source DIR] tst_name [tst_name ...]'
    echo 'Uses ./dev test; see ./dev --help and docs/development.md.'
    exit 0
fi
exec "$source_dir/dev" test "$@" --style "${QT_QUICK_CONTROLS_STYLE:-Material}"
