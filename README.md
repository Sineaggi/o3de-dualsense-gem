# DualSense Gem for O3DE

Sony DualSense (PS5) controller support for O3DE. Implemented today (macOS):
standard gamepad input, rumble haptic feedback, and light bar color control.
Planned: adaptive trigger force-feedback effects, HD haptics, player LEDs, and
extended inputs.

Status: Phase 1 complete — on macOS (11.3+) a DualSense works as the standard
gamepad device with rumble (CoreHaptics) and light bar. Next: trigger-effect API
(phase 2). Console commands: `dualsense_rumble`, `dualsense_lightbar`,
`dualsense_debug_swap`, `dualsense_debug_restore`. Hardware verification pending — see docs/hardware-smoke.md.

## Setup

1. Register the gem: `scripts/o3de.sh register -gp <path-to-this-repo>` (from your engine root)
2. Enable it for a project: `scripts/o3de.sh enable-gem -gn DualSense -pp <project-path>`
3. Configure + build your project as usual.

## Build & test (Mac)

    cd ~/Source/o3de
    cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE -DO3DE_EXTERNAL_SUBDIRS="$HOME/Source/o3de-dualsense-gem"
    cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
    ./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests

Engine-only builds require `-DO3DE_EXTERNAL_SUBDIRS` because manifest-registered gems are not auto-included; when building with a project that enables the gem (`-DLY_PROJECTS=...`), the flag is still recommended for the test target.

## Debug console commands

- `dualsense_rumble <left 0-1> <right 0-1> [slot]` — send vibration to a gamepad slot
- `dualsense_lightbar <r 0-1> <g 0-1> <b 0-1> [slot]` — set light bar color for a gamepad slot
- `dualsense_debug_swap [slot]` — swap a gamepad slot to the debug implementation
- `dualsense_debug_restore [slot]` — restore the platform-default implementation
