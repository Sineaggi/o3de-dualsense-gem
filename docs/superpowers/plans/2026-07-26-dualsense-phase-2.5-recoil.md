# DualSense Phase 2.5 — Recoil ("Firing Feel") Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sharp haptic "kick" pulses (CoreHaptics transients) with an API scripts can call per shot, plus hardware-synchronized weapon-fire detection (adaptive-trigger status edge → notification + optional auto-kick), demoable via one console command.

**Architecture:** Two thin layers on the existing Phase-1/2 machinery. (1) `DualSenseHapticPulseRequestBus` (ById `InputDeviceId`, Single — same traits as the trigger bus) with `PlayHapticPulse(left, right, sharpness)`, implemented on `DualSenseHapticsMac` as CoreHaptics **transient** events on the existing per-handle engines. (2) Weapon-fire detection: `TickInputDevice` already reads the pad each frame; add per-trigger status-edge detection (`GCDualSenseAdaptiveTriggerStatusWeaponFired` transition, SDK-verified 2026-07-26) → broadcast `DualSenseTriggerNotificationBus::OnWeaponTriggerFired(Trigger)` + optional per-device auto-recoil (config via the pulse bus). All handler lifecycles ride the existing impl (connect in ctor beside the trigger bus, disconnect in dtor in the established order).

**Tech Stack / verified SDK facts:** `CHHapticEventTypeHapticTransient` + `CHHapticEventParameterIDHapticSharpness` (macOS 10.15+ — under our 11.3 floor, no new guards); `GCDualSenseAdaptiveTrigger.status` with `...WeaponReady/WeaponFiring/WeaponFired` (11.3+). All established repo conventions bind (MRC, per-send @try/@catch, main-thread, quoted same-dir includes).

## Global Constraints

Identical to the Phase 2 plan's Global Constraints (same branch mechanics — new branch `feature/phase-2.5-recoil` from `main`; same build/test commands; suite baseline **42/42**, must only grow; never commit red; trailer `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`; zero engine modifications; hardware smoke + tag deferred to human). Public API names below are frozen once Task 1 lands.

---

### Task 1: Pulse API — bus, Mac transient implementation, reflection

**Files:**
- Create: `Code/Include/DualSense/DualSenseHaptics.h` (new UUID via uuidgen in TypeIds header)
- Modify: `Code/Include/DualSense/DualSenseTypeIds.h`, `Code/dualsense_api_files.cmake`
- Modify: `Code/Platform/Mac/DualSenseHapticsMac.h`, `.mm` (add `PlayTransientPulse(float left, float right, float sharpness)`)
- Modify: `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.h`, `.mm` (derive + connect the new bus handler; forward to m_haptics)
- Modify: `Code/Tests/Clients/DualSenseScriptReflectionTests.cpp` (+ files.cmake if new file preferred)

**Interfaces (frozen):**
```cpp
namespace DualSense
{
    class DualSenseHapticPulseRequests : public AZ::EBusTraits
    {   // traits identical to DualSenseTriggerEffectRequests (ById, Single, BusIdType=InputDeviceId)
    public:
        //! One sharp transient kick. Intensities/sharpness normalized [0,1]; 0 intensity = skip that side.
        virtual void PlayHapticPulse(float leftIntensity, float rightIntensity, float sharpness) = 0;
        //! Enable/disable hardware-synchronized auto-recoil for a trigger's Weapon-mode fire edge.
        virtual void SetAutoRecoil(Trigger trigger, bool enabled, float intensity, float sharpness) = 0;
        virtual ~DualSenseHapticPulseRequests() = default;
    };
    using DualSenseHapticPulseRequestBus = AZ::EBus<DualSenseHapticPulseRequests>;
}
```
Mac transient implementation: build a `CHHapticPattern` with one `CHHapticEventTypeHapticTransient` event carrying Intensity + Sharpness `CHHapticEventParameter`s, per side with intensity > 0.001; reuse the existing per-handle engines; player lifecycle per the file's existing rolling-slot MRC idiom (a transient player slot per side, released when replaced/torn down — mirror `UpdateSide`); every CH call individually @try/@catch'd. `SetAutoRecoil` just stores per-trigger config on the impl (used by Task 2); document that in a comment.
Reflection: bus events with named `BehaviorParameterOverrides` in the existing `Reflect`'s BehaviorContext branch (same Module/Category/Scope). TDD: reflection-presence test + a Lua dispatch test into a fixture handler (copy the established `TestTriggerEffectHandler` + `DualSense_GetGamepadDeviceId` pattern). Steps: failing tests → build red → implement → 42+N green → commit `feat: haptic pulse bus + CoreHaptics transient kicks (mac)`.

