# DualSense Phase 2.6 — Autofire Feel + Keyboard Test Scene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the three gaps vs the hardware-validated reference implementation documented at `~/pong/docs/dualsense-porting-guide.md` (READ IT FIRST — every implementer): (1) the *repeated-firing* trigger feel (vibration mode at LOW frequency — the reference uses 25 Hz raw; our normalized 0.6 ≈ fine buzz, wrong feel), (2) sustained `haptic_buzz` + `haptic_stop` with the guide's CoreHaptics lifecycle hardening, (3) a keyboard-driven in-Editor test scene (Lua + `.inputbindings` shipped in gem `Assets/`) replacing console typing — modeled on `~/pong/trigger_test.gd`.

**Architecture:** Extends the existing frozen buses (additive only). `PlayHapticBuzz`/`StopHaptics` join `DualSenseHapticPulseRequestBus`. The Mac haptics layer gains a duration-bearing continuous event (CoreHaptics 30 s ceiling) sharing the continuous slot with rumble (last-writer-wins — the guide confirms CoreHaptics-wins contention makes "one system at a time" the rule anyway), plus the guide's lifecycle gotchas #2/#3/#5: stoppedHandler installed, BOTH handlers' bodies dispatched to the main queue (they fire off-main — Apple-documented; our current resetHandler restart has this latent race TODAY and gets fixed here), per-side generation counters so deferred clears can't nil newer players. Two script convenience helpers (`DualSense_SetRumble`, `DualSense_SetLightBar`) bridge the engine's unreflected output buses for the Lua scene's coexistence checks. Test scene assets ship in gem `Assets/DualSenseTest/` (scan folder already configured in Registry/assetprocessor_settings.setreg).

**Reference material for implementers (read-only, cite what you use):** `~/pong/docs/dualsense-porting-guide.md` (validated gotchas + protocol), `~/pong/trigger_test.gd` (key map + validation semantics). Frequency mapping fact: raw firmware frequency is Hz 0–255; Apple's API is normalized [0,1] — assume linear (≈ Hz/255, so 25 Hz ≈ 0.098) and let the scene's frequency-sweep keys lock the feel empirically on hardware.

## Global Constraints

Same as prior phases (branch `feature/phase-2.6-autofire` from `main`; build targets `DualSense DualSense.Tests Editor`; suite baseline **58/58**, only grows; zero engine modifications; MRC + per-CH-call @try/@catch; frozen existing API names untouched — additions only; trailer `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`; never commit red; hardware smoke + tag deferred to human).

---

### Task 1: Buzz/Stop API + CoreHaptics lifecycle hardening

**Files:** `Code/Include/DualSense/DualSenseHaptics.h` (additive), `Code/Platform/Mac/DualSenseHapticsMac.{h,mm}`, `Code/Platform/Mac/InputDeviceGamepadDualSenseMac.{h,mm}` (forwarders), `Code/Tests/Clients/DualSenseScriptReflectionTests.cpp`, cmake untouched (no new files) unless a pure helper wants one.

