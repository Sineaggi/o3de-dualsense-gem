# DualSense Gem — Design

**Date:** 2026-07-26
**Status:** Approved direction; implementation not started
**Engine:** O3DE `development` (researched at commit `f6e9f8c3d1`)
**Repo:** `~/Source/o3de-dualsense-gem` (external gem, registered via `external_subdirectories`)

## 1. Goal

An O3DE gem that gives Sony DualSense (PS5) controllers first-class support on desktop
platforms, adding the two features the hardware is famous for and the engine has no API
for: **adaptive trigger force-feedback effects** and **haptic feedback** — plus the light
bar, player LEDs, and (later) extended inputs (touchpad, gyro, mute button).

Requires **zero engine modifications**: every hook used is a public extension point.

### Non-goals

- Console (PS5 platform) support — that lives in Sony's NDA'd `restricted/` platforms.
- DualShock 4 support (protocol is similar; a possible follow-up, out of scope).
- PCM audio-driven haptics on Windows/Linux in the core phases (explicit stretch, Phase 6).
- Server variants — this is a client-only gem (`.Clients`/`.Unified`/`.Tools` aliases only).

## 2. Engine capability baseline (research summary)

What O3DE provides today, per the 2026-07-26 research pass:

- **Output surface is 2-float rumble only.** `InputHapticFeedbackRequestBus::SetVibration(l, r)`
  is the entire motor API. `InputLightBarRequestBus` exists but has zero implementations in
  the public repo. Neither bus is reflected to script. No trigger-effect or waveform API exists.
- **Platform backends:** Windows = XInput (a bare DualSense is invisible); Mac/iOS =
  GameController.framework circa-2013 API (`GCExtendedGamepad` accessors; `SetVibration` is an
  empty function); Linux = libevdev with the device opened `O_RDONLY` (input works via the
  kernel `hid-playstation` driver; all output blocked). No code anywhere reads VID/PID.
- **No HID transport exists** — no hidapi/SDL/IOKit-HID/libusb in the engine or its 3rdParty
  package set. The gem brings its own.
- **Extension points (all public, all with in-tree precedent):**
  - Per-slot backend swap: `InputDeviceImplementationRequest<InputDeviceGamepad>::Bus::Event(
    deviceId, &SetCustomImplementation, &factory)` — precedent `Gems/BarrierInput`
    (`BarrierInputSystemComponent.cpp:119-152`).
  - `InputDeviceGamepad::Implementation` pimpl with `RawGamepadState` +
    `ProcessRawGamepadState()` driving all 32 standard channels
    (`AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h`).
  - Self-registering devices: constructing an `AzFramework::InputDevice` subclass connects it
    to tick + enumeration buses — precedent `Gems/VirtualGamepad`, `Gems/Gestures`. New
    devices/channels auto-populate StartingPointInput's editor dropdowns.
  - 3rdParty from source: `o3de_fetch_content` find-module pattern — precedent
    `Gems/MiniAudio/3rdParty/Findminiaudio.cmake` (+ `Installer/` variant).
  - Per-platform pimpl in a gem: linker-selected `Implementation::Create()` — precedent
    `Gems/SaveData`; framework linking via `platform_<p>.cmake` — precedent `Gems/Microphone`.
