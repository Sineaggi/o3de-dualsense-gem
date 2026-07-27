#pragma once

// Pure DualSense DS5 wire-protocol packet builders. No SDL includes, no platform code --
// this header (and its .cpp) build unconditionally on every platform, regardless of
// PAL_TRAIT_DUALSENSE_SDL_BACKEND. Semantics ported from
// /Users/claytonwalker/pong/dualsense/src/ds5_effects.{h,cpp} (hardware-validated Godot
// GDExtension reference, itself derived from Godot draft PR #111682 / Nielk1's DualSense
// trigger-effect gist, MIT), with the two known upstream bugs fixed rather than ported --
// see CompileTriggerEffectRaw below. Field/layout reference:
// https://controllers.fandom.com/wiki/Sony_DualSense#Output_Reports

#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>

#include <DualSense/DualSenseTriggerEffects.h>

namespace DualSense
{
    //! Size in bytes of one trigger effect's raw command block within the DS5 output report.
    constexpr size_t Ds5TriggerBlockSize = 11;

    //! The 47-byte DualSense "set state" output-report payload (matches SDL's
    //! DS5EffectsState_t framing; SDL itself adds the report id, Bluetooth wrapper, and CRC).
    //!
    //! Only the sections this gem actually drives are broken out into named fields; the
    //! remaining wire bytes (audio routing, LED/lightbar, player indicators, power-save bits,
    //! etc.) are represented as opaque reserved bytes so the struct still totals exactly 47
    //! bytes and every byte offset in the wire format is accounted for, without pulling in a
    //! full bitfield mirror of fields this gem never touches. Every member is a plain AZ::u8
    //! (or an array of them) -- no bitfields, no multi-byte integers -- so there is zero
    //! padding anywhere in the struct and the struct-packing trap the porting guide warns
    //! about (a naively-typed uint32_t timestamp silently growing the struct to 48 bytes via
    //! alignment padding) cannot occur here by construction; m_hostTimestamp is still called
    //! out explicitly as uint8_t[4] in the field comment for anyone tempted to "fix" it.
    struct Ds5EffectsPacket
    {
        //! Byte 0 valid-flag bits: only a flagged section is applied by the controller;
        //! everything else zero means "leave that section's current state alone". Bit
        //! positions used by this gem: bit2 = AllowRightTriggerFFB, bit3 = AllowLeftTriggerFFB.
        AZ::u8 m_validFlags0 = 0; // byte 0
        AZ::u8 m_validFlags1 = 0; // byte 1

        AZ::u8 m_rumbleRight = 0; // byte 2: RumbleEmulationRight
        AZ::u8 m_rumbleLeft = 0; // byte 3: RumbleEmulationLeft

        AZ::u8 m_reservedAudio[6] = {}; // bytes 4-9: volumes / mic-audio-path bits / mute-light mode / power-save bits (unused by this gem)

        AZStd::array<AZ::u8, Ds5TriggerBlockSize> m_rightTriggerFfb{}; // bytes 10-20
        AZStd::array<AZ::u8, Ds5TriggerBlockSize> m_leftTriggerFfb{}; // bytes 21-31

        //! Bytes 32-35: host timestamp. MUST stay uint8_t[4] (NOT uint32_t) -- see struct
        //! comment above; this is the exact field the porting guide's packing trap is about.
        AZ::u8 m_hostTimestamp[4] = {};

        AZ::u8 m_reservedLighting[11] = {}; // bytes 36-46: motor power reduction, speaker/audio control, light fade/brightness, player indicators, LED RGB (unused by this gem)

        //! Byte 0 bit for "apply m_rightTriggerFfb".
        static constexpr AZ::u8 RightTriggerFfbValidBit = 1 << 2;
        //! Byte 0 bit for "apply m_leftTriggerFfb".
        static constexpr AZ::u8 LeftTriggerFfbValidBit = 1 << 3;

        //! Copies `block` into m_rightTriggerFfb and sets RightTriggerFfbValidBit in
        //! m_validFlags0. Touches nothing else: does not zero or otherwise alter
        //! m_leftTriggerFfb, m_validFlags1, rumble, or any reserved byte, so a right-only
        //! write never disturbs left-trigger, rumble, or LED state already staged in this
        //! packet (mirrors the guide's "set only the flags for what you're changing").
        void SetRightTriggerBlock(const AZStd::array<AZ::u8, Ds5TriggerBlockSize>& block)
        {
            m_rightTriggerFfb = block;
            m_validFlags0 |= RightTriggerFfbValidBit;
        }

        //! Left-trigger counterpart of SetRightTriggerBlock; same isolation guarantee.
        void SetLeftTriggerBlock(const AZStd::array<AZ::u8, Ds5TriggerBlockSize>& block)
        {
            m_leftTriggerFfb = block;
            m_validFlags0 |= LeftTriggerFfbValidBit;
        }
    };
    static_assert(sizeof(Ds5EffectsPacket) == 47, "DS5 output-report payload must be exactly 47 bytes");

    //! Compiles a (already-normalized-float) TriggerEffect into the raw 11-byte DS5
    //! trigger-effect command block (mode byte + payload) for one trigger.
    //!
    //! Quantization from the gem's normalized [0,1] float fields to the protocol's integer
    //! domains (per the porting guide's validation ranges):
    //!   - positions  -> zone index 0-9:  round(position * 9),  clamped to [0, 9]
    //!   - strengths  -> level 0-8:       round(value * 8),     clamped to [0, 8]
    //!   - frequency  -> byte 0-255:      round(m_frequency * 255), clamped to [0, 255]
    //!
    //! "0 = off shortcut": the guide's validation-range note is implemented here exactly as
    //! the hardware-validated reference's caller layer implements it (pong's dualsense.cpp,
    //! joy_adaptive_triggers_feedback/_vibration/_multi_feedback/_multi_vibration) -- a
    //! zero-strength Feedback, a zero-amplitude-or-zero-frequency Vibration, or an
    //! all-zero-zones MultiPositionFeedback/MultiPositionVibration (also zero-frequency for
    //! the latter) compiles to the **literal Off block** (mode byte 0x05, rest zero), not an
    //! empty-zoned 0x21/0x26. This was corrected after review flagged the original
    //! empty-zoned-0x21/0x26 behavior as an unverified firmware-equivalence assumption never
    //! exercised on real hardware, unlike the validated Off redirect.
    //!
    //! Mode-specific notes (see .cpp for the full per-mode commentary):
    //!   - Weapon's break strength has no "0 = off" shortcut in this compiler (a weapon break
    //!     degenerate to zero force is meaningless), so it is clamped to a minimum of 1 before
    //!     the `strength - 1` byte is written; Weapon's start/end zones are additionally
    //!     clamped into the guide's validated ranges (start 2-7, end start+1..8). Note: pong's
    //!     caller layer redirects Weapon strength == 0 to Off too; this compiler's minimum-1
    //!     clamp is a deliberate, narrower design choice for this mode only.
    //!   - MultiPositionVibration packs each zone's quantized amplitude with `& 0x07`, never
    //!     the upstream Godot draft's `(amplitude - 1) * 0x07` multiplication bug (which
    //!     corrupts neighboring zones' 3-bit fields for amplitude >= 3).
    //!   - SlopeFeedback clamps m_strength/m_endStrength (not m_startPosition/m_endPosition)
    //!     to the 0-8 strength domain, unlike the upstream Godot draft, which validated
    //!     `start_position > 8` where it meant `start_strength > 8`.
    AZStd::array<AZ::u8, Ds5TriggerBlockSize> CompileTriggerEffectRaw(const TriggerEffect& effect);

} // namespace DualSense
