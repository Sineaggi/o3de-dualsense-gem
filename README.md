# DualSense Gem for O3DE

Sony DualSense (PS5) controller support for O3DE: adaptive trigger force-feedback
effects, haptic feedback, light bar, player LEDs, and (planned) extended inputs.

Status: Phase 0 (swap architecture proven). See `docs/superpowers/specs/` for the
design and `docs/superpowers/plans/` for the implementation plan.

## Setup

1. Register the gem: `scripts/o3de.sh register -gp <path-to-this-repo>` (from your engine root)
2. Enable it for a project: `scripts/o3de.sh enable-gem -gn DualSense -pp <project-path>`
3. Configure + build your project as usual.

## Build & test (Mac)

    cd ~/Source/o3de
    cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE
    cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
    ./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests

## Debug console commands

- `dualsense_debug_swap [slot]` — swap a gamepad slot to the debug implementation
- `dualsense_debug_restore [slot]` — restore the platform-default implementation