**Interfaces (frozen additions to `DualSenseHapticPulseRequests`):**
```cpp
//! Sustained buzz on the voice-coil actuators. Duration clamped to (0, 30] seconds
//! (CoreHaptics ceiling). Shares the continuous actuator channel with rumble
//! (SetVibration): last writer wins per side. Intensity 0 skips that side.
virtual void PlayHapticBuzz(float leftIntensity, float rightIntensity, float sharpness, float durationSeconds) = 0;
//! Stops gem-issued haptics (transient pulses and buzzes) on both sides.
//! Does not touch trigger effects; a subsequent SetVibration re-owns the channel.
virtual void StopHaptics() = 0;
```
Mac: buzz = continuous CHHapticEvent with duration on the existing continuous slots (document the rumble-sharing in a comment); Stop clears transient + continuous slots both sides. Lifecycle hardening in the SAME task (guide gotchas #2/#3/#5, cite them in comments): install `stoppedHandler` (clear cached players for that engine's side), wrap BOTH resetHandler and stoppedHandler bodies in `dispatch_async(dispatch_get_main_queue(), ...)` with the existing `__block`/alive-guard idioms adapted (handlers fire off-main; our current resetHandler restart is racy — fix it here), add a per-side `uint32_t` generation counter bumped on every player store/clear; deferred handler clears compare generation before nil-ing. TDD: reflection + Lua dispatch tests for both new methods (established fixture pattern). Commit `feat(mac): haptic buzz/stop + CoreHaptics lifecycle hardening (porting-guide gotchas 2/3/5)`.

### Task 2: Autofire preset + script bridge helpers

**Files:** `Code/Source/Clients/DualSenseSystemComponent.cpp`, `Code/Include/DualSense/DualSenseTriggerEffects.h` (reflection additions only), tests file.

- `dualsense_trigger` gains mode word `autofire`: Vibration mode, start 0.2, amplitude 0.9, **frequency 0.098** (≈25 Hz per the reference; comment the mapping assumption). `dualsense_fire_demo` gains optional mode word `auto` → autofire trigger preset + auto-recoil stays OFF (the trigger's own vibration IS the repeated fire feel; document distinction from the weapon+kick demo).
- Reflect two global helpers (same attribute set as `DualSense_GetGamepadDeviceId`): `DualSense_SetRumble(AZ::u32 slot, float left, float right)` → `InputHapticFeedbackRequestBus::Event(IdForIndexN(slot), &SetVibration, l, r)`; `DualSense_SetLightBar(AZ::u32 slot, float r, float g, float b)` → `InputLightBarRequestBus` equivalent. One-line comment each: the engine buses aren't behavior-reflected (see spec §2) — these bridge them for scripts/test scenes.
- TDD: Lua dispatch tests for both helpers (fixture handlers on the ENGINE buses this time — connect a test handler to InputHapticFeedbackRequestBus at IdForIndex0, same shape as existing fixtures). Commit `feat: autofire trigger preset + script bridge helpers for rumble/lightbar`.

### Task 3: Keyboard test scene (Lua + inputbindings in gem Assets)

**Files:** `Assets/DualSenseTest/dualsense_test.inputbindings`, `Assets/DualSenseTest/DualSenseTest.lua`, README, docs/hardware-smoke.md.

- `.inputbindings`: hand-authored (copy the XML ObjectStream structure from the engine's `Gems/StartingPointInput/Assets/*.inputbindings` samples — read one first). Keyboard events (device `keyboard`, channels `keyboard_key_alphanumeric_*` / `keyboard_key_numeric_*` — verify exact channel names in engine `InputDeviceKeyboard.h`): mirror `~/pong/trigger_test.gd`'s map — 1 feedback, 2 weapon, 3 vibration(classic 0.6), 4 multifeedback, 5 slope, 6 multivibration, 7 **autofire**, 0 off, Q/E/W axis L2/R2/Both, R rumble-1s (coexistence), L lightbar cycle, T tap, Y buzz 1.5s, U stop, G left-only tap, H right-only buzz, **[ / ] frequency sweep** (±0.02 normalized, re-applies autofire, logs the value — this is how the firing feel gets locked empirically). Event names prefixed `ds_` (e.g. `ds_mode_feedback`).
- `DualSenseTest.lua`: connects `InputEventNotificationBus` per event (StartingPointInput pattern — see engine `Gems/StartingPointInput/Assets/Scripts/` samples for the Lua idiom), holds state (axis, frequency), calls our buses + the Task-2 helpers, `Debug.Log`s every action mirroring trigger_test.gd's prints. Also connects `DualSenseTriggerNotificationBus` and logs `OnWeaponTriggerFired` (live fire-event visibility in the scene).
- README: "Test scene" section — 30-second setup (entity in any level + Input Configuration component with the bindings asset + Lua Script component with the script), full key legend, note that this supersedes console commands for hardware passes and finally exercises the per-binding input path (deferred smoke item from phase 1).
- hardware-smoke.md: "Phase 2.6" unchecked section — keyboard-driven full matrix per the guide's validation protocol §"Validation protocol" (each mode felt incl. autofire ≈ repeated-fire thumps at swept frequency; buzz/stop/locality; coexistence R/L both orders; per-target isolation with explicit clears; disconnect/reconnect mid-buzz), plus "phase-1 per-binding input matrix items now checkable via this scene."
- Commit `feat: keyboard-driven hardware test scene (gem assets); phase 2.6 docs`.

### Final review
Whole-branch (most capable model), one fix wave max, then human hardware pass in the test scene → tag `phase-2.6` → merge.

## Self-review notes
- Existing API surface untouched; buzz shares the continuous slot BY DESIGN (guide-verified contention semantics) — reviewers shouldn't demand a third slot without hardware evidence.
- The 25 Hz ≈ 0.098 mapping is an assumption to be validated by the sweep keys — the plan says so; do not present it as fact in user docs.
- `.inputbindings` hand-authoring risk: if the ObjectStream format fights back, fallback is documented in-task: author it in the Editor once and commit the produced asset (still ships in gem Assets) — implementer reports which path was taken.
