# DualSense Gem for O3DE

Sony DualSense (PS5) controller support for O3DE. Implemented on macOS (11.3+):
standard gamepad input, rumble haptic feedback, light bar color control, and
adaptive trigger force-feedback effects. Planned: HD haptics, player LEDs, and
extended inputs.

Status: Phase 2.5 complete — adaptive trigger effects + hardware-synchronized auto-recoil API live on macOS (via
`GCDualSenseAdaptiveTrigger` + haptic engine; USB-verified, Bluetooth pending hardware pass).
Console commands: `dualsense_rumble`, `dualsense_lightbar`, `dualsense_trigger`,
`dualsense_trigger_clear`, `dualsense_debug_swap`, `dualsense_debug_restore`,
`dualsense_fire_demo`, `dualsense_fire_demo_off`, `dualsense_pulse`.
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
- `dualsense_fire_demo [l2|r2|both] [slot]` — activate demo fire feel (weapon trigger effect + auto-recoil) for isolated testing; defaults to r2, slot 0
- `dualsense_fire_demo_off [slot]` — deactivate demo fire feel and disable auto-recoil
- `dualsense_pulse <left 0-1> <right 0-1> [sharpness] [slot]` — send a sharp haptic pulse transient for isolated kick testing; sharpness defaults to 0.5

## Scripting

Adaptive trigger effects are scriptable via the `DualSenseTriggerEffectRequestBus` EBus (module: `dualsense`).

**Script reflection names:**
- Class: `DualSenseTriggerEffect` (properties: `mode`, `startPosition`, `endPosition`, `strength`, `endStrength`, `frequency`, `positionalValues`)
- Trigger enum: `DualSenseTrigger_L2`, `DualSenseTrigger_R2`, `DualSenseTrigger_Both`
- Effect mode enum: `DualSenseTriggerEffectMode_Off`, `DualSenseTriggerEffectMode_Feedback`, `DualSenseTriggerEffectMode_Weapon`, `DualSenseTriggerEffectMode_Vibration`, `DualSenseTriggerEffectMode_MultiPositionFeedback`, `DualSenseTriggerEffectMode_MultiPositionVibration`, `DualSenseTriggerEffectMode_SlopeFeedback`
- Bus: `DualSenseTriggerEffectRequestBus` with methods `SetTriggerEffect(Trigger, DualSenseTriggerEffect)` and `ClearTriggerEffects()`
- Helper: `DualSense_GetGamepadDeviceId(slotIndex)` — returns the `InputDeviceId` for a gamepad slot (0-3), for use as the bus address below. Use this instead of constructing `InputDeviceId` directly in Lua: doing so currently returns a default-constructed (i.e. wrong) id, because `AzFramework::InputDeviceId::Reflect()` is missing a `ConstructorOverride` that Lua's binding needs to dispatch to the real 2-argument constructor (an upstream engine reflection gap, not specific to this gem).

**Lua example:**
```lua
-- Create a weapon-mode trigger effect for the left trigger (gamepad slot 0)
local effect = DualSenseTriggerEffect()
effect.mode = DualSenseTriggerEffectMode_Weapon
effect.startPosition = 0.2
effect.strength = 0.9
local deviceId = DualSense_GetGamepadDeviceId(0)
DualSenseTriggerEffectRequestBus.Event.SetTriggerEffect(deviceId, DualSenseTrigger_L2, effect)
```

## Recoil / firing feel

Weapon fire haptic feedback combines three components, all scriptable via reflected buses:

1. **Trigger resistance:** Weapon-mode adaptive trigger effect defines the pull zone and break-point feel (set via `DualSenseTriggerEffectRequestBus::SetTriggerEffect`).

2. **Auto-recoil:** Hardware-synchronized transient kick on trigger-break for Weapon-mode effects (enable via `DualSenseHapticPulseRequestBus::SetAutoRecoil(trigger, enabled, intensity, sharpness)`). When enabled, the controller automatically delivers a haptic pulse at the precise moment the trigger's Weapon-mode activation edge fires, without any gameplay script latency.

3. **Per-shot variation:** For varied recoil per shot within a magazine or magazine load, use `PlayHapticPulse()` from the `OnWeaponTriggerFired` callback. The notification bus fires on the same hardware edge that triggers auto-recoil, allowing gameplay to react with varied intensity pulses (different weapon types, ammo states, etc.).

**Script names for auto-recoil and pulse:**
- Trigger enum: `DualSenseTrigger_L2`, `DualSenseTrigger_R2`, `DualSenseTrigger_Both`
- Bus: `DualSenseHapticPulseRequestBus` with methods `PlayHapticPulse(LeftIntensity, RightIntensity, Sharpness)` and `SetAutoRecoil(Trigger, Enabled, Intensity, Sharpness)`
- Notification: `DualSenseTriggerNotificationBus` — implement `OnWeaponTriggerFired(Trigger)` to react per-shot
- Helper: `DualSense_GetGamepadDeviceId(slotIndex)` — use to address both request and notification buses

**Lua example:**
```lua
-- Enable auto-recoil for the right trigger at 0.9 intensity, 0.7 sharpness
local deviceId = DualSense_GetGamepadDeviceId(0)
DualSenseHapticPulseRequestBus.Event.SetAutoRecoil(
    deviceId, DualSenseTrigger_R2, true, 0.9, 0.7)

-- Implement OnWeaponTriggerFired to send per-shot variable kicks
local MyWeaponComponent = {}
function MyWeaponComponent:OnWeaponTriggerFired(trigger)
    -- vary intensity based on ammo type, recoil pattern, etc
    local intensity = 0.8
    DualSenseHapticPulseRequestBus.Event.PlayHapticPulse(
        deviceId, intensity, intensity, 0.7)
end
```

**Status:** Auto-recoil and pulse APIs are live on macOS (USB-verified, Bluetooth pending hardware pass). Bluetooth connectivity requires hardware pass before general availability — BT latency adds uncertainty to the trigger-edge synchronization that auto-recoil depends on.

**Demo commands:** Console commands `dualsense_fire_demo` and `dualsense_fire_demo_off` provide one-line activation of a complete firing feel for testing and prototyping; `dualsense_pulse` sends isolated pulses for algorithm validation.
