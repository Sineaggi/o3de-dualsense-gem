# DualSense Phase 3a — SDL3 Foundation (feature-flagged, Mac-provable) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The cross-platform SDL3 transport layer, compiled behind build+runtime flags, provable on macOS today: SDL3 fetched joystick-only, the pure DS5 effects-packet/trigger-block compiler with byte-vector tests, and an SDL-backed gamepad implementation selectable via `dualsense_backend sdl` — zero behavior change with flags at defaults.

**Architecture:** Per spec §4.4 (as amended for SDL3, commit 612e0d5) and the hardware-validated reference at `~/pong/docs/dualsense-porting-guide.md` (+ its reference implementation `~/pong/dualsense/src/` — both READ-ONLY; `ds5_effects.{h,cpp}` contains unit-tested packet builders with exact byte vectors to mirror, minus the two upstream Godot-PR bugs the guide documents in §"The 11-byte trigger effect blocks"). Build gate: `PAL_TRAIT_DUALSENSE_SDL_BACKEND` (Mac/Windows/Linux TRUE, mobile FALSE) → SDL3 fetch + `DUALSENSE_SDL_BACKEND_ENABLED` compile def. Runtime gate: cvar `dualsense_backend` (`native`|`sdl`, default `native` — the Mac native path stays the shipping default; `sdl` swaps the monitor/impl to the SDL stack for A/B hardware testing). Known open question this phase answers empirically: SDL-vs-GameController coexistence on macOS (two-writers risk on this one platform).

## Global Constraints

Branch `feature/phase-3a-sdl` stacked on `feature/phase-2.6-autofire` head (612e0d5) — 2.6's hardware pass runs on this tree, so **flag-off behavior must be bit-identical to 2.6** (default cvar `native`, no SDL init unless selected). Build targets/test commands as established; suite baseline **63/63**, grows with pure-protocol tests. Zero engine modifications. MRC/idioms bind for any .mm touched. Existing frozen APIs untouched. Trailer `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`; never commit red. SDL3 license zlib (record in 3rdParty/ per MiniAudio's license-file convention).

---

### Task 1: SDL3 fetch + trait/flag plumbing (no behavior change)

