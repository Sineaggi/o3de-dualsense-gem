# DualSense Gem for O3DE

Sony DualSense (PS5) controller support for O3DE. Implemented on macOS (11.3+):
standard gamepad input, rumble haptic feedback, light bar color control, and
adaptive trigger force-feedback effects. Planned: HD haptics, player LEDs, and
extended inputs.

Status: Phase 2 complete — adaptive trigger effects API live on macOS (via
`GCDualSenseAdaptiveTrigger`; USB-verified, Bluetooth pending hardware pass).
Console commands: `dualsense_rumble`, `dualsense_lightbar`, `dualsense_trigger`,
`dualsense_trigger_clear`, `dualsense_debug_swap`, `dualsense_debug_restore`.
See docs/hardware-smoke.md for smoke-test checklist.

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

## Console commands

- `dualsense_rumble <left 0-1> <right 0-1> [slot]` — send vibration to a gamepad slot
- `dualsense_lightbar <r 0-1> <g 0-1> <b 0-1> [slot]` — set light bar color for a gamepad slot
- `dualsense_trigger <l2|r2|both> <mode> [slot]` — send adaptive trigger effect to a gamepad slot; modes: `off`, `feedback`, `weapon`, `vibration`, `slope`, `multifeedback`, `multivibration`
- `dualsense_trigger_clear [slot]` — clear all trigger effects for a gamepad slot
- `dualsense_debug_swap [slot]` — swap a gamepad slot to the debug implementation
- `dualsense_debug_restore [slot]` — restore the platform-default implementation

## Scripting

Adaptive trigger effects are scriptable via the `DualSenseTriggerEffectRequestBus` EBus (module: `dualsense`).

**Script reflection names:**
- Class: `DualSenseTriggerEffect` (properties: `mode`, `startPosition`, `endPosition`, `strength`, `endStrength`, `frequency`, `positionalValues`)
- Trigger enum: `DualSenseTrigger_L2`, `DualSenseTrigger_R2`, `DualSenseTrigger_Both`
- Effect mode enum: `DualSenseTriggerEffectMode_Off`, `DualSenseTriggerEffectMode_Feedback`, `DualSenseTriggerEffectMode_Weapon`, `DualSenseTriggerEffectMode_Vibration`, `DualSenseTriggerEffectMode_MultiPositionFeedback`, `DualSenseTriggerEffectMode_MultiPositionVibration`, `DualSenseTriggerEffectMode_SlopeFeedback`
- Bus: `DualSenseTriggerEffectRequestBus` with methods `SetTriggerEffect(Trigger, DualSenseTriggerEffect)` and `ClearTriggerEffects()`

**Lua example:**
```lua
-- Create a weapon-mode trigger effect for the left trigger (gamepad slot 0)
local effect = DualSenseTriggerEffect()
effect.mode = DualSenseTriggerEffectMode_Weapon
effect.startPosition = 0.2
effect.strength = 0.9
local deviceId = InputDeviceId(InputDeviceGamepad.name, 0)
DualSenseTriggerEffectRequestBus.Event:SetTriggerEffect(deviceId, DualSenseTrigger_L2, effect)
```
