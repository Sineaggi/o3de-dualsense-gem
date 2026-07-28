# Hardware smoke checklist

Run after each phase lands. Record date + result per line.

## Phase 0 — swap proof (Editor, no hardware needed)
- [x] `dualsense_debug_swap` logs install line, no errors (2026-07-26, Editor profile build)
- [x] `dualsense_debug_restore` restores; no "skipped" warning in Editor (2026-07-26)
- [x] Regular paired controller (if any) still works after restore — N/A, no controller paired (2026-07-26)

## Phase 1 — Mac input

With a DualSense connected via the testbed Editor. Try BOTH USB and Bluetooth.

**2026-07-26 result: USB matrix verified at smoke level (real DualSense, Editor profile build).
Bluetooth deferred by decision — re-run this section over BT before relying on BT support.**

- [x] Console shows the takeover log from Task 7 ("DualSense: controller detected, taking over gamepad slot N") — USB, verified across multiple connect/reconnect cycles (2026-07-26)
- [ ] Entity with an Input component bound to a default `.inputbindings` asset (or any input-driven sample) receives events for:
  - [ ] All 4 face buttons (cross/circle/square/triangle -> A/B/X/Y)
  - [ ] D-pad (up/down/left/right)
  - [ ] Shoulders (L1/R1)
  - [ ] Stick clicks (L3/R3)
  - [ ] Menu/Options (Start/Select)
  - [ ] Both analog triggers report gradual (non-binary) values across their range
  - [ ] Both sticks report full range of motion with no drift at rest
  - *(Not run per-binding — testbed has no bindings asset yet. Raw smoke 2026-07-26 USB: sticks/triggers/buttons exercised with no console errors or event spam. Full per-channel matrix deferred to the phase-2 sample content.)*
- [ ] Sticks at rest produce no held events (deadzones effective) — *not separately asserted; no spam observed at rest during smoke (USB, 2026-07-26)*
- [ ] Repeat the above over Bluetooth (not just USB) — **deferred 2026-07-26**
- [x] With DualSense connected, only ONE gamepad slot reports connected (no ghost second slot in logs) — USB, log-verified (2026-07-26; playerIndex coordination fix 1e387e1)

## Phase 1 — rumble

With a DualSense connected via the testbed Editor. Try BOTH USB and Bluetooth.

- [x] `dualsense_rumble 1 0` -> strong vibration concentrated on the left grip (USB, 2026-07-26)
- [x] `dualsense_rumble 0 1` -> strong vibration concentrated on the right grip (USB, 2026-07-26)
- [x] `dualsense_rumble 0 0` -> silence (USB, 2026-07-26)
- [x] `dualsense_rumble 0.2 0.2` -> clearly weaker than `dualsense_rumble 1 1` (USB, 2026-07-26)
- [x] Disconnect mid-rumble -> no crash, clean restore logs (FAILED 2026-07-26: NSException from dead CHHapticEngine + off-main notification delivery — fixed e961d66+c539a3e, re-verified PASS 2026-07-26; allocator-assert follow-up 74bacb7 also re-verified clean)
- [ ] Repeat over Bluetooth — **deferred 2026-07-26**

## Phase 1 — lightbar

With a DualSense connected via the testbed Editor. Try BOTH USB and Bluetooth.

- [x] `dualsense_lightbar 1 0 0` -> red (USB, 2026-07-26)
- [x] `dualsense_lightbar 0 1 0` -> green (USB, 2026-07-26)
- [ ] `dualsense_lightbar 0 0 1` -> blue — *not run*
- [ ] `dualsense_lightbar 1 1 1` -> white — *not run*
- [ ] Disconnect mid-color-change -> no crash, clean restore logs — *not run (disconnect-mid-rumble covers the teardown path)*
- [ ] Reconnect -> lightbar state sane — *reconnects verified repeatedly; lightbar state not specifically observed*
- [ ] Repeat over Bluetooth — **deferred 2026-07-26**

## Phase 2 — adaptive triggers

**2026-07-26: full matrix PASS on hardware (USB, real DualSense) — all modes felt correct incl. slope/multi-position (macOS >= 12.3 confirmed); unplug-with-active-effect clean; BT still deferred.**

With a DualSense connected via the testbed Editor (USB; BT deferred).