**Files:** `3rdParty/FindSDL3.cmake` (+ `3rdParty/Installer/FindSDL3.cmake`), root `CMakeLists.txt` (CMAKE_MODULE_PATH append, MiniAudio pattern), `Code/Platform/{Mac,Windows,Linux}/PAL_*.cmake` (`set(PAL_TRAIT_DUALSENSE_SDL_BACKEND TRUE)`; Android/iOS FALSE), `Code/CMakeLists.txt` (conditional `3rdParty::SDL3` dependency + `DUALSENSE_SDL_BACKEND_ENABLED` compile def on Private.Object when trait set — Microphone's conditional-3rdParty pattern), `Code/Source/Clients/DualSenseSystemComponent.cpp` (cvar `dualsense_backend`, AZ_CVAR string, default "native", DontReplicate, no consumer yet — comment says Task 3 consumes).

- `o3de_fetch_content` SDL3 (pin the current stable release tag + hash; joystick-only: `SDL_AUDIO=OFF SDL_VIDEO=OFF SDL_RENDER=OFF SDL_GPU=OFF SDL_CAMERA=OFF SDL_HAPTIC=OFF SDL_POWER=OFF SDL_SENSOR=ON` — sensors stay ON, phase 5 wants them; static lib; verify the actual SDL3 CMake option names against the fetched source, record corrections). Follow MiniAudio's Find module shape exactly incl. `ly_install` lines and target-name collision guidance.
- Gate: full build on Mac with the trait TRUE links SDL3; suite 63/63 unchanged; a trivial compile-test TU (`#if DUALSENSE_SDL_BACKEND_ENABLED` + `SDL_GetVersion` call in a gem-internal sanity function, exercised by one new unit test asserting the linked SDL version ≥ 3.0) proves fetch+link+flag plumbing without touching runtime behavior.
- Commit `build: SDL3 (joystick-only) fetched behind PAL_TRAIT_DUALSENSE_SDL_BACKEND; dualsense_backend cvar stub`.

### Task 2: Pure DS5 protocol — effects packet + raw trigger-block compiler (TDD, byte vectors)

**Files:** `Code/Include/DualSense/DualSenseDs5Protocol.h`, `Code/Source/Clients/DualSenseDs5Protocol.cpp`, tests file, cmake lists. Pure C++ — builds on every platform regardless of trait.

- 47-byte effects packet struct per the guide §"The packet": valid-flag bits (right-trigger byte0 bit2, left byte0 bit3 — set ONLY what's changing), trigger blocks at bytes 10–20 / 21–31, `uint8_t[4]` timestamp field (the packing trap), `static_assert(sizeof == 47)`.
- Raw 11-byte compiler `CompileTriggerEffectRaw(const TriggerEffect&) -> AZStd::array<AZ::u8,11>` per the guide's table: Off `0x05`; Feedback `0x21` zone-mask + 3-bit force packing; Weapon `0x25` `(1<<start)|(1<<end)`, byte3 strength-1; Vibration `0x26` + byte9 frequency Hz (our normalized `m_frequency` × 255, rounded); multi/slope via per-zone arrays into `0x21`/`0x26`. **Explicit regression tests for the two upstream bugs:** multi-vibration amplitude packs with `& 0x07` masking per-zone (NOT `(amp-1)*0x07`), slope validates strengths not positions. Zone/strength quantization from normalized floats: positions → zone 0–9 (`round(p*9)`), strengths → 0–8 (`round(s*8)`).
- Byte-vector tests: mirror `~/pong/dualsense/src/ds5_effects.cpp`'s unit-test vectors (read it; cite which vectors were ported); plus round-trip sanity vs our existing `DegradeToBaselineApi` semantics (mode coverage for all 7 TriggerEffectMode values).
- Commit `feat: DS5 effects-packet + raw trigger-block compiler (byte-vector tested)`.

### Task 3: SDL runtime + SDL-backed gamepad implementation (cvar-selected)

**Files:** `Code/Source/Clients/Sdl/DualSenseSdlRuntime.{h,cpp}`, `Code/Source/Clients/Sdl/InputDeviceGamepadDualSenseSdl.{h,cpp}`, `Code/Source/Clients/Sdl/DualSenseSdlMonitor.{h,cpp}` (all inside `#if DUALSENSE_SDL_BACKEND_ENABLED`), system-impl selection, cmake.

- `DualSenseSdlRuntime`: SDL_Init(SDL_INIT_JOYSTICK|SDL_INIT_SENSOR) lazily on first `sdl` selection (never at module load), hints pinned (`SDL_HINT_JOYSTICK_HIDAPI_PS5=1`; disable drivers we don't need — verify hint names against fetched SDL3 headers), pump via system tick, SDL_Quit on release. Enumeration: `SDL_GetJoysticks` + `SDL_GetGamepadTypeForID == SDL_GAMEPAD_TYPE_PS5`; GUID bus-byte logged (03 USB / 05 BT — the guide's transport-detection trick).
- `InputDeviceGamepadDualSenseSdl : InputDeviceGamepad::Implementation`: input via SDL joystick state mapped to the shared button map/RawGamepadState (axis ranges: SDL −32768..32767 sticks, 0..32767 triggers — normalize); SetVibration/lightbar/trigger-bus handler/pulse-bus handler → effects packets through `SDL_SendJoystickEffect` (rumble emulation bytes + valid flags; CoreHaptics NOT available on this path — PlayHapticPulse/Buzz degrade to rumble-emulation approximations with a documented comment; this asymmetry is expected and logged once).
- Backend selection: `DualSenseSystemImpl` (Mac) consults the cvar at activation and on change (BarrierInput's cvar-callback pattern): `native` → existing GC monitor; `sdl` → SdlMonitor (SDL hotplug events polled in tick; same slot-tracker + swap calls). Switching backends live: restore all slots, tear down old stack, bring up new.
- Hardware smoke additions (unchecked): `dualsense_backend sdl` → scene re-run over SDL on macOS (input, triggers via raw compiler!, rumble emulation), coexistence observations vs GC, switch back to native live. This is the empirical answer to the Mac two-writers question — record findings.
- Commit `feat: SDL3 backend behind dualsense_backend cvar (mac-provable phase-3 transport)`.

### Task 4: docs + wrap
README backend section (flags, cvar, per-backend capability matrix — CoreHaptics vs rumble-emulation asymmetry), smoke section, final review. NO tag (rides to main with 2.6-stacked merge sequencing).

## Self-review notes
- Task 2's compiler is deliberately independent of Apple's normalized API (raw Hz byte, zone quantization) — the SDL path uses it; the Mac native path keeps using GCDualSenseAdaptiveTrigger. Divergence in feel between backends is measurable via the test scene — that's a feature.
- Flag-off purity is a hard gate at every task: no SDL_Init, no static SDL state, suite green, 2.6 behavior identical.
- Stacked-branch bookkeeping: phase-2.6 tag goes on 612e0d5 after the pending hardware pass; this branch merges after.
