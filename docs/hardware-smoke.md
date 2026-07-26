# Hardware smoke checklist

Run after each phase lands. Record date + result per line.

## Phase 0 — swap proof (Editor, no hardware needed)
- [ ] `dualsense_debug_swap` logs install line, no errors
- [ ] `dualsense_debug_restore` restores; no "skipped" warning in Editor
- [ ] Regular paired controller (if any) still works after restore

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