- [x] `dualsense_trigger r2 weapon` -> R2 has a distinct resistance zone with a "break" like a gun trigger
- [x] `dualsense_trigger r2 feedback` -> constant resistance from ~30% pull
- [x] `dualsense_trigger both vibration` -> both triggers buzz when pulled past ~20%
- [x] `dualsense_trigger r2 slope` -> resistance ramps up across the pull (macOS 12.3+)
- [x] `dualsense_trigger r2 multifeedback` -> stepped resistance zones (macOS 12.3+)
- [x] `dualsense_trigger_clear` -> both triggers neutral again
- [x] Unplug with an active trigger effect -> no crash, clean restore
- [x] Reconnect -> triggers neutral (no stale effect)

## Phase 2.5 — recoil

**2026-07-26: PASS on hardware (USB) — fire demo kick-on-break verified all variants; user note: semantics for REPEATED firing (full-auto feel) are phase 2.6 scope, not a 2.5 defect.**

With a DualSense connected via the testbed Editor (USB; BT deferred).

- [x] `dualsense_fire_demo r2` -> R2 has weapon resistance and a sharp kick felt exactly on trigger break
- [x] `dualsense_fire_demo l2` -> L2 has weapon resistance and auto-recoil kick
- [x] `dualsense_fire_demo both` -> both triggers have weapon resistance and synchronized kicks on each break
- [x] `dualsense_fire_demo_off` -> no more kicks, triggers return to neutral
- [x] `dualsense_pulse 1 0` -> sharp isolated kick felt in left actuator
- [x] `dualsense_pulse 0 1` -> sharp isolated kick felt in right actuator
- [x] Unplug with auto-recoil active -> no crash, no lingering haptic state, clean restore
- [x] Reconnect after unplug -> no phantom kick on reconnect (baseline-not-edge correctness verified)

## Phase 2.6 — autofire feel + keyboard test scene

**2026-07-27: NATIVE backend over BLUETOOTH — PASS (real DualSense, BT link).** Full trigger-mode
matrix (feedback/weapon/vibration/multifeedback/slope/multivibration/autofire) applied and felt
correct over Bluetooth via the test scene; OnWeaponTriggerFired events streamed on trigger breaks.
This closes the native-path "BT deferred" caveat carried since Phase 1 for trigger effects and
fire detection. Still open: BT rumble/lightbar/haptics spot-checks, and the SDL-backend BT leg
(Windows/Linux transport validation) — see Phase 3a section.


