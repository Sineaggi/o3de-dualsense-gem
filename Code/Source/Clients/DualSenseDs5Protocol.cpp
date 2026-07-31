#include <DualSense/DualSenseDs5Protocol.h>

#include <AzCore/std/algorithm.h>

#include <cmath>

// See DualSenseDs5Protocol.h for the overall porting-source citation and quantization
// summary. Per-mode layouts below mirror the porting guide's "11-byte trigger effect
// blocks" table, itself ported from the pong reference's ds5_effects.cpp build_*
// functions (see the header for what "pong" is).

namespace DualSense
{
    namespace
    {
        constexpr AZ::u8 EffectModeOff = 0x05;
        constexpr AZ::u8 EffectModeFeedback = 0x21;
        constexpr AZ::u8 EffectModeWeapon = 0x25;
        constexpr AZ::u8 EffectModeVibration = 0x26;

        constexpr size_t ZoneCount = 10;

        //! round(value * scale), clamped to [0, maxValue]. Shared by every normalized-float
        //! quantization in this file (zones, strengths, frequency) -- see header doc comment
        //! for the three concrete domains this is used for.
        AZ::u8 QuantizeClamped(float value, float scale, AZ::u8 maxValue)
        {
            const float scaled = std::round(value * scale);
            if (scaled <= 0.0f)
            {
                return 0;
            }
            if (scaled >= static_cast<float>(maxValue))
            {
                return maxValue;
            }
            return static_cast<AZ::u8>(scaled);
        }

        AZ::u8 QuantizeZone(float normalizedPosition)
        {
            return QuantizeClamped(normalizedPosition, 9.0f, 9);
        }

        AZ::u8 QuantizeStrength(float normalizedValue)
        {
            return QuantizeClamped(normalizedValue, 8.0f, 8);
        }

        AZ::u8 QuantizeFrequencyByte(float normalizedFrequency)
        {
            return QuantizeClamped(normalizedFrequency, 255.0f, 255);
        }

        //! Packs per-zone strength levels (0-8, 0 = zone inactive) into the active-zone
        //! bitmask (bytes 1-2) + 3-bit-per-zone force field (bytes 3-6) shared by Feedback
        //! (0x21) and Vibration (0x26) payloads. This is the CORRECT packing -- force masked
        //! with `& 0x07` per zone -- used uniformly for every mode below, including
        //! MultiPositionVibration, where the upstream Godot draft PR instead used
        //! `(amplitude - 1) * 0x07` (a bug: multiplication overflows 3 bits and corrupts
        //! neighboring zones for amplitude >= 3). Mirrors pong's build_feedback /
        //! build_vibration / build_multi_feedback / build_multi_vibration (post-fix) bodies.
        void PackZonesFromStrengths(const AZStd::array<AZ::u8, ZoneCount>& zoneStrengths, AZ::u8 modeByte, AZStd::array<AZ::u8, Ds5TriggerBlockSize>& out)
        {
            out.fill(0);
            out[0] = modeByte;

            AZ::u16 activeZones = 0;
            AZ::u32 forceZones = 0;
            for (size_t zone = 0; zone < ZoneCount; ++zone)
            {
                if (zoneStrengths[zone] > 0)
                {
                    const AZ::u8 forceValue = static_cast<AZ::u8>((zoneStrengths[zone] - 1) & 0x07);
                    forceZones |= static_cast<AZ::u32>(forceValue) << (3 * zone);
                    activeZones |= static_cast<AZ::u16>(1u << zone);
                }
            }

            out[1] = static_cast<AZ::u8>(activeZones & 0xFF);
            out[2] = static_cast<AZ::u8>((activeZones >> 8) & 0xFF);
            out[3] = static_cast<AZ::u8>(forceZones & 0xFF);
            out[4] = static_cast<AZ::u8>((forceZones >> 8) & 0xFF);
            out[5] = static_cast<AZ::u8>((forceZones >> 16) & 0xFF);
            out[6] = static_cast<AZ::u8>((forceZones >> 24) & 0xFF);
        }

