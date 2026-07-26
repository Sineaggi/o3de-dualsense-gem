# DualSense Phase 2 — Adaptive Trigger Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A frozen, scriptable trigger-effect API (`TriggerEffect` + request bus) with a working Mac backend via `GCDualSenseAdaptiveTrigger`, hardware-smokable through console commands.

**Architecture:** A gem-owned EBus addressed by `InputDeviceId` (same traits as the engine's light-bar bus) carries a reflected `TriggerEffect` struct modeled on the libScePad/Apple effect set. The Mac gamepad implementation (`InputDeviceGamepadDualSenseMac`) connects as the per-slot handler and applies effects through `GCDualSenseGamepad`'s `GCDualSenseAdaptiveTrigger` objects. Pure-C++ clamping/degradation logic is unit-tested; only the final ObjC application layer is hardware-coupled. Spec: `docs/superpowers/specs/2026-07-26-dualsense-gem-design.md` §4.3/§5 — with one amendment made by this plan: `m_frequencyHz` becomes normalized `m_frequency` (Apple's API takes normalized [0-1]; the phase-3 HID compiler maps to the raw byte).

**Tech Stack:** C++17 (AZStd/EBus/SerializeContext/BehaviorContext), Objective-C++ (MRC — no ARC) for the Mac layer, GameController.framework, AzTest.

## Global Constraints

- **Zero engine modifications.** All work in `~/Source/o3de-dualsense-gem`, branch `feature/phase-2-triggers` (create from `main` at start).
- **Build (from `~/Source/o3de`):** `cmake --build build/mac_ninja --config profile --target DualSense DualSense.Tests -j 10` (add `Editor` to the target list on any task that changes runtime behavior the smoke test needs). Retry once on "fixup_bundle: not a valid bundle". If targets go unknown after a reconfigure, the cache needs `-DO3DE_EXTERNAL_SUBDIRS="$HOME/Source/o3de-dualsense-gem"` (and the Editor needs `-DLY_PROJECTS` to include `$HOME/Source/dualsense-testbed` — verify with `grep ^LY_PROJECTS build/mac_ninja/CMakeCache.txt`, do not trust prior configures).
- **Tests:** `./build/mac_ninja/bin/profile/AzTestRunner $PWD/build/mac_ninja/bin/profile/libDualSense.Tests.dylib AzRunUnitTests`. Suite currently 16/16 — must stay green plus new tests. Never commit red. Test evidence in reports must be ACTUAL unedited output.
- **Apple API facts (verified against the local SDK's `GCDualSenseAdaptiveTrigger.h` 2026-07-26 — trust these, but re-verify if a call fails to compile):**
  - macOS 11.3+: `setModeOff`, `setModeFeedbackWithStartPosition:resistiveStrength:`, `setModeWeaponWithStartPosition:endPosition:resistiveStrength:`, `setModeVibrationWithStartPosition:amplitude:frequency:`
  - macOS 12.3+: `setModeSlopeFeedbackWithStartPosition:endPosition:startStrength:endStrength:`, `setModeFeedbackWithResistiveStrengths:` (`GCDualSenseAdaptiveTriggerPositionalResistiveStrengths`, `float values[10]`), `setModeVibrationWithAmplitudes:frequency:` (`GCDualSenseAdaptiveTriggerPositionalAmplitudes`), `GCDualSenseAdaptiveTriggerDiscretePositionCount == 10`
  - ALL parameters normalized [0-1].
- **macOS floors:** 11.3 baseline (matches existing guards). 12.3-only modes must degrade deterministically below 12.3 (Task 2 defines the exact degradation) — never silently no-op, log once per degradation at debug level.
- **MRC discipline (`.mm` files are non-ARC):** no `__bridge_retained`/`__bridge_transfer`; follow the existing idioms in `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.mm`. Every GameController/adaptive-trigger ObjC call that could hit a dead controller gets its own `@try/@catch (NSException*)` (hardware-proven necessity — see `DualSenseHapticsMac.mm` comments).
- **Threading:** bus events arrive on the main thread (game thread). Do not add cross-thread machinery; the notification-marshaling in `DualSenseSystemImpl_Mac.mm` already guarantees monitor work is main-thread.
- **Commits** end with `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`. Style `<type>: <summary>`.
- **Channel-id / naming:** new script-visible names use the `DualSense` module, category "DualSense".

---

### Task 1: Public API — `TriggerEffect`, enums, request bus, serialize reflection

**Files:**
- Create: `Code/Include/DualSense/DualSenseTriggerEffects.h`
- Modify: `Code/Include/DualSense/DualSenseTypeIds.h` (add UUIDs)
- Modify: `Code/Source/Clients/DualSenseSystemComponent.cpp` (`Reflect`)
- Modify: `Code/dualsense_api_files.cmake`
- Create: `Code/Tests/Clients/DualSenseTriggerEffectTests.cpp`; modify `Code/dualsense_tests_files.cmake`

**Interfaces:**
- Consumes: existing TypeIds header pattern; `AzFramework::InputDeviceId`.
- Produces (frozen for Tasks 2–5 and phase 3):
  ```cpp
  namespace DualSense
  {
      enum class Trigger : AZ::u8 { L2, R2, Both };
      enum class TriggerEffectMode : AZ::u8 { Off, Feedback, Weapon, Vibration,
          MultiPositionFeedback, MultiPositionVibration, SlopeFeedback };

      struct TriggerEffect
      {
          AZ_TYPE_INFO(TriggerEffect, DualSenseTriggerEffectTypeId);
          static void Reflect(AZ::ReflectContext* context);

          TriggerEffectMode m_mode = TriggerEffectMode::Off;
          float m_startPosition = 0.0f;   // all fields normalized [0,1]
          float m_endPosition = 1.0f;
          float m_strength = 0.0f;        // amplitude for vibration modes; startStrength for slope
          float m_endStrength = 0.0f;     // slope mode only
          float m_frequency = 0.0f;       // vibration modes only (normalized; spec amendment)
          AZStd::array<float, 10> m_positionalValues{{0,0,0,0,0,0,0,0,0,0}}; // multi-position modes

          TriggerEffect Clamped() const;  // sanitized copy, see rules below
      };

      class DualSenseTriggerEffectRequests : public AZ::EBusTraits
      {
      public:
          static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
          static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
          using BusIdType = AzFramework::InputDeviceId;
          virtual ~DualSenseTriggerEffectRequests() = default;
          virtual void SetTriggerEffect(Trigger trigger, const TriggerEffect& effect) = 0;
          virtual void ClearTriggerEffects() = 0;
      };
      using DualSenseTriggerEffectRequestBus = AZ::EBus<DualSenseTriggerEffectRequests>;
  }
  ```
- `Clamped()` rules: every float clamped to [0,1] (each `m_positionalValues` entry too); then `m_endPosition = max(m_endPosition, m_startPosition)`.

- [ ] **Step 1: Write the failing tests**

`Code/Tests/Clients/DualSenseTriggerEffectTests.cpp` — fixture `UnitTest::LeakDetectionFixture`, tests:

```cpp
TEST_F(TriggerEffectFixture, Defaults_AreOffAndNeutral)
// mode Off, start 0, end 1, strengths 0, frequency 0, all positional 0

TEST_F(TriggerEffectFixture, Clamped_ClampsAllFieldsToUnitRange)
// set every field to -2.0f and 3.0f variants incl. two positional entries; Clamped() puts all in [0,1]

TEST_F(TriggerEffectFixture, Clamped_EnforcesEndPositionNotBeforeStart)
// start 0.8, end 0.2 -> end becomes 0.8; start 0.3, end 0.9 stays 0.9

TEST_F(TriggerEffectFixture, SerializeReflect_RegistersType)
// AZ::SerializeContext sc; DualSense::TriggerEffect::Reflect(&sc);
// EXPECT_NE(sc.FindClassData(azrtti_typeid<DualSense::TriggerEffect>()), nullptr);
```

Add to `Code/dualsense_tests_files.cmake`. Build → expect compile failure (header missing).

- [ ] **Step 2: Implement the header + Reflect + Clamped**

Create `DualSenseTriggerEffects.h` exactly per Interfaces (include `<AzCore/RTTI/TypeInfoSimple.h>`, `<AzCore/std/containers/array.h>`, `<AzCore/EBus/EBus.h>`, `<AzFramework/Input/Devices/InputDeviceId.h>`; UUIDs added to `DualSenseTypeIds.h` as `DualSenseTriggerEffectTypeId` and `DualSenseTriggerEffectRequestsTypeId` — generate fresh UUIDs with `uuidgen`). Implement `Reflect` (SerializeContext: all seven fields, Version(1)) and `Clamped()` in a new small cpp OR header-inline (inline is fine — it's arithmetic). Add the header to `dualsense_api_files.cmake`. Call `TriggerEffect::Reflect(context)` from `DualSenseSystemComponent::Reflect`. NOTE: only SerializeContext in this task — BehaviorContext comes in Task 4.

- [ ] **Step 3: Build + run tests → all pass (16 existing + 4 new)**

- [ ] **Step 4: Amend the spec** — in `docs/superpowers/specs/2026-07-26-dualsense-gem-design.md` §4.3, change `m_frequencyHz` to `m_frequency // normalized [0,1] (Apple API is normalized; HID compiler maps to raw byte)` and add `Both` to the Trigger enum sketch.

- [ ] **Step 5: Commit** — `feat: TriggerEffect struct, trigger enums, request bus, serialize reflection`

---

### Task 2: Degradation + validation logic (pure C++, TDD)

**Files:**
- Create: `Code/Source/Clients/DualSenseTriggerEffectMapping.h`, `.cpp`
- Modify: `Code/dualsense_private_files.cmake`
- Create: `Code/Tests/Clients/DualSenseTriggerMappingTests.cpp`; modify tests cmake

**Interfaces:**
- Consumes: `TriggerEffect` (Task 1).
- Produces (used by Task 3's .mm and by the phase-3 HID compiler as precedent):
  ```cpp
  namespace DualSense
  {
      //! True if the mode needs the macOS 12.3+ / firmware multi-position API surface.
      bool RequiresExtendedTriggerApi(TriggerEffectMode mode);

      //! Deterministic approximation of a 12.3+-only effect using only baseline
      //! (11.3) modes. Baseline modes pass through unchanged (after Clamped()).
      TriggerEffect DegradeToBaselineApi(const TriggerEffect& effect);
  }
  ```
- Degradation rules (exact, testable):
  - `MultiPositionFeedback` → `Feedback` with `m_startPosition = (index of first value > 0) / 9.0f` (1.0 if none) and `m_strength = max(values)`.
  - `MultiPositionVibration` → `Vibration` with the same start/amplitude derivation, `m_frequency` preserved.
  - `SlopeFeedback` → `Feedback` with `m_startPosition` preserved and `m_strength = (m_strength + m_endStrength) / 2`.
  - `Off/Feedback/Weapon/Vibration` → returned as `effect.Clamped()`, mode unchanged.
  - Output of degradation is always itself `Clamped()`.

- [ ] **Step 1: Write failing tests** — one test per rule above plus `RequiresExtendedTriggerApi` truth table (7 modes). Concrete cases:

```cpp
// MultiPositionFeedback with values {0,0,0.5,0.9,0,...} -> Feedback, start = 2/9.0f, strength 0.9
// MultiPositionFeedback all zeros -> Feedback, start 1.0, strength 0
// MultiPositionVibration values {0.4,0,...}, freq 0.6 -> Vibration start 0, amplitude 0.4, freq 0.6
// SlopeFeedback start .2 end .8 strengths .4/.8 -> Feedback start .2 strength .6
// Weapon passes through with clamping applied (feed it an out-of-range field)
```

- [ ] **Step 2: Build → compile failure; implement; build + run → green (16+4+~7)**
- [ ] **Step 3: Commit** — `feat: trigger-effect baseline-API degradation rules (pure, tested)`

---

### Task 3: Mac backend — bus handler on the gamepad implementation

**Files:**
- Modify: `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.h`, `.mm`
- Modify: `docs/hardware-smoke.md` (new unchecked section, Step 4)

**Interfaces:**
- Consumes: Tasks 1–2; `GCDualSenseAdaptiveTrigger` API per Global Constraints.
- Produces: the live handler — `InputDeviceGamepadDualSenseMac` connects `DualSenseTriggerEffectRequestBus` at `AzFramework::InputDeviceGamepad::IdForIndexN(GetInputDeviceIndex())` for its lifetime.

- [ ] **Step 1: Header changes** — class additionally derives `public DualSense::DualSenseTriggerEffectRequestBus::Handler`; declares `void SetTriggerEffect(Trigger, const TriggerEffect&) override;` and `void ClearTriggerEffects() override;` plus private `void ApplyEffectToTrigger(void* gcAdaptiveTrigger, const TriggerEffect& clamped);`.

- [ ] **Step 2: Implementation (.mm)** — connect in the ctor after the existing setup: `DualSenseTriggerEffectRequestBus::Handler::BusConnect(AzFramework::InputDeviceGamepad::IdForIndexN(GetInputDeviceIndex()));`; in the dtor, FIRST `ClearTriggerEffects()` (best-effort neutral triggers on release, each call guarded), then `BusDisconnect()`, before the existing haptics/controller teardown. Application logic:

```objc
void InputDeviceGamepadDualSenseMac::SetTriggerEffect(Trigger trigger, const TriggerEffect& effect)
{
    if (@available(macOS 11.3, *))
    {
        GCDualSenseGamepad* pad = (GCDualSenseGamepad*)((__bridge GCController*)m_controller).extendedGamepad;
        if (![pad isKindOfClass:[GCDualSenseGamepad class]]) { return; }
        TriggerEffect resolved = effect.Clamped();
        bool needsExtended = RequiresExtendedTriggerApi(resolved.m_mode);
        if (needsExtended)
        {
            if (@available(macOS 12.3, *)) { /* keep as-is */ }
            else
            {
                resolved = DegradeToBaselineApi(resolved);
                AZLOG_DEBUG("DualSense: degraded trigger effect mode %u to baseline API (macOS < 12.3)",
                            static_cast<AZ::u32>(effect.m_mode));
            }
        }
        if (trigger == Trigger::L2 || trigger == Trigger::Both)
        {
            ApplyEffectToTrigger((__bridge void*)pad.leftTrigger, resolved);
        }
        if (trigger == Trigger::R2 || trigger == Trigger::Both)
        {
            ApplyEffectToTrigger((__bridge void*)pad.rightTrigger, resolved);
        }
    }
}
```

`ApplyEffectToTrigger` switches on `m_mode`, each ObjC send in its own `@try/@catch (NSException*)` (log-debug and continue — dead-controller hardening, same rationale as the haptics file):
- `Off` → `setModeOff`
- `Feedback` → `setModeFeedbackWithStartPosition:m_startPosition resistiveStrength:m_strength`
- `Weapon` → `setModeWeaponWithStartPosition:m_startPosition endPosition:m_endPosition resistiveStrength:m_strength`
- `Vibration` → `setModeVibrationWithStartPosition:m_startPosition amplitude:m_strength frequency:m_frequency`
- `SlopeFeedback` (12.3 guard) → `setModeSlopeFeedbackWithStartPosition:endPosition:startStrength:m_strength endStrength:m_endStrength`
- `MultiPositionFeedback` (12.3 guard) → fill `GCDualSenseAdaptiveTriggerPositionalResistiveStrengths s; for i in 0..9 s.values[i]=m_positionalValues[i];` → `setModeFeedbackWithResistiveStrengths:s`
- `MultiPositionVibration` (12.3 guard) → same with `PositionalAmplitudes` + `frequency:m_frequency`

`ClearTriggerEffects()` → `SetTriggerEffect(Trigger::Both, TriggerEffect{})` (default mode Off).
Cast note: the adaptive-trigger objects are `pad.leftTrigger`/`pad.rightTrigger` typed `GCControllerButtonInput*`; cast to `GCDualSenseAdaptiveTrigger*` inside `ApplyEffectToTrigger` — the `isKindOfClass:GCDualSenseGamepad` gate makes that safe (mirror the file's existing downcast comment).

- [ ] **Step 3: Build module + tests + Editor (`--target DualSense DualSense.Tests Editor`), run suite** — green, no new unit tests (hardware-coupled; the pure logic was tested in Tasks 1–2 — say so in the report).

- [ ] **Step 4: Append to `docs/hardware-smoke.md`:**

```markdown
## Phase 2 — adaptive triggers

With a DualSense connected via the testbed Editor (USB; BT deferred).

- [ ] `dualsense_trigger r2 weapon` -> R2 has a distinct resistance zone with a "break" like a gun trigger
- [ ] `dualsense_trigger r2 feedback` -> constant resistance from ~30% pull
- [ ] `dualsense_trigger both vibration` -> both triggers buzz when pulled past ~20%
- [ ] `dualsense_trigger r2 slope` -> resistance ramps up across the pull (macOS 12.3+)
- [ ] `dualsense_trigger r2 multifeedback` -> stepped resistance zones (macOS 12.3+)
- [ ] `dualsense_trigger_clear` -> both triggers neutral again
- [ ] Unplug with an active trigger effect -> no crash, clean restore
- [ ] Reconnect -> triggers neutral (no stale effect)
```

- [ ] **Step 5: Commit** — `feat(mac): apply trigger effects via GCDualSenseAdaptiveTrigger (bus handler on gamepad impl)`

---

### Task 4: BehaviorContext reflection — Script Canvas / Lua access

**Files:**
- Modify: `Code/Include/DualSense/DualSenseTriggerEffects.h` (Reflect grows BehaviorContext branch)
- Create: `Code/Tests/Clients/DualSenseScriptReflectionTests.cpp`; modify tests cmake

**Interfaces:**
- Consumes: Task 1's Reflect.
- Produces: script names — class `DualSenseTriggerEffect` (properties `mode`, `startPosition`, `endPosition`, `strength`, `endStrength`, `frequency`, `positionalValues`), enum constants (`DualSenseTrigger_L2/R2/Both`, `DualSenseTriggerEffectMode_Off/...`), EBus `DualSenseTriggerEffectRequestBus` with events `SetTriggerEffect`, `ClearTriggerEffects`.

- [ ] **Step 1: Failing test**

```cpp
TEST_F(ScriptReflectionFixture, BehaviorContext_RegistersTriggerEffectClassAndBus)
{
    AZ::BehaviorContext bc;
    DualSense::TriggerEffect::Reflect(&bc);
    EXPECT_NE(bc.m_classes.find("DualSenseTriggerEffect"), bc.m_classes.end());
    EXPECT_NE(bc.m_ebuses.find("DualSenseTriggerEffectRequestBus"), bc.m_ebuses.end());
}
```

(If `m_classes`/`m_ebuses` member names differ in this engine version, check `AzCore/RTTI/BehaviorContext.h` and use the real maps — record the correction.)

- [ ] **Step 2: Implement** — in `TriggerEffect::Reflect`, add the `azrtti_cast<AZ::BehaviorContext*>` branch following the MiniAudio pattern (`Gems/MiniAudio/Code/Source/Clients/MiniAudioPlaybackComponent.cpp` in the engine repo, read it for attribute style): `->Attribute(AZ::Script::Attributes::Module, "dualsense")`, `Category "DualSense"`, `Scope Common`; class properties via `BehaviorValueProperty`; enums as `bc->EnumProperty` or class constants (match what compiles cleanly in this engine — record choice); EBus with `BehaviorParameterOverrides` giving named/documented params for `SetTriggerEffect` ("Trigger", "Which trigger: L2, R2, or Both" / "Effect", "The trigger effect to apply").

- [ ] **Step 3: Build + run suite → green. Commit** — `feat: reflect trigger effects to BehaviorContext (Script Canvas/Lua)`

---

### Task 5: Console commands for hardware smoke

**Files:**
- Modify: `Code/Source/Clients/DualSenseSystemComponent.cpp` (DebugCommands namespace)

**Interfaces:**
- Consumes: bus + struct (Task 1).
- Produces: `dualsense_trigger <l2|r2|both> <off|feedback|weapon|vibration|slope|multifeedback|multivibration> [slot]` and `dualsense_trigger_clear [slot]`, preset parameters per mode (constants in the command, documented in its help string):
  - feedback: start 0.3, strength 0.8
  - weapon: start 0.2, end 0.6, strength 0.9
  - vibration: start 0.2, amplitude 0.75, frequency 0.6
  - slope: start 0.2, end 0.9, startStrength 0.3, endStrength 1.0
  - multifeedback: positional {0,0,0.3,0.3,0.6,0.6,0.9,0.9,1.0,1.0}
  - multivibration: positional {0,0.5,0,0.5,0,0.5,0,0.5,0,0.5}, frequency 0.5

- [ ] **Step 1: Implement both commands** (usage message on wrong arg count, matching `dualsense_lightbar`'s pattern; `DontReplicate`; bus event addressed `IdForIndexN(slot)`).
- [ ] **Step 2: Build module + tests + Editor; suite green. Commit** — `feat: dualsense_trigger console commands with per-mode presets`

---

### Task 6: Docs + wrap (phase-2 tag deferred to hardware pass)

**Files:**
- Modify: `README.md`, `docs/superpowers/specs/2026-07-26-dualsense-gem-design.md` (only if Task 1 Step 4 left anything stale)

- [ ] **Step 1: README** — move adaptive triggers from Planned to Implemented (macOS, USB-verified pending), document the two console commands and the script API names, keep honest BT status.
- [ ] **Step 2: Build everything once more (`DualSense DualSense.Tests Editor`), full suite green.**
- [ ] **Step 3: Commit** — `docs: phase 2 status (trigger effects implemented on macOS, hardware pass pending)`. NO tag — `phase-2` happens after the human hardware smoke.

## Self-review notes

- Spec §4.3 coverage: `DualSenseRequestBus` extras (`IsDualSense`, `GetConnectionType`, `SetPlayerLeds`, `SetMicLed`) are deliberately OUT of this plan — GameController.framework exposes none of them on Mac; they land with the raw-HID phase 3 where they're implementable. Recorded as a conscious scope cut, not an omission.
- Type consistency: `Trigger`/`TriggerEffectMode`/`TriggerEffect`/bus names identical across Tasks 1–5; degradation function names in Task 2 match Task 3's calls.
- The only "verify against reality" steps: BehaviorContext member names (Task 4 Step 1) and enum-reflection idiom (Task 4 Step 2) — both with exact file pointers.