### Task 2: Weapon-fire detection — status edges, notification bus, auto-recoil

**Files:**
- Create: `Code/Source/Clients/DualSenseTriggerFireDetector.h`, `.cpp` (pure edge logic) + tests file
- Modify: `Code/Include/DualSense/DualSenseHaptics.h` (notification bus lives beside the pulse bus)
- Modify: `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.h`, `.mm` (tick integration)
- Modify: cmake lists

**Interfaces (frozen):**
```cpp
namespace DualSense
{
    class DualSenseTriggerNotifications : public AZ::EBusTraits
    {   // ById on InputDeviceId, HandlerPolicy::Multiple (listeners), BusIdType=InputDeviceId
    public:
        virtual void OnWeaponTriggerFired(Trigger trigger) {}
        virtual ~DualSenseTriggerNotifications() = default;
    };
    using DualSenseTriggerNotificationBus = AZ::EBus<DualSenseTriggerNotifications>;

    //! Pure, unit-testable: true exactly on the transition INTO the fired state.
    //! prev/current are the raw GCDualSenseAdaptiveTriggerStatus integral values, but the
    //! function takes the gem-side enum so it stays ObjC-free:
    enum class WeaponTriggerStatus : AZ::u8 { Unknown, Ready, Firing, Fired };
    bool IsWeaponFireEdge(WeaponTriggerStatus previous, WeaponTriggerStatus current);
}
```
Rules (test each): `(*, Fired)` where previous != Fired → true; `(Fired, Fired)` → false; anything → non-Fired → false; Unknown handled (first frame never fires). TDD the pure function first. Mac tick: after the existing input read (pad non-nil path only), map each trigger's `.status` (guarded @try/@catch; unknown on exception) to `WeaponTriggerStatus` via a small switch (`...WeaponReady/Firing/Fired` → enum; everything else → Unknown), run edge detection against per-trigger previous-state members, and on edge: `DualSenseTriggerNotificationBus::Event(deviceId, OnWeaponTriggerFired, trigger)` + if that trigger's auto-recoil config (Task 1) is enabled, `m_haptics->PlayTransientPulse(...)` biased to the firing side (L2 → left engine, R2 → right; use the configured intensity on that side, 0 on the other). Reset previous-state on pad-nil. Reflect the notification bus to BehaviorContext with an `AZ_EBUS_BEHAVIOR_BINDER` handler (engine precedent: `InputDeviceNotificationBusBehaviorHandler` in AzFramework's `InputDevice.cpp`) so Lua/SC can implement `OnWeaponTriggerFired`. Commit `feat(mac): weapon-fire detection, notification bus, auto-recoil`.

### Task 3: Demo command + docs + wrap

**Files:** `Code/Source/Clients/DualSenseSystemComponent.cpp`, `README.md`, `docs/hardware-smoke.md`

- `dualsense_fire_demo [l2|r2|both] [slot]` (default r2, slot 0): sets the Weapon trigger effect (existing preset values) AND `SetAutoRecoil(trigger, true, 0.9f, 0.7f)` via the two buses — one command, complete firing feel. `dualsense_fire_demo_off [slot]`: `dualsense_trigger_clear` semantics + auto-recoil disabled. Usage messages per file convention.
- README: new "Recoil / firing feel" section (pulse API, notification, auto-recoil, demo commands, script names); keep honest about hardware-pending + BT.
- hardware-smoke.md: unchecked "Phase 2.5 — recoil" section: fire-demo kick felt on trigger break (r2, l2, both), kick absent after demo_off, PlayHapticPulse via console... (pulse-only check needs a command? `dualsense_pulse <l> <r> [sharpness] [slot]` — add it in this task, tiny), unplug during active auto-recoil clean.
- Full build (three targets) + suite green; commit `feat: fire demo + pulse command; phase 2.5 docs`. NO tag (post-hardware).

### Final review
Controller-run whole-branch review (most capable model) per SDD process, one fix wave max, then human hardware smoke → tag `phase-2.5` → merge decision.

## Self-review notes
- Notification bus uses HandlerPolicy::Multiple deliberately (listeners), unlike the Single request buses — flagged so reviewers don't "fix" it.
- WeaponTriggerStatus keeps ObjC out of the pure detector; the .mm owns the GC-status→enum mapping.
- Auto-recoil intensity is per-trigger config, not per-shot — per-shot variation stays gameplay's job via OnWeaponTriggerFired + PlayHapticPulse.
