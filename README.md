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

Works against either engine flavor: a source-built O3DE checkout, or an installed/downloaded O3DE
SDK. `3rdParty/FindSDL3.cmake` detects which one it's running under and fetches SDL3 accordingly
(the engine's own package system when available, a plain stock-CMake `FetchContent` fallback
otherwise) — no extra steps needed either way.

## Quick launch (Mac)

    scripts/launch-testbed.sh            # launch the testbed Editor (native backend)
    scripts/launch-testbed.sh -s         # launch with the SDL backend selected
    scripts/launch-testbed.sh -b -a -t   # rebuild, process assets, run tests, then launch
    scripts/launch-testbed.sh --help     # all options

Launched via `open` (launchd-owned). After changing anything under `Assets/`, use `-a` —
never rely on hot-reload with a live Editor (known engine heap-corruption bug in Lua reload).

## Build & test (Mac)

    cd ~/Source/o3de
    cmake --preset mac-ninja -DLY_DISABLE_TEST_MODULES=FALSE -DO3DE_EXTERNAL_SUBDIRS="$HOME/Source/o3de-dualsense-gem"
    cmake --build build/mac_ninja --config profile --target DualSense.Tests -j 10
    ./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests

Engine-only builds require `-DO3DE_EXTERNAL_SUBDIRS` because manifest-registered gems are not auto-included; when building with a project that enables the gem (`-DLY_PROJECTS=...`), the flag is still recommended for the test target.

## Console commands

- `dualsense_rumble <left 0-1> <right 0-1> [slot]` — send vibration to a gamepad slot
- `dualsense_lightbar <r 0-1> <g 0-1> <b 0-1> [slot]` — set light bar color for a gamepad slot
- `dualsense_trigger <l2|r2|both> <mode> [slot]` — send adaptive trigger effect to a gamepad slot; modes: `off`, `feedback`, `weapon`, `vibration`, `autofire`, `slope`, `multifeedback`, `multivibration`
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

3. **Per-shot variation:** For varied recoil per shot within a magazine or magazine load, use `PlayHapticPulse()` from the `OnWeaponTriggerFired` callback. The notification bus fires on the same hardware edge that triggers auto-recoil, allowing gameplay to react with varied intensity pulses (different weapon types, ammo states, etc.). Auto-recoil's own pulse is issued before the notification dispatches, so a handler that calls `PlayHapticPulse` from `OnWeaponTriggerFired` fires afterward and wins the shared transient-pulse slot, overriding the default kick for that shot.

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

function MyWeaponComponent:OnActivate()
    local deviceId = DualSense_GetGamepadDeviceId(0)
    self.fireHandler = DualSenseTriggerNotificationBus.Connect(self, deviceId)
end

function MyWeaponComponent:OnDeactivate()
    self.fireHandler:Disconnect()
end

function MyWeaponComponent:OnWeaponTriggerFired(trigger)
    -- vary intensity based on ammo type, recoil pattern, etc
    local intensity = 0.8
    local deviceId = DualSense_GetGamepadDeviceId(0)
    DualSenseHapticPulseRequestBus.Event.PlayHapticPulse(
        deviceId, intensity, intensity, 0.7)