- **Known engine quirks to respect:**
  - `SetCustomImplementation(nullptr)` does *not* clear an implementation (handler only swaps
    on non-null). Restore the platform default by re-passing
    `AZ::Interface<InputDeviceGamepad::ImplementationFactory>::Get()` (BarrierInput's approach).
  - If no gamepad `ImplementationFactory` is registered, `InputSystemComponent` creates zero
    gamepad devices; after changing factories call
    `InputSystemRequestBus::Broadcast(&RecreateEnabledInputDevices)`.
  - Windows gamepad ticking is focus-gated (`::GetFocus()`); the gem matches that behavior.
  - `InputChannelId` equality is crc32-only — new channel names must be globally unique.

## 3. DualSense hardware facts that drive the design

- All non-input features ride one HID output report: USB report `0x02`, Bluetooth report
  `0x31` (78 bytes; trailing CRC32 seeded over `0xA2` + bytes 0–73). A shared 47-byte effects
  block carries rumble emulation, two 11-byte trigger-effect blocks, lightbar RGB, player
  LEDs, mic LED. BT requires a one-time feature-report read (`0x05`, which also yields IMU
  calibration) to unlock full "enhanced mode" reports.
- Adaptive trigger block = 1 mode byte + 10 parameter bytes. Stable, official effect set
  (identical in Sony's libScePad and Apple's API): **Off, Feedback, Weapon, Vibration,
  MultiPositionFeedback, MultiPositionVibration, SlopeFeedback**. 10 trigger-travel zones,
  16-bit zone mask, 3-bit-per-zone strengths, frequency byte for vibration modes.
  Unofficial modes (Bow/Galloping/Machine, legacy 0x01/0x02/0x06) are firmware leftovers —
  excluded from the public API.
- **Trigger effects work over USB and Bluetooth.** True voice-coil haptics are audio-driven
  (4-channel USB audio device, channels 3–4 = actuator PCM) and USB-only on PC; the HID
  rumble-emulation bytes are the universal fallback.
- Firmware quirk: improved-rumble flag (COMPATIBLE_VIBRATION2) threshold differs between SDL
  (fw 2.24) and the Linux kernel (2.21) — validate on hardware, prefer SDL's behavior.
- Detection: VID `0x054C`, PID `0x0CE6` (DualSense) / `0x0DF2` (DualSense Edge). On Apple
  platforms detection is `isKindOfClass:[GCDualSenseGamepad class]` instead.
- Apple maps everything natively: `GCDualSenseGamepad` (macOS 11.3+),
  `GCDualSenseAdaptiveTrigger` (setModeFeedback/Weapon/Vibration/SlopeFeedback — slope needs
  macOS 12.3+), `GCDeviceHaptics` → CoreHaptics engines per locality (works over BT),
  `GCDeviceLight` for the lightbar, `GCMotion` for IMU.

Protocol references: SDL3 `SDL_hidapi_ps5.c`, Linux `hid-playstation.c`, nondebug/dualsense,
flok/pydualsense, Nielk1's trigger-effect factory gist.

## 4. Architecture

### 4.1 Identity: hybrid

**Core (Phases 0–4):** the DualSense masquerades as the standard `gamepad` device. A
`DualSenseSystemComponent` monitors for DualSense hardware and swaps the affected gamepad
slot's implementation via `SetCustomImplementation`, restoring the platform default on
disconnect. Everything existing — `.inputbindings`, `SetVibration` callers, the orphaned
light-bar bus — works unmodified.

**Extended (Phase 5):** a companion `InputDeviceId("dualsense")` device (VirtualGamepad
pattern) carries channels the fixed 32-channel gamepad set cannot: touchpad position/click,
gyro/accelerometer (opt-in via the `InputMotionSensorRequestBus` pattern), mute/PS buttons.
Channel names: `dualsense_touchpad_*`, `dualsense_gyro`, `dualsense_button_mute`, … —
prefixed with the device name to guarantee crc32 uniqueness.

### 4.2 Components

```
DualSenseSystemComponent (Clients)
├─ owns DualSenseDeviceMonitor          — platform detection, hotplug, slot bookkeeping
├─ owns DualSenseGamepadImplFactory     — InputDeviceGamepad::ImplementationFactory
│    └─ creates InputDeviceGamepadDualSense : InputDeviceGamepad::Implementation
│         ├─ input → RawGamepadState → ProcessRawGamepadState() (32 std channels)
│         ├─ SetVibration / SetLightBarColor overrides
│         └─ owns DualSenseTransport (pimpl, per-platform: see 4.4)
├─ handles DualSenseTriggerEffectRequestBus (per InputDeviceId)
├─ handles DualSenseRequestBus (capability queries, LEDs, connection type)
└─ Phase 5: owns InputDeviceDualSense (companion device)
```

Service declarations: requires `InputSystemService`, dependent on
`NativeUIInputSystemService` (platform factories must register first).

### 4.3 Public API (gem-owned buses, frozen in Phase 2)

`DualSenseTriggerEffectRequestBus` — EBus traits copied from `InputLightBarRequestBus`
(`AddressPolicy::ById`, `BusIdType = InputDeviceId`, `HandlerPolicy::Single`):

```cpp
struct TriggerEffect            // serialize- and behavior-reflected
{
    enum class Mode { Off, Feedback, Weapon, Vibration,
                      MultiPositionFeedback, MultiPositionVibration, SlopeFeedback };
    Mode  m_mode;
    float m_startPosition;      // all positions/strengths normalized 0..1
    float m_endPosition;
    float m_strength;           // or amplitude for vibration modes
    float m_endStrength;        // slope mode
    float m_frequency;          // vibration modes only; normalized [0,1] (Apple API is normalized; HID compiler maps to raw byte)
    AZStd::array<float, 10> m_positionalValues; // multi-position modes
};

enum class Trigger { L2, R2, Both };
virtual void SetTriggerEffect(Trigger trigger, const TriggerEffect&) = 0;
virtual void ClearTriggerEffects() = 0;
```

This one struct compiles to the 11-byte HID block (Windows/Linux) and to
`GCDualSenseAdaptiveTrigger` calls (Apple) — the mapping is 1:1 by construction.

`DualSenseRequestBus` (same traits): `IsDualSense(deviceId)`, `GetConnectionType()`
(USB/Bluetooth/None — lets gameplay degrade gracefully), `SetPlayerLeds(mask)`,
`SetMicLed(mode)`.

Both buses + `TriggerEffect` are reflected to BehaviorContext (MiniAudio reflection style:
named `BehaviorParameterOverrides`, `Module`/`Category`/`Scope` attributes) for Script
Canvas and Lua. Channel-name constants reflected à la `InputDeviceGamepad::Reflect`.

Settings: `Registry/dualsense.setreg` under `/O3DE/DualSense/...` deserialized into a
reflected settings struct (ScriptAutomation pattern); live toggles as CVars
(`dualsense_enable`, `dualsense_triggerEffectsEnabled`) per the BarrierInput pattern.

### 4.4 Platform backends (SaveData pimpl pattern)

| Platform | Transport | Notes |
|---|---|---|
| **Mac** (first) | GameController.framework: `GCDualSenseGamepad` input, `GCDualSenseAdaptiveTrigger` triggers, CoreHaptics via `GCDeviceHaptics` for `SetVibration` (continuous events on left/right handle engines), `GCDeviceLight` lightbar | No raw HID, works over BT, `@available` guards (floor: macOS 11.3; slope effect 12.3+). Frameworks linked in `platform_mac.cmake` (Microphone pattern) |
| **Windows** | hidapi via `o3de_fetch_content` (BSD-3 option of its triple license); raw `0x02`/`0x31` reports; VID/PID enumeration + hotplug | XInput never claims the pad, so the swapped impl owns a slot outright. Detect/document Steam Input contention |
| **Linux** | Same hidapi protocol code over hidraw | Takes the device over from `hid-playstation` outputs (as SDL does). Ship a documented udev rule |
| Others | `Common/Unimplemented` stub | Gem gated by `PAL_TRAIT_DUALSENSE_SUPPORTED` in `PAL_<p>.cmake` |

The Windows/Linux protocol layer (report parsing, effects-block builder,
`TriggerEffect` → 11-byte compiler, CRC32, BT enhanced-mode unlock) is shared code under
`Source/Clients/Protocol/`, unit-testable without hardware.

## 5. Phases

Cut platform-first along risk boundaries; every phase ends hardware-demoable.

- **Phase 0 — Skeleton + swap proof.** Gem registered & building; dummy implementation
  swapped in/out via `SetCustomImplementation` without disturbing real pads.
- **Phase 1 — Mac core.** Detection, 32 standard channels, rumble via CoreHaptics, lightbar,
  hotplug with clean restore. *Done =* a DualSense drives existing `.inputbindings` on Mac
  with working rumble — already beats stock O3DE.
- **Phase 2 — Trigger effect API.** `TriggerEffect` struct + buses, Mac backend via
  `GCDualSenseAdaptiveTrigger`, BehaviorContext reflection, Script Canvas demo. API frozen
  after this phase.
- **Phase 3 — Windows backend.** hidapi fetch, enumeration, input parsing, output builder,
  trigger compiler, BT + CRC32. Splits into 3a (USB input + rumble) / 3b (triggers + BT) if
  needed. *Done =* parity with Mac.
- **Phase 4 — Linux backend.** Protocol reuse over hidraw; udev docs.
- **Phase 5 — Extended inputs.** Companion `dualsense` device: touchpad, IMU (opt-in), mute/PS.
- **Phase 6 — PCM haptics (stretch).** `CHHapticPattern` playback API on Mac; 4-channel
  WASAPI endpoint on Windows (USB only).

## 6. Testing

- **Protocol unit tests (no hardware):** trigger-effect compiler output vs known-good byte
  sequences from pydualsense/SDL; CRC32 vectors; report parsing from captured input reports.
- **Swap lifecycle tests:** factory swap/restore against a mock implementation
  (`PAL_TRAIT_DUALSENSE_TEST_SUPPORTED`).
- **Hardware smoke checklist per phase:** documented manual pass (connect USB, connect BT,
  hotplug during play, rumble, each trigger mode, lightbar, sleep/wake).

## 7. Risks

- **Bluetooth ceiling (PC):** triggers/rumble yes, PCM haptics no → `GetConnectionType()`
  exists so callers can degrade; docs state it plainly.
- **Device ownership conflicts:** Steam Input (Windows/Linux) and the Linux kernel driver's
  own output writes → take over the hidraw device wholesale (SDL's approach); detect Steam
  where feasible and document.
- **Firmware variance:** rumble-flag threshold 2.21 vs 2.24; DualSense Edge extras → follow
  SDL's current behavior; Edge treated as plain DualSense initially.
- **Engine quirk:** null-factory swap doesn't clear → always restore via the platform
  default factory; if that breaks, small upstream PR to o3de (gem stays external).
- **macOS floor:** `GCDualSenseGamepad` needs 11.3+ → acceptable in 2026; `@available`
  guards keep older OSes at stock behavior.

## 8. References

- Engine research (file-level detail): conversation research pass 2026-07-26 over
  `Code/Framework/AzFramework/AzFramework/Input/**`, `Gems/{BarrierInput,VirtualGamepad,
  Gestures,StartingPointInput,SaveData,MiniAudio,Microphone}`.
- Protocol: github.com/libsdl-org/SDL `src/joystick/hidapi/SDL_hidapi_ps5.c`;
  torvalds/linux `drivers/hid/hid-playstation.c`; github.com/nondebug/dualsense;
  github.com/flok/pydualsense; Nielk1 trigger-effect gist
  (gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db).
- Apple: developer.apple.com/documentation/gamecontroller/gcdualsenseadaptivetrigger,
  gcdevicehaptics, gcdevicelight.
