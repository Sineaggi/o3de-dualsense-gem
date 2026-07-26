# Hardware smoke checklist

Run after each phase lands. Record date + result per line.

## Phase 0 — swap proof (Editor, no hardware needed)
- [x] `dualsense_debug_swap` logs install line, no errors (2026-07-26, Editor profile build)
- [x] `dualsense_debug_restore` restores; no "skipped" warning in Editor (2026-07-26)
- [x] Regular paired controller (if any) still works after restore — N/A, no controller paired (2026-07-26)

## Phase 1 — Mac input

With a DualSense connected via the testbed Editor. Try BOTH USB and Bluetooth.

- [ ] Console shows the takeover log from Task 7 ("DualSense: controller detected, taking over gamepad slot N")
- [ ] Entity with an Input component bound to a default `.inputbindings` asset (or any input-driven sample) receives events for:
  - [ ] All 4 face buttons (cross/circle/square/triangle -> A/B/X/Y)
  - [ ] D-pad (up/down/left/right)
  - [ ] Shoulders (L1/R1)
  - [ ] Stick clicks (L3/R3)
  - [ ] Menu/Options (Start/Select)
  - [ ] Both analog triggers report gradual (non-binary) values across their range
  - [ ] Both sticks report full range of motion with no drift at rest
- [ ] Sticks at rest produce no held events (deadzones effective)
- [ ] Repeat the above over Bluetooth (not just USB)
- [ ] With DualSense connected, only ONE gamepad slot reports connected (no ghost second slot in logs)

## Phase 1 — rumble

With a DualSense connected via the testbed Editor. Try BOTH USB and Bluetooth.

- [ ] `dualsense_rumble 1 0` -> strong vibration concentrated on the left grip
- [ ] `dualsense_rumble 0 1` -> strong vibration concentrated on the right grip
- [ ] `dualsense_rumble 0 0` -> silence
- [ ] `dualsense_rumble 0.2 0.2` -> clearly weaker than `dualsense_rumble 1 1`
- [ ] Disconnect mid-rumble -> no crash, clean restore logs (FAILED 2026-07-26: NSException from dead CHHapticEngine + off-main notification delivery — fixed, re-verify)

## Phase 1 — lightbar

With a DualSense connected via the testbed Editor. Try BOTH USB and Bluetooth.

- [ ] `dualsense_lightbar 1 0 0` -> red
- [ ] `dualsense_lightbar 0 1 0` -> green
- [ ] `dualsense_lightbar 0 0 1` -> blue
- [ ] `dualsense_lightbar 1 1 1` -> white
- [ ] Disconnect mid-color-change -> no crash, clean restore logs
- [ ] Reconnect -> lightbar state sane
