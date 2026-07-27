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

- [ ] `7` autofire at the default 0.098 (assumed 25 Hz) -> feels like repeated-fire thumps, not a
      fine buzz
- [ ] `[` / `]` sweep the frequency in 0.02 steps while holding the trigger pulled -> find the
      value that feels like the reference implementation's 25 Hz repeated fire
- [ ] **Record the locked-in frequency here:** _______________ (replaces the 0.098 assumption in
      `DualSenseSystemComponent.cpp`'s `CreateTriggerEffectForMode("autofire")` once confirmed)

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

## Phase 3a Task 3: SDL3 backend (`dualsense_backend sdl`), macOS

All unchecked -- Task 4 will fold results/corrections into the README. Run the same test
scene/console commands as the native-backend items above, but with `dualsense_backend sdl` set
(console command, or `+dualsense_backend sdl` on the command line) before/while a DualSense is
connected. This is the empirical answer to the Mac "two writers" question the plan called out:
GameController.framework (stock engine + this gem's native backend) and SDL3 (this gem's sdl
backend) both being able to open the same USB/BT DualSense from the same process.

**Backend selection / flag purity:**

- [ ] Default (`dualsense_backend` left at `native`): confirm via Console/log that no
      `DualSense (SDL):` log lines ever appear and no SDL_Init cost is paid -- behavior is
      bit-for-bit identical to pre-Task-3.
- [ ] `dualsense_backend sdl` typed at the console with a DualSense already connected -> native
      GameController slot is released (log: "restoring platform default"), SDL backend activates
      (log: "backend activated"), monitor detects the same physical pad within a tick or two (log:
      "controller detected, taking over gamepad slot N") and its GUID bus byte is logged (`0x03`
      USB / `0x05` Bluetooth -- confirm it matches the actual cable/BT state).
- [ ] `dualsense_backend native` typed back while sdl is active -> reverse of the above (SDL slot
      restored, SDL backend deactivated/`SDL_Quit`'d, native GameController stack reclaims the pad
      within a tick or two).
- [ ] Rapid back-and-forth (`sdl`/`native`/`sdl`/...) several times in a row -> no crash, no
      duplicate slot claims, no leaked SDL_Gamepad handles (watch for repeated "closing gamepad
      handle" debug logs matching each activation).
- [ ] Two physical DualSense pads connected simultaneously while switching backends -> both slots
      swap together, no cross-wiring (pad A never ends up driving pad B's slot).

**Input parity vs. native (buttons/sticks/triggers-as-digital-input):**

- [ ] All 14 digital buttons (face buttons, D-pad, L1/R1/L3/R3, Start/Select) report identically to
      the native backend.
- [ ] Both sticks: X axis matches native (no left/right flip); Y axis matches native (no up/down
      flip -- this is the axis this backend inverts in code, SDL's raw convention being
      down-positive; confirm the inversion is actually correct on real hardware, not just by
      inspection).
- [ ] Both analog triggers (as continuous 0-1 input, independent of any adaptive-trigger effect)
      match native's feel/range.

**Adaptive triggers via the raw HID compiler (`SDL_SendJoystickEffect`):**

- [ ] Every mode reachable from `dualsense_trigger`/the test scene's keys (Off, Feedback, Weapon,
      Vibration, SlopeFeedback, MultiPositionFeedback, MultiPositionVibration) feels correct over
      SDL, both USB and (separately, deliberately) Bluetooth.
- [ ] Per-trigger targeting (L2 vs R2 vs Both) isolates correctly, with explicit clears between
      steps (per the porting guide: a single-trigger write doesn't clear the other trigger --
      confirm that's still true through this backend).
- [ ] `dualsense_trigger_clear` returns to neutral cleanly from every mode above.

**Rumble / light bar (SDL_RumbleJoystick / SDL_SetJoystickLED):**

- [ ] `dualsense_rumble` produces felt rumble, stops when re-issued with `0 0`, and does NOT
      silently stop before the caller says so (validates the "indefinite duration" choice in
      `InputDeviceGamepadDualSenseSdl::SetVibration` -- let it run for several minutes unattended
      if practical, confirming it does not time out early on real firmware).
- [ ] `dualsense_lightbar` sets the color correctly; compare hue/brightness against the native
      (CoreHaptics-adjacent GameController) light bar path for any visible discrepancy.
- [ ] Rumble and an active trigger effect coexist without interference (mirrors the native-backend
      coexistence checks above) -- confirms the porting guide's "trigger effects survive engine
      rumble/LED writes" finding holds through SDL's own PS5 driver too.

**Haptic-pulse/buzz degradation (rumble-emulation approximation, NOT CoreHaptics):**

- [ ] `dualsense_pulse` produces a felt rumble burst (not silence) -- confirm the degrade path
      actually reaches hardware, and that the one-time `AZLOG_DEBUG` degrade-notice appears exactly
      once per gamepad instance, not once per call.
- [ ] Compare the felt quality against the native backend's CoreHaptics tap/buzz -- expected to
      feel like ordinary rumble, not a crisp haptic tap; record how different it actually feels.
- [ ] `SetAutoRecoil` / weapon-fire-edge behavior: confirm `OnWeaponTriggerFired` never fires under
      `dualsense_backend sdl` (no adaptive-trigger status API on this path -- expected, not a bug;
      this is the item Task 4's README note documents).

**Coexistence with the stock engine / a second physical pad on the OTHER backend:**

- [ ] With pad A on `dualsense_backend sdl` and a second physical DualSense (pad B) left on the
      stock engine's native GameController backend (never taken over -- e.g. temporarily rename
      away Task 3's cvar effect, or test before switching) simultaneously connected: confirm pad A
      and pad B do not interfere with each other's input/effects. This is the actual two-writers
      scenario (SDL and GameController.framework both live in-process against different physical
      units); record any surprises.
- [ ] Disconnect pad A (sdl-owned) mid-effect -> no crash, "restoring platform default" log, clean
      teardown; reconnect -> monitor re-detects and re-takes-over within a tick or two.

**Deferred, consistent with all prior phases' BT deferral policy:**

- [ ] Full Bluetooth pass of every item above (this section assumes USB-first verification, same
      as every earlier phase in this gem).