end
```

**Status:** Auto-recoil and pulse APIs are live on macOS (USB-verified, Bluetooth pending hardware pass). Bluetooth connectivity requires hardware pass before general availability — BT latency adds uncertainty to the trigger-edge synchronization that auto-recoil depends on.

**Demo commands:** Console commands `dualsense_fire_demo` and `dualsense_fire_demo_off` provide one-line activation of a complete firing feel for testing and prototyping; `dualsense_pulse` sends isolated pulses for algorithm validation.

## Test scene

`Assets/DualSenseTest/` ships a keyboard-driven hardware test scene that exercises every scripted
DualSense bus through a real `.inputbindings` asset — the per-binding input path itself (an
Input component reading a bindings asset, rather than console commands), which was a deferred item
from the phase 1 smoke matrix. It supersedes typing console commands by hand for hardware passes:
hold the axis key, tap a mode key, feel it.

**Setup (30 seconds):**

1. Open any level in the Editor (the testbed project's default level is fine).
2. Create an entity.
3. Add an **Input** component (from the StartingPoint Input gem), and set its **Input to event
   bindings** field to `dualsense_test.inputbindings`.
4. Add a **Lua Script** component, and set its script to `DualSenseTest.lua`.
5. Enter game mode (Ctrl+G) with a DualSense connected, and use the keys below. Console output
   (`Debug.Log`) mirrors every action, including the live `OnWeaponTriggerFired` notification feed.

**Key legend:**

| Key(s)  | Action |
|---------|--------|
| `1`     | Trigger effect: feedback |
| `2`     | Trigger effect: weapon |
| `3`     | Trigger effect: vibration (classic buzz, freq 0.6) |
| `4`     | Trigger effect: multi-position feedback |
| `5`     | Trigger effect: slope feedback |
| `6`     | Trigger effect: multi-position vibration |
| `7`     | Trigger effect: autofire (repeated-fire vibration at the current swept frequency) |
| `0`     | Trigger effect: off |
| `Q`     | Axis = LEFT (L2) |
| `E`     | Axis = RIGHT (R2) |
| `W`     | Axis = BOTH (default) |
| `R`     | Rumble for 1s, both motors (coexistence check: does the active trigger effect survive?). **Shares the continuous actuator slot with `Y`/`H` haptic buzz** -- starting a buzz while `R`'s 1s rumble is still running keeps the buzz alive (the deferred rumble-zero is skipped); starting `R` after a buzz replaces it with rumble immediately (last writer wins). |
| `L`     | Cycle light bar color: red -> green -> blue -> white (coexistence check) |
| `T`     | Haptic tap, both sides |
| `Y`     | Haptic buzz, both sides, 1.5s. Shares the continuous actuator slot with `R` rumble -- see `R`'s entry above |
| `U`     | Haptic stop |
| `G`     | Haptic tap, LEFT side only |
| `H`     | Haptic buzz, RIGHT side only, 1.0s. Shares the continuous actuator slot with `R` rumble -- see `R`'s entry above |
| `[`     | Autofire frequency sweep down by 0.02 (re-applies autofire immediately, logs new value) |
| `]`     | Autofire frequency sweep up by 0.02 (re-applies autofire immediately, logs new value) |

The `[` / `]` sweep exists specifically to lock in the repeated-fire feel empirically: press `7` to
start autofire at the assumed 25 Hz mapping (0.098 normalized), then tap `[` / `]` while the trigger
is held to find the value that actually feels like 25 Hz on real hardware, and record it in
`docs/hardware-smoke.md`.

## Backends

DualSense input and haptics on macOS can use one of two backend implementations. The `dualsense_backend`
console variable selects between them at runtime (no restart required).

### Backend selection

**Console command:** `dualsense_backend native|sdl`

**Default:** `native` (per-platform, using GameController.framework on macOS).

**Effective immediately** — the cvar callback tears down the current backend (releasing all held
gamepads, restoring slots to the platform default) and activates the new one. Switching back and
forth mid-session is safe; all gamepad slots are re-claimed when the new backend starts.

### Capability matrix

| Capability | Native | SDL3 (3.4.12) |
|---|---|---|
| Gamepad input (buttons, sticks, triggers) | ✓ | ✓ |
| Rumble (two-motor vibration) | ✓ | ✓ via `SDL_RumbleJoystick` |
| Light bar color control | ✓ | ✓ via `SDL_SetJoystickLED` |
| **Adaptive trigger effects** | ✓ Full: Feedback, Weapon, Vibration, SlopeFeedback, MultiPositionFeedback, MultiPositionVibration | ✓ Full: same modes via raw PS5 HID compiler + `SDL_SendJoystickEffect` |
| Haptic pulse/buzz (transient haptic feedback) | ✓ CoreHaptics (crisp, sharp taps) | ⚠ Rumble emulation (short rumble bursts; lower fidelity than CoreHaptics) |
| Weapon trigger fire detection (`OnWeaponTriggerFired` event) | ✓ | ✗ SDL has no adaptive-trigger status query |
| Autofire / per-shot recoil | ✓ Fires on trigger-break via CoreHaptics | ✗ No fire detection → no callback firing (see Weapon fire detection above) |
| Trigger feel parity vs. native | — | ⚠ Measurable divergence: the native backend uses Apple's normalized API (`GCDualSenseAdaptiveTrigger`); the SDL backend uses the raw PS5 protocol directly. Both feel correct but are not identical. The test scene (`Assets/DualSenseTest/`) allows empirical per-mode comparison. |

### Build flags

**PAL trait** `PAL_TRAIT_DUALSENSE_SDL_BACKEND`:
- `TRUE` on macOS, Windows, Linux (SDL3 build is available on these platforms).
- `FALSE` on Android, iOS (the gem module itself is not compiled on these platforms; the cvar and SDL-backend code do not exist at all).

**Compile definition** `DUALSENSE_SDL_BACKEND_ENABLED`: defined when `PAL_TRAIT_DUALSENSE_SDL_BACKEND` is `TRUE`. Guards all SDL-specific source files (`Code/Source/Clients/Sdl/*.cpp`).

**CMake dependency**: SDL3 (pinned to `release-3.4.12` tag) is fetched and linked as a private dependency of `DualSense.Private.Object` only when the PAL trait is true.

### SDL version pinned

**Version:** `release-3.4.12` (2026-07-01). This is the latest stable SDL3 at the time this backend was implemented. A future update to a newer SDL3 release is straightforward — update the tag/hash in `3rdParty/FindSDL3.cmake` and re-run the configure step.

### macOS A/B testing intent

Both backends can coexist in the same process (GameController.framework and SDL3 managing different physical pads, or the same pad switching ownership at runtime). This allows:

1. **Sequential A/B comparison** on a single pad: switch the `dualsense_backend` cvar between `native` and `sdl` across multiple runs on the same hardware to compare feel/behavior side by side. Alternatively, run two pads concurrently—one via the gem's SDL backend, the second via the stock engine's native GameController.framework—to isolate any cross-writer interference.
2. **Gradual rollout** or **fallback path**: deploy the SDL backend alongside the native one and use the cvar to switch between them in the field if issues arise, without recompiling or restarting.
3. **Platform-validation path**: the native backend is macOS-only (GameController.framework); SDL is cross-platform. Validating the same input/effects contract on SDL before rolling out to Linux/Windows reduces platform-specific regressions.

### Windows / Linux enablement

**Windows (Phase 3c):** `DualSenseSystemImpl::Create()` on Windows now stands up the SDL3 backend
(`Code/Platform/Windows/DualSenseSystemImpl_Windows.cpp`), replacing the passive Unimplemented
stub every earlier phase used. Windows has no native (first-party) DualSense backend the way macOS
has GameController.framework, so it runs SDL3 unconditionally: **both** `dualsense_backend native`
(the default) and `dualsense_backend sdl` bring up the same SDL3 stack on this platform — the cvar
is accepted but informational-only here (a log line at activation makes this explicit). This means
a Windows user who never touches the cvar still gets a working gem out of the box.

**Windows status: UNTESTED as of this commit.** Written and reviewed entirely on macOS, with no
Windows toolchain, SDK, or DualSense-over-Windows hardware pass available to verify it. See
`docs/hardware-smoke.md`'s "Phase 3c — Windows (first-run)" section for what still needs to be
run before trusting this on real Windows hardware, and the Phase 3c report
(`.superpowers/sdd/phase-3c-windows-report.md`) for the specific unverified assumptions (SDL3
fetch/build on Windows, HIDAPI joystick driver behavior, and link-library completeness — see
`Code/Platform/Windows/platform_windows.cmake`). Note: the report's top risk item ("SDL3 fetch +
configure succeeds on Windows at all") specifically flagged `o3de_fetch_content` as untested outside
macOS; that function is only defined for source-built engines and is unconditionally unavailable on
an installed/downloaded O3DE SDK regardless of platform (fixed in `3rdParty/FindSDL3.cmake` — see
git history for `fix(cmake): SDL3 fetch works on installed engines`). The remaining open risk is
narrower: whether the fetch/build itself behaves correctly under an actual Windows toolchain, not
whether the correct CMake machinery is reachable.

Expected to work on Windows (same SDL3 code path already hardware-verified on macOS via
`dualsense_backend sdl`): standard gamepad input, adaptive trigger effects via the raw PS5 HID
compiler, rumble, and light bar color, all through SDL3.

Expected NOT to work on Windows (macOS-native-only, no equivalent wired up on this platform):
CoreHaptics-based HD haptics (buzz/tap fidelity — Windows degrades to rumble emulation, same as
macOS's own SDL backend already does), and weapon-fire detection / autofire
(`OnWeaponTriggerFired` — SDL has no adaptive-trigger status query on any platform).

**Linux:** still uses the passive Unimplemented stub (untouched by Phase 3c) — a separate future
task. Linux can be enabled by flipping the `PAL_TRAIT_DUALSENSE_SDL_BACKEND` flag in
`Code/Platform/Linux/PAL_linux.cmake` (already `TRUE`) and adding a Linux counterpart of
`DualSenseSystemImpl_Windows.cpp`. The underlying C++ (`Code/Source/Clients/Sdl/`) is already
portable — it compiles clean on any platform with SDL3 available.

### Bluetooth prerequisites (macOS, SDL backend)

The SDL backend talks to the DualSense over raw HID (SDL3's HIDAPI-based PS5 driver), which macOS gates
behind the **Input Monitoring** privacy permission (TCC) — a check the native GameController backend
never needs, since it does not use HID directly. Before relying on `dualsense_backend sdl` over
Bluetooth:

- **Input Monitoring permission.** The first time the sdl backend activates (`dualsense_backend sdl`),
  it checks (`IOHIDCheckAccess`) whether this permission has been granted; the check itself never
  triggers a prompt. macOS may prompt the user the first time SDL actually attempts to open a matching
  HID device, or it may silently deny with no prompt at all depending on prior history for this binary —
  don't assume a prompt will appear. If the console logs `DualSense (SDL): Input Monitoring permission
  has not been granted...`, grant it manually (add/enable the Editor or the built app), then relaunch:
  **System Settings > Privacy & Security > Input Monitoring** on macOS 13 (Ventura) and later, or
  **System Preferences > Security & Privacy > Privacy > Input Monitoring** on macOS 11–12 (this gem's
  floor is 11.3).
- **TCC rebinds on rebuild.** macOS's TCC grants are tied to the binary's code signature/identity, not
  just its path. A dev (ad-hoc-signed, frequently-relinked) Editor or launcher binary can silently lose
  its Input Monitoring grant after a rebuild, even though nothing in the privacy settings above changed
  — if a previously-working BT session suddenly logs the not-granted warning after a rebuild, re-grant
  the permission (may require removing and re-adding the entry, not just re-checking it) rather than
  assuming a code regression.
- **Pairing.** Put the DualSense into Bluetooth pairing mode by holding **PS + Create** until the light
  bar starts flashing rapidly, then pair it from macOS's Bluetooth settings. **Unplug the USB cable**
  while testing over Bluetooth — a DualSense that is both plugged in and paired can present as two
  ambiguous connections at once, making it unclear which transport is actually being exercised.
- **GUID first byte is ground truth.** This backend logs each connection's transport via the
  `SDL_GUID`'s first byte (`DualSense (SDL): joystick N transport byte 0x03/0x05 (USB/Bluetooth)`,
  emitted by `DualSenseSdlRuntime::LogTransport`) — `0x03` = USB, `0x05` = Bluetooth. Trust this log
  line over assumptions about which cable/pairing state you think is active; it is the actual value SDL
  derived from the device, not what you intended to test.