With a DualSense connected via the testbed Editor (USB; BT deferred). Use the keyboard test scene
(`Assets/DualSenseTest/dualsense_test.inputbindings` + `DualSenseTest.lua` — see README "Test
scene" for setup) for all items below; this is the per-binding input path deferred from the
Phase 1 matrix, now checkable.

**Full mode matrix (`Q`/`E`/`W` select axis, default BOTH):**

- [ ] `1` feedback -> distinct resistance zone starting ~30% pull
- [ ] `2` weapon -> resistance zone with a "break" like a gun trigger
- [ ] `3` vibration -> fine buzz past ~20% pull
- [ ] `4` multifeedback -> stepped resistance zones
- [ ] `5` slope -> resistance ramps up across the pull
- [ ] `6` multivibration -> alternating buzz zones
- [ ] `0` off -> trigger returns to neutral
- [ ] `Q` / `E` / `W` -> effect applies to LEFT only / RIGHT only / BOTH triggers respectively

**Autofire feel + frequency lock (the central Phase 2.6 deliverable):**

- [x] `7` autofire at 0.098 (25 Hz, mapping now proven) -> repeated-fire thumps (2026-07-27)
- [x] `[` / `]` sweep available for per-title tuning; default confirmed correct, no sweep needed
      to calibrate an unknown mapping (superseded by the instruction-level RE)
- [x] **Locked-in frequency: 0.098 (== 25 Hz) — CONFIRMED 2026-07-27.** Two independent lines of
      evidence: (1) user hardware verdict "feels about right" for full-auto cadence; (2) the
      normalized->raw mapping was reverse-engineered at instruction level (round(f*255), see spec
      §3) proving 0.098 == 25 Hz on BOTH backends, so the shipped default needed no change.

**Coexistence (rumble/lightbar concurrent with an active trigger effect):**

- [ ] With a trigger effect active (any of `1`-`7`), press `R` -> rumble runs for 1s, trigger
      effect is unaffected/still felt afterward
- [ ] With a trigger effect active, press `L` -> light bar cycles color, trigger effect is
      unaffected/still felt afterward
- [ ] Reverse order: `R` then a mode key while rumble is still running -> both coexist, no
      interference
- [ ] Reverse order: `L` then a mode key while a color is set -> both coexist, no interference
- [ ] Shared-slot semantics: `R` rumble, then `Y`/`H` buzz within the 1s rumble window -> buzz
      keeps playing for its own full duration (the deferred rumble-zero at 1s is skipped once a
      buzz has taken the slot); this is expected -- `R` rumble and `Y`/`H` haptic buzz share one
      continuous actuator slot on Mac (see README key legend), unlike trigger effects vs.
      rumble/lightbar above, which are independent channels

**Haptics isolation (per-target clears):**

- [ ] `T` tap (both sides) -> felt in both actuators simultaneously
- [ ] `G` tap LEFT only -> felt in left actuator only, right silent
- [ ] `Y` buzz 1.5s (both sides) -> sustained buzz both sides, stops on its own after 1.5s
- [ ] `H` buzz RIGHT only 1.0s -> sustained buzz right actuator only, left silent, stops after 1.0s
- [ ] `U` stop mid-buzz -> immediately silences both sides without waiting for the duration to
      elapse
- [ ] `U` with no haptic active -> no-op, no error

**Disconnect/reconnect mid-buzz:**

- [ ] Trigger `Y` (buzz 1.5s), unplug mid-buzz -> no crash, clean restore logs
- [ ] Reconnect -> no lingering/phantom buzz, light bar and trigger state sane
- [ ] Repeat over Bluetooth -> **deferred, consistent with all prior phases' BT deferral**

**Live fire-event visibility:**

- [ ] With `2` (weapon) applied and auto-recoil/fire-demo active via console
      (`dualsense_fire_demo`), pull and release the trigger past the break -> `OnWeaponTriggerFired`
      logs in the console via the test scene's `DualSenseTriggerNotificationBus` handler

**Phase-1 per-binding input matrix — infrastructure now checkable via this scene:**

Phase 1 deferred the per-binding item because the testbed had no `.inputbindings` asset; only raw
device polling was exercised. This scene is the first hand-authored bindings asset in the gem, so
it proves the full pipeline (Input Configuration component -> `.inputbindings` asset ->
`InputEventNotificationBus` -> Lua) end to end for the first time, on the same component used for
gamepad bindings elsewhere:

- [ ] All 20 `ds_*` keyboard events resolve correctly (every key in the legend produces exactly the
      logged action, no misfires, no dead keys)
- [ ] No spurious/held-key event spam while a key is held down but not repeatedly pressed
      (confirms dead-zone/event-edge handling in the bindings pipeline, standing in for the
      deferred gamepad stick-deadzone item since the same pipeline code handles both)

## Phase 3a — SDL backend (macOS A/B)

**2026-07-27: SDL BACKEND OVER BLUETOOTH — PASS (real DualSense, transport byte 0x05 logged).**
Backend activated cleanly, pad detected and claimed via SDL, trigger modes 1-7 applied and felt
correct through the gem's raw byte compiler -> SDL_SendJoystickEffect -> SDL BT framing/CRC.
This hardware-validates the exact transport stack the Windows/Linux backends will use.
Finding from the same session: editor fly-cam DRIFT under sdl backend — root-caused to zero
stick deadzones (correct for pre-filtered GameController input, wrong for SDL raw ADC values);
fix: XInput-canonical deadzones in the SDL impl (see deadzone-fix commit). Re-verify no-drift
after the fix.


All items unchecked. Run the test scene/console commands with `dualsense_backend sdl` active
(set via console command or `+dualsense_backend sdl` command-line flag before launching). This
section validates the SDL3 backend implementation and verifies coexistence with the stock
engine's GameController.framework. Use the same test scene (`Assets/DualSenseTest/`) and
console commands as the Phase 2.6 items above, selecting the SDL backend at the top of each test.

**Backend selection / flag purity:**

- [ ] Default (`dualsense_backend` left at `native`): confirm via Console/log that no SDL3
      initialization occurs and behavior is bit-for-bit identical to pre-Phase-3a (no SDL_Init
      cost, 2.6 behavior unchanged).
- [ ] `dualsense_backend sdl` with a DualSense connected: native GameController slot is released
      ("restoring platform default" log), SDL backend activates, monitor detects the same pad
      within a tick or two ("controller detected, taking over gamepad slot N" log). GUID bus byte
      logged: `0x03` USB, `0x05` Bluetooth (confirm match vs. actual connection type).
- [ ] `dualsense_backend native` while sdl is active: reverse transition (SDL slot restored, SDL
      backend deactivated, native GameController reclaims the pad within a tick or two).
- [ ] Rapid back-and-forth switching (`sdl`/`native`/`sdl`/...) several times: no crash, no
      duplicate slot claims, no leaked gamepad handles (watch for matching "closing gamepad
      handle" debug logs per activation).

**Input parity vs. native (buttons/sticks/triggers-as-digital-input):**

- [ ] All 14 digital buttons (face, D-pad, L1/R1/L3/R3, Start/Select) report identically to native.
- [ ] Both sticks: X and Y axes match native (confirm Y axis inversion correction works on real
      hardware; SDL's raw convention is down-positive, this backend inverts it).
- [ ] Both analog triggers (as continuous 0-1 input) match native range and feel.

**Adaptive triggers via the raw PS5 HID compiler (`SDL_SendJoystickEffect`):**

- [ ] Scene re-run: all 7 modes (Off, Feedback, Weapon, Vibration, SlopeFeedback,
      MultiPositionFeedback, MultiPositionVibration) feel correct over SDL.
- [ ] Feel parity notes: record any perceptible differences from the native backend per mode.
      The compiler intentionally uses raw PS5 protocol (not Apple's normalized API), so divergence
      is expected and measurable (see README's "Trigger feel parity" matrix item).
- [ ] Per-trigger targeting (L2 vs R2 vs Both) isolates correctly; explicit clears between steps.
- [ ] `dualsense_trigger_clear` returns to neutral cleanly from every mode.

**Rumble (via `SDL_RumbleJoystick`) and light bar (via `SDL_SetJoystickLED`):**

- [ ] `dualsense_rumble 1 0` / `0 1` / `0 0`: produces felt rumble, stops on `0 0`, does NOT
      auto-timeout (confirms indefinite duration holds on real firmware).
- [ ] `dualsense_lightbar`: sets color correctly; compare hue/brightness vs. native for any
      visible discrepancy.
- [ ] Rumble + active trigger effect coexist without interference (confirms the porting guide's
      "trigger effects survive rumble/LED writes" finding holds through SDL's driver too).

**Haptic-pulse/buzz degradation (rumble-emulation, NOT CoreHaptics):**

- [ ] `dualsense_pulse`: produces felt rumble burst (not silence); confirm degrade path reaches
      hardware and the degrade-notice log appears once per gamepad instance, not per call.
- [ ] Feel quality: compare vs. native CoreHaptics tap/buzz. Expected: felt like ordinary rumble,
      not a crisp tap. Record how different it actually feels.
- [ ] Weapon fire edge (`OnWeaponTriggerFired` event): confirm NEVER fires under SDL backend
      (expected -- no adaptive-trigger status API exists on this path; this is documented in the
      README Capability matrix).

**GameController coexistence (two-writers scenario):**

- [ ] Pad A on SDL backend, Pad B left on stock engine's native GameController.framework
      simultaneously connected: confirm no input/effect interference between the two (this is the
      actual "two writers" scenario — SDL and GameController.framework both live in-process). Record
      any surprises.
- [ ] Single pad with `dualsense_backend sdl` active: verify the pad does NOT also feed a stock
      GameController gamepad slot (no duplicate input on two engine slots — the 2.6 playerIndex
      forensics predict stock GC impls may re-claim a playerIndex-Unset pad; record what actually
      happens).
- [ ] Disconnect pad A (sdl-owned) mid-effect: no crash, "restoring platform default" log, clean
      teardown; reconnect: monitor re-detects and re-takes-over within a tick or two.

**Idle-stability and Bluetooth (if pairing available):**

BT prerequisites (see README "Bluetooth prerequisites (macOS, SDL backend)" for the full
explanation): grant **Input Monitoring** to the Editor/app — **System Settings > Privacy &
Security > Input Monitoring** on macOS 13+, or **System Preferences > Security & Privacy >
Privacy > Input Monitoring** on macOS 11-12 (this gem's floor is 11.3) — before/when the console
logs the not-granted warning. The first sdl activation may prompt, or may silently deny with no
prompt at all; dev (ad-hoc-signed) binaries can lose this grant on rebuild (TCC is tied to code
identity, not just path) even with no config change, so re-check it if a previously-working BT
session starts warning again after a rebuild.
Pair via **PS + Create** held until the light bar flashes rapidly, and **unplug USB** while testing
BT to avoid a dual-connection ambiguity (plugged-in + paired at once). The GUID first byte in the
transport log (`0x03` USB / `0x05` Bluetooth) is the ground truth for which link is actually active
— trust it over assumptions about cable/pairing state.

- [ ] Idle-stability: SDL backend active, no input/effects applied, let it sit for several minutes
      (confirm no spurious log spam, no phantom inputs, clean idling).
- [ ] Bluetooth if available: repeat all items above over Bluetooth; GUID byte should be `0x05`
      (vs. `0x03` USB). Record any Bluetooth-specific behavior differences vs. USB.
- [ ] First sdl activation over BT: permission warn absent (or granted), pad enumerates, GUID logs 05
