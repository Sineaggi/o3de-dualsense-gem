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