        //! Feedback/Vibration (single-position) zone fill: a uniform `strength` level is
        //! applied to every zone from `startZone` to 9 inclusive ("...active at uniform
        //! strength" per the guide; m_endPosition is not used by these two modes -- the
        //! effect always runs to the end of trigger travel, matching pong's build_feedback /
        //! build_vibration, which take only a single `position` parameter). Callers are
        //! expected to have already redirected `strength == 0` to the literal Off block (see
        //! CompileTriggerEffectRaw) -- the `strength > 0` guard here is just defensive, so
        //! this never wraps to the maximum force value via an unclamped `0 - 1` if called
        //! directly with 0.
        AZStd::array<AZ::u8, ZoneCount> DenseZonesFrom(AZ::u8 startZone, AZ::u8 strength)
        {
            AZStd::array<AZ::u8, ZoneCount> zoneStrengths{};
            if (strength > 0)
            {
                for (size_t zone = startZone; zone < ZoneCount; ++zone)
                {
                    zoneStrengths[zone] = strength;
                }
            }
            return zoneStrengths;
        }

        //! True if any zone in `zoneStrengths` is active (level > 0). Used by the
        //! MultiPosition* modes to decide whether to redirect to the literal Off block --
        //! mirrors pong's `have_positive_values` scan in joy_adaptive_triggers_multi_feedback
        //! / joy_adaptive_triggers_multi_vibration (dualsense.cpp).
        bool AnyZoneActive(const AZStd::array<AZ::u8, ZoneCount>& zoneStrengths)
        {
            for (AZ::u8 level : zoneStrengths)
            {
                if (level > 0)
                {
                    return true;
                }
            }
            return false;
        }

        //! Per-zone quantized levels straight from TriggerEffect::m_positionalValues, for the
        //! MultiPosition* modes. Matches pong's build_multi_feedback / build_multi_vibration:
        //! each zone's own value decides both activation (>0) and force, independent of every
        //! other zone.
        AZStd::array<AZ::u8, ZoneCount> ZonesFromPositionalValues(const AZStd::array<float, ZoneCount>& positionalValues)
        {
            AZStd::array<AZ::u8, ZoneCount> zoneStrengths{};
            for (size_t zone = 0; zone < ZoneCount; ++zone)
            {
                zoneStrengths[zone] = QuantizeStrength(positionalValues[zone]);
            }
            return zoneStrengths;
        }

        //! SlopeFeedback's per-zone ramp: constant 0 before startZone, linear ramp from
        //! startStrength to endStrength across [startZone, endZone], constant endStrength
        //! after. Matches pong's build_slope_feedback exactly (same formula, same rounding),
        //! feeding the result into the same multi-feedback packer as MultiPositionFeedback.
        //!
        //! Bug-regression note (second upstream Godot bug): startZone/endZone here are
        //! already clamped to the zone domain [0,9] by QuantizeZone, and startStrength/
        //! endStrength are already clamped to the strength domain [0,8] by QuantizeStrength,
        //! by construction -- the upstream draft instead validated `start_position > 8` where
        //! it meant `start_strength > 8`, which both left position wrongly capped below its
        //! real maximum (9) and left strength unclamped. Using the dedicated Quantize*
        //! function for each field, rather than one shared ad hoc clamp, is what prevents
        //! that mix-up here.
        AZStd::array<AZ::u8, ZoneCount> SlopeZones(AZ::u8 startZone, AZ::u8 endZone, AZ::u8 startStrength, AZ::u8 endStrength)
        {
            if (endZone < startZone)
            {
                endZone = startZone;
            }

            AZStd::array<AZ::u8, ZoneCount> zoneStrengths{};
            const float zoneSpan = static_cast<float>(endZone) - static_cast<float>(startZone);
            const float slope = (zoneSpan == 0.0f) ? 0.0f : (static_cast<float>(endStrength) - static_cast<float>(startStrength)) / zoneSpan;

            for (size_t zone = 0; zone < ZoneCount; ++zone)
            {
                if (zone < startZone)
                {
                    zoneStrengths[zone] = 0;
                }
                else if (zone <= endZone)
                {
                    const float ramped = std::round(startStrength + slope * (static_cast<float>(zone) - startZone));
                    zoneStrengths[zone] = static_cast<AZ::u8>(AZStd::clamp(ramped, 0.0f, 8.0f));
                }
                else
                {
                    zoneStrengths[zone] = endStrength;
                }
            }
            return zoneStrengths;
        }
    } // namespace

