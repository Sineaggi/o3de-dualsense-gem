#!/bin/bash
# Launch the DualSense testbed Editor for hardware testing.
#
# Usage: scripts/launch-testbed.sh [options]
#   -b, --build     Rebuild Editor + gem targets first (incremental; minutes)
#   -a, --assets    Run AssetProcessorBatch to completion before launching
#                   (do this after changing anything in Assets/ — NEVER rely on
#                   hot-reload with a live Editor: engine Lua hot-reload can
#                   corrupt the heap, see upstream candidate #3 in the ledger)
#   -s, --sdl       Launch with dualsense_backend=sdl (SDL3 backend active
#                   from startup; default is the native GameController backend)
#   -t, --tests     Run the gem unit suite first and abort launch on failure
#   -k, --keep      Do NOT kill an already-running Editor (default kills it)
#   -h, --help      This text
#
# The Editor is launched via `open` so launchd owns it (survives this shell).
# Console log: ~/Source/dualsense-testbed/user/log/Editor.log
# Test-scene key legend: see README "Test scene" section.

set -euo pipefail

ENGINE="$HOME/Source/o3de"
BUILD="$ENGINE/build/mac_ninja"
BIN="$BUILD/bin/profile"
PROJECT="$HOME/Source/dualsense-testbed"

DO_BUILD=0; DO_ASSETS=0; DO_SDL=0; DO_TESTS=0; DO_KILL=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    -b|--build)  DO_BUILD=1 ;;
    -a|--assets) DO_ASSETS=1 ;;
    -s|--sdl)    DO_SDL=1 ;;
    -t|--tests)  DO_TESTS=1 ;;
    -k|--keep)   DO_KILL=0 ;;
    -h|--help)   sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $1 (try --help)"; exit 2 ;;
  esac
  shift
done

if [[ $DO_BUILD -eq 1 ]]; then
  echo "==> Building Editor + gem (profile)..."
  # fixup_bundle race: a failed 'not a valid bundle' converges on retry
  cmake --build "$BUILD" --config profile --target DualSense DualSense.Tests Editor -j 10 ||
  cmake --build "$BUILD" --config profile --target DualSense DualSense.Tests Editor -j 10
fi

if [[ $DO_TESTS -eq 1 ]]; then
  echo "==> Running gem unit suite..."
  "$BIN/AzTestRunner" "$BIN/libDualSense.Tests.dylib" AzRunUnitTests | tail -3
fi

if [[ $DO_ASSETS -eq 1 ]]; then
  echo "==> Processing assets to completion (AssetProcessorBatch)..."
  "$BIN/AssetProcessorBatch.app/Contents/MacOS/AssetProcessorBatch" \
    --project-path="$PROJECT" 2>&1 | tail -3
fi

if [[ $DO_KILL -eq 1 ]] && pgrep -x Editor >/dev/null 2>&1; then
  echo "==> Closing running Editor..."
  pkill -x Editor || true
  sleep 2
fi

ARGS=(--project-path "$PROJECT")
if [[ $DO_SDL -eq 1 ]]; then
  ARGS+=(+dualsense_backend sdl)
  echo "==> SDL backend selected (watch the log for the transport GUID: 03=USB, 05=Bluetooth,"
  echo "    and for an Input Monitoring permission warning on first use)"
fi

echo "==> Launching Editor (launchd-owned)..."
open "$BIN/Editor.app" --args "${ARGS[@]}"

for _ in $(seq 1 15); do
  sleep 2
  if pgrep -x Editor >/dev/null 2>&1; then
    echo "==> Editor is up. Log: $PROJECT/user/log/Editor.log"
    echo "    Tail it with: tail -f $PROJECT/user/log/Editor.log"
    exit 0
  fi
done
echo "!! Editor did not appear within 30s — check $PROJECT/user/log/Editor.log" >&2
exit 1