    AZStd::array<AZ::u8, Ds5TriggerBlockSize> CompileTriggerEffectRaw(const TriggerEffect& effect)
    {
        AZStd::array<AZ::u8, Ds5TriggerBlockSize> out{};

        switch (effect.m_mode)
        {
        case TriggerEffectMode::Off:
            out[0] = EffectModeOff;
            break;

        case TriggerEffectMode::Feedback:
        {
            const AZ::u8 zone = QuantizeZone(effect.m_startPosition);
            const AZ::u8 strength = QuantizeStrength(effect.m_strength);
            // Matches the validated reference's caller-level redirect
            // (joy_adaptive_triggers_feedback, dualsense.cpp:119-122): strength == 0 compiles
            // to the literal Off block, not an empty-zoned 0x21.
            if (strength == 0)
            {
                out[0] = EffectModeOff;
                break;
            }
            PackZonesFromStrengths(DenseZonesFrom(zone, strength), EffectModeFeedback, out);
            break;
        }

        case TriggerEffectMode::Weapon:
        {
            // Guide-validated ranges: start 2-7, end start+1..8. Clamp quantized zones into
            // that window rather than rejecting out-of-range input.
            const AZ::u8 rawStartZone = QuantizeZone(effect.m_startPosition);
            const AZ::u8 startZone = AZStd::clamp<AZ::u8>(rawStartZone, 2, 7);
            const AZ::u8 rawEndZone = QuantizeZone(effect.m_endPosition);
            const AZ::u8 endZone = AZStd::clamp<AZ::u8>(rawEndZone, static_cast<AZ::u8>(startZone + 1), 8);

            // No "0 = off" shortcut for a weapon break -- clamp to a minimum of 1 so byte3
            // never underflows to 0xFF.
            const AZ::u8 strength = AZStd::max<AZ::u8>(QuantizeStrength(effect.m_strength), 1);

            const AZ::u16 mask = static_cast<AZ::u16>((1u << startZone) | (1u << endZone));
            out[0] = EffectModeWeapon;
            out[1] = static_cast<AZ::u8>(mask & 0xFF);
            out[2] = static_cast<AZ::u8>((mask >> 8) & 0xFF);
            out[3] = static_cast<AZ::u8>(strength - 1);
            break;
        }

        case TriggerEffectMode::Vibration:
        {
            const AZ::u8 zone = QuantizeZone(effect.m_startPosition);
            const AZ::u8 strength = QuantizeStrength(effect.m_strength);
            const AZ::u8 frequencyByte = QuantizeFrequencyByte(effect.m_frequency);
            // Matches joy_adaptive_triggers_vibration (dualsense.cpp:145-148): amplitude == 0
            // OR frequency == 0 both redirect to the literal Off block, not a stale/zero-freq
            // 0x26 with an empty or meaningless zone mask.
            if (strength == 0 || frequencyByte == 0)
            {
                out[0] = EffectModeOff;
                break;
            }
            PackZonesFromStrengths(DenseZonesFrom(zone, strength), EffectModeVibration, out);
            out[9] = frequencyByte;
            break;
        }

        case TriggerEffectMode::MultiPositionFeedback:
        {
            const auto zoneStrengths = ZonesFromPositionalValues(effect.m_positionalValues);
            // Matches joy_adaptive_triggers_multi_feedback's have_positive_values scan
            // (dualsense.cpp:157-172): all-zero zones redirect to the literal Off block.
            if (!AnyZoneActive(zoneStrengths))
            {
                out[0] = EffectModeOff;
                break;
            }
            PackZonesFromStrengths(zoneStrengths, EffectModeFeedback, out);
            break;
        }

        case TriggerEffectMode::MultiPositionVibration:
        {
            const auto zoneStrengths = ZonesFromPositionalValues(effect.m_positionalValues);
            const AZ::u8 frequencyByte = QuantizeFrequencyByte(effect.m_frequency);
            // Matches joy_adaptive_triggers_multi_vibration (dualsense.cpp:191-206): all-zero
            // amplitudes OR frequency == 0 both redirect to the literal Off block.
            if (!AnyZoneActive(zoneStrengths) || frequencyByte == 0)
            {
                out[0] = EffectModeOff;
                break;
            }
            PackZonesFromStrengths(zoneStrengths, EffectModeVibration, out);
            out[9] = frequencyByte;
            break;
        }

        case TriggerEffectMode::SlopeFeedback:
        {
            const AZ::u8 startZone = QuantizeZone(effect.m_startPosition);
            const AZ::u8 endZone = QuantizeZone(effect.m_endPosition);
            const AZ::u8 startStrength = QuantizeStrength(effect.m_strength);
            const AZ::u8 endStrength = QuantizeStrength(effect.m_endStrength);
            PackZonesFromStrengths(SlopeZones(startZone, endZone, startStrength, endStrength), EffectModeFeedback, out);
            break;
        }

        default:
            break;
        }

        return out;
    }

} // namespace DualSense
