#include <AzCore/UnitTest/TestTypes.h>
#include <DualSense/DualSenseDs5Protocol.h>
#include <DualSense/DualSenseTriggerEffectMapping.h>

#include <cstring>

// Byte vectors in this file are ported from the hardware-validated reference implementation's
// own unit tests: /Users/claytonwalker/pong/dualsense/tests/test_ds5_effects.cpp (which exercises
// /Users/claytonwalker/pong/dualsense/src/ds5_effects.cpp). See per-test comments for which
// pong vector each one mirrors, and DualSenseDs5Protocol.h for the normalized-float ->
// protocol-integer quantization this gem adds on top (the pong reference takes pre-quantized
// ints directly).

namespace DualSenseTests
{
    using DualSense::Ds5EffectsPacket;
    using DualSense::Ds5TriggerBlockSize;
    using DualSense::TriggerEffect;
    using DualSense::TriggerEffectMode;
    using DualSense::CompileTriggerEffectRaw;

    using Ds5TriggerBlock = AZStd::array<AZ::u8, Ds5TriggerBlockSize>;

    using Ds5ProtocolFixture = UnitTest::LeakDetectionFixture;

    // ---------------------------------------------------------------------------------------
    // Packet layout: 47-byte static_assert compile-proof, plus a runtime check of the same
    // fact (belt and suspenders -- the static_assert alone would simply fail to compile this
    // TU if it ever regressed, which is the point, but an explicit runtime EXPECT_EQ also
    // shows up in test output).
    // ---------------------------------------------------------------------------------------

    static_assert(sizeof(Ds5EffectsPacket) == 47, "DS5 output-report payload must be exactly 47 bytes");

    TEST_F(Ds5ProtocolFixture, Packet_IsExactly47Bytes)
    {
        EXPECT_EQ(sizeof(Ds5EffectsPacket), 47u);
    }

    TEST_F(Ds5ProtocolFixture, Packet_DefaultConstructed_IsAllZero)
    {
        Ds5EffectsPacket packet;
        AZ::u8 raw[47] = {};
        std::memcpy(raw, &packet, 47);
        for (AZ::u8 byte : raw)
        {
            EXPECT_EQ(byte, 0);
        }
    }

    // ---------------------------------------------------------------------------------------
    // SetRightTriggerBlock / SetLeftTriggerBlock: flag-isolation. Mirrors pong's
    // make_state(LEFT|RIGHT|BOTH) placement + flag-byte assertions.
    // ---------------------------------------------------------------------------------------

    TEST_F(Ds5ProtocolFixture, SetRightTriggerBlock_SetsOnlyRightFlagBitAndRightBlock)
    {
        const Ds5TriggerBlock probe = { { 0xAA, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } };
        Ds5EffectsPacket packet;
        packet.SetRightTriggerBlock(probe);

        AZ::u8 raw[47] = {};
        std::memcpy(raw, &packet, 47);

        EXPECT_EQ(raw[0], 0x04); // bit2 only
        EXPECT_EQ(raw[1], 0); // valid-flags byte1 untouched
        EXPECT_EQ(raw[2], 0); // rumble right untouched
        EXPECT_EQ(raw[3], 0); // rumble left untouched

        AZ::u8 rightBlock[11];
        std::memcpy(rightBlock, raw + 10, 11);
        EXPECT_EQ(std::memcmp(rightBlock, probe.data(), 11), 0);

        AZ::u8 leftBlock[11] = {};
        EXPECT_EQ(std::memcmp(raw + 21, leftBlock, 11), 0); // left block still all zero

        AZ::u8 timestamp[4] = {};
        EXPECT_EQ(std::memcmp(raw + 32, timestamp, 4), 0); // timestamp untouched

        for (int i = 36; i < 47; ++i)
        {
            EXPECT_EQ(raw[i], 0) << "reserved byte " << i << " should be untouched";
        }
    }

    TEST_F(Ds5ProtocolFixture, SetLeftTriggerBlock_SetsOnlyLeftFlagBitAndLeftBlock)
    {
        const Ds5TriggerBlock probe = { { 0xAA, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } };
        Ds5EffectsPacket packet;
        packet.SetLeftTriggerBlock(probe);

        AZ::u8 raw[47] = {};
        std::memcpy(raw, &packet, 47);

        EXPECT_EQ(raw[0], 0x08); // bit3 only

        AZ::u8 leftBlock[11];
        std::memcpy(leftBlock, raw + 21, 11);
        EXPECT_EQ(std::memcmp(leftBlock, probe.data(), 11), 0);

        AZ::u8 rightBlock[11] = {};
        EXPECT_EQ(std::memcmp(raw + 10, rightBlock, 11), 0); // right block still all zero
    }

    TEST_F(Ds5ProtocolFixture, SetBothTriggerBlocks_SetsBothFlagBitsAndBothBlocksIndependently)
    {
        const Ds5TriggerBlock rightProbe = { { 0x11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } };
        const Ds5TriggerBlock leftProbe = { { 0x22, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 } };
        Ds5EffectsPacket packet;
        packet.SetRightTriggerBlock(rightProbe);
        packet.SetLeftTriggerBlock(leftProbe);

        AZ::u8 raw[47] = {};
        std::memcpy(raw, &packet, 47);

        EXPECT_EQ(raw[0], 0x0C); // bit2 | bit3
        EXPECT_EQ(std::memcmp(raw + 10, rightProbe.data(), 11), 0);
        EXPECT_EQ(std::memcmp(raw + 21, leftProbe.data(), 11), 0);
    }

    // ---------------------------------------------------------------------------------------
    // CompileTriggerEffectRaw: byte vectors ported from pong's test_ds5_effects.cpp.
    // The pong reference builders take pre-quantized integers directly; here we drive them
    // through the gem's normalized [0,1] TriggerEffect fields chosen so quantization lands
    // exactly on the same integers pong's tests used (see the divisions in each comment).
    // ---------------------------------------------------------------------------------------

    // Ported: build_off() -> {0x05,0,0,0,0,0,0,0,0,0,0}
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Off_MatchesPongVector)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Off;

        const Ds5TriggerBlock want = { { 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // Ported: build_feedback(position=2, strength=8) ->
    // {0x21, 0xFC, 0x03, 0xC0, 0xFF, 0xFF, 0x3F, 0,0,0,0}
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Feedback_Position2Strength8_MatchesPongVector)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Feedback;
        effect.m_startPosition = 2.0f / 9.0f; // -> zone 2
        effect.m_strength = 1.0f; // -> level 8 -> force 7

        const Ds5TriggerBlock want = { { 0x21, 0xFC, 0x03, 0xC0, 0xFF, 0xFF, 0x3F, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // Ported: build_feedback(position=9, strength=1) ->
    // {0x21, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0,0,0,0}
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Feedback_Position9Strength1_MatchesPongVector)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Feedback;
        effect.m_startPosition = 1.0f; // 9/9 -> zone 9
        effect.m_strength = 1.0f / 8.0f; // -> level 1 -> force 0

        const Ds5TriggerBlock want = { { 0x21, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // Ported: build_weapon(start=2, end=7, strength=8) -> {0x25, 0x84, 0x00, 0x07, 0,0,0,0,0,0,0}
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Weapon_2_7_8_MatchesPongVector)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Weapon;
        effect.m_startPosition = 2.0f / 9.0f;
        effect.m_endPosition = 7.0f / 9.0f;
        effect.m_strength = 1.0f; // -> 8 -> byte3 = 7

        const Ds5TriggerBlock want = { { 0x25, 0x84, 0x00, 0x07, 0, 0, 0, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // Ported: build_vibration(position=0, frequency=25, amplitude=8) ->
    // {0x26, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x3F, 0,0,25,0}
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Vibration_0_25_8_MatchesPongVector)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Vibration;
        effect.m_startPosition = 0.0f;
        effect.m_strength = 1.0f; // -> 8 -> force 7
        effect.m_frequency = 25.0f / 255.0f;

        const Ds5TriggerBlock want = { { 0x26, 0xFF, 0x03, 0xFF, 0xFF, 0xFF, 0x3F, 0, 0, 25, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // Ported: build_multi_feedback({0,0,4,0,0,0,0,0,0,8}) ->
    // {0x21, 0x04, 0x02, 0xC0, 0x00, 0x00, 0x38, 0,0,0,0}
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_MultiPositionFeedback_MatchesPongVector)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::MultiPositionFeedback;
        effect.m_positionalValues[2] = 4.0f / 8.0f; // level 4 -> force 3
        effect.m_positionalValues[9] = 1.0f; // level 8 -> force 7

        const Ds5TriggerBlock want = { { 0x21, 0x04, 0x02, 0xC0, 0x00, 0x00, 0x38, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // Ported: build_multi_vibration(frequency=50, {8,0,0,0,0,0,0,0,0,0}) ->
    // {0x26, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0,0,50,0}
    // This is also the FIRST bug-regression vector: the upstream Godot draft packed amplitude
    // with "(amplitude - 1) * 0x07" instead of "& 0x07". For amplitude=8, the buggy formula
    // gives (8-1)*0x07 = 0x31 (49), which does NOT fit in a 3-bit field and corrupts
    // neighboring zones' bits when shifted into place; the correct "& 0x07" gives 0x07. This
    // vector's expected byte3 == 0x07 (not 0x31) is exactly the discriminator.
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_MultiPositionVibration_MatchesPongVector_AndIsNotUpstreamMultiplyBug)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::MultiPositionVibration;
        effect.m_positionalValues[0] = 1.0f; // level 8
        effect.m_frequency = 50.0f / 255.0f;

        const Ds5TriggerBlock want = { { 0x26, 0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0, 0, 50, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // Additional (not from pong) regression coverage for the same bug: two ADJACENT zones
    // both at max amplitude. The buggy multiply formula would bleed bits from zone 0's field
    // into zone 1's, and vice versa; the correct "& 0x07" keeps each zone's 3 bits isolated.
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_MultiPositionVibration_AdjacentMaxZones_NoBitBleed)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::MultiPositionVibration;
        effect.m_positionalValues[0] = 1.0f; // level 8 -> force 7
        effect.m_positionalValues[1] = 1.0f; // level 8 -> force 7

        const Ds5TriggerBlock raw = CompileTriggerEffectRaw(effect);
        EXPECT_EQ(raw[0], 0x26);
        EXPECT_EQ(raw[1], 0x03); // zones 0 and 1 active
        EXPECT_EQ(raw[2], 0x00);
        // force_zones = (7 << 0) | (7 << 3) = 0x3F, isolated in the low byte.
        EXPECT_EQ(raw[3], 0x3F);
        EXPECT_EQ(raw[4], 0x00);
        EXPECT_EQ(raw[5], 0x00);
        EXPECT_EQ(raw[6], 0x00);
    }

    // Ported: build_slope_feedback(start=0, end=9, startStrength=1, endStrength=8) ->
    // {0x21, 0xFF, 0x03, 0x88, 0x34, 0xB6, 0x3E, 0,0,0,0}
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_SlopeFeedback_0_9_1_8_MatchesPongVector)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::SlopeFeedback;
        effect.m_startPosition = 0.0f;
        effect.m_endPosition = 1.0f; // 9/9
        effect.m_strength = 1.0f / 8.0f; // start strength 1
        effect.m_endStrength = 1.0f; // end strength 8

        const Ds5TriggerBlock want = { { 0x21, 0xFF, 0x03, 0x88, 0x34, 0xB6, 0x3E, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // SECOND bug-regression: the upstream Godot draft validated "start_position > 8" where it
    // meant "start_strength > 8". Two symptoms, tested independently:
    //
    // (a) Positions legitimately range 0-9 (ten zones) -- an out-of-unit-range endPosition
    //     must clamp into that 0-9 zone domain, and must NOT be miscapped at 8 (which is what
    //     a strength-shaped validation applied to position would do, silently dropping zone 9
    //     from every slope effect that reaches the end of trigger travel).
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_SlopeFeedback_OutOfRangeEndPosition_ClampsToZone9NotZone8)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::SlopeFeedback;
        effect.m_startPosition = 0.0f;
        effect.m_endPosition = 5.0f; // wildly out of [0,1]; must clamp to zone 9, not zone 8
        effect.m_strength = 1.0f / 8.0f;
        effect.m_endStrength = 1.0f;

        // Same expected bytes as the exact-range vector above: clamping endPosition down to
        // zone 9 must reproduce the identical ramp/pack result.
        const Ds5TriggerBlock want = { { 0x21, 0xFF, 0x03, 0x88, 0x34, 0xB6, 0x3E, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    // (b) Strengths legitimately range 0-8 -- an out-of-unit-range m_strength/m_endStrength
    //     MUST clamp into that domain (never left unbounded the way the buggy validation left
    //     start_strength unchecked), so the packed 3-bit force never sees an unmasked/out of
    //     range level.
    //
    // Zone 9 (the last zone) is used as both start and end here so there are no zones
    // "beyond end_position" to also light up at end_strength (per pong's build_slope_feedback,
    // every zone after end_position holds at end_strength, which is exercised by the ported
    // 0,9,1,8 vector above and would otherwise make this test's mask harder to read).
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_SlopeFeedback_OutOfRangeStrengths_ClampToLevel8NotLeftUnbounded)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::SlopeFeedback;
        effect.m_startPosition = 1.0f; // zone 9
        effect.m_endPosition = 1.0f; // zone 9 (single-zone ramp)
        effect.m_strength = 5.0f; // wildly out of [0,1]; must clamp to level 8 -> force 7
        effect.m_endStrength = 5.0f;

        const Ds5TriggerBlock raw = CompileTriggerEffectRaw(effect);
        EXPECT_EQ(raw[0], 0x21);
        EXPECT_EQ(raw[1], 0x00); // zone 9 only: mask = 0x0200
        EXPECT_EQ(raw[2], 0x02);
        EXPECT_EQ(raw[3], 0x00);
        EXPECT_EQ(raw[4], 0x00);
        EXPECT_EQ(raw[5], 0x00);
        EXPECT_EQ(raw[6], 0x38); // force 7 (level 8 clamped, not some unmasked/huge value) at bit 27: 7 << 27 = 0x38000000
    }

    // ---------------------------------------------------------------------------------------
    // Quantization edge cases: 0.0, 1.0, and a mid value, for each normalized domain.
    // ---------------------------------------------------------------------------------------

    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Feedback_ZeroStrength_IsOffShortcut_NoZonesActive)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Feedback;
        effect.m_startPosition = 0.0f;
        effect.m_strength = 0.0f; // level 0 == guide's "off shortcut": no active zones

        const Ds5TriggerBlock want = { { 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0, 0, 0, 0 } };
        EXPECT_EQ(CompileTriggerEffectRaw(effect), want);
    }

    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Feedback_MidPositionAndStrength_QuantizesToNearestZoneAndLevel)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Feedback;
        effect.m_startPosition = 0.5f; // round(0.5*9) = round(4.5) = 5
        effect.m_strength = 0.5f; // round(0.5*8) = 4 -> force 3

        const Ds5TriggerBlock raw = CompileTriggerEffectRaw(effect);
        EXPECT_EQ(raw[0], 0x21);
        // Zones 5..9 active: mask = 0b1111100000 = 0x03E0.
        EXPECT_EQ(raw[1], 0xE0);
        EXPECT_EQ(raw[2], 0x03);
    }

    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Vibration_FrequencyEdgeCases_ZeroAndOneQuantizeToByteExtremes)
    {
        TriggerEffect zeroFreq;
        zeroFreq.m_mode = TriggerEffectMode::Vibration;
        zeroFreq.m_startPosition = 0.0f;
        zeroFreq.m_strength = 1.0f;
        zeroFreq.m_frequency = 0.0f;
        EXPECT_EQ(CompileTriggerEffectRaw(zeroFreq)[9], 0);

        TriggerEffect oneFreq = zeroFreq;
        oneFreq.m_frequency = 1.0f;
        EXPECT_EQ(CompileTriggerEffectRaw(oneFreq)[9], 255);
    }

    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_Weapon_StrengthClampedToMinimumOne_NeverUnderflowsByte)
    {
        // Weapon has no "0 = off" shortcut (a weapon break of zero force is meaningless);
        // strength 0.0 must clamp to level 1 (byte3 = 0), never wrap to 0xFF via an
        // unclamped `0 - 1` on an unsigned byte.
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::Weapon;
        effect.m_startPosition = 2.0f / 9.0f;
        effect.m_endPosition = 7.0f / 9.0f;
        effect.m_strength = 0.0f;

        EXPECT_EQ(CompileTriggerEffectRaw(effect)[3], 0);
    }

    // ---------------------------------------------------------------------------------------
    // Round-trip sanity vs DegradeToBaselineApi (DualSenseTriggerEffectMapping.h): every one
    // of the 7 TriggerEffectMode values must compile to a well-formed 11-byte block (correct
    // mode byte at minimum) both directly and after baseline-API degradation, so the two
    // subsystems never disagree about what a given mode compiles to.
    // ---------------------------------------------------------------------------------------

    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_AllSevenModes_ProduceExpectedModeByte)
    {
        TriggerEffect effect;
        effect.m_startPosition = 2.0f / 9.0f;
        effect.m_endPosition = 7.0f / 9.0f;
        effect.m_strength = 0.5f;
        effect.m_endStrength = 1.0f;
        effect.m_frequency = 0.5f;
        effect.m_positionalValues.fill(0.5f);

        effect.m_mode = TriggerEffectMode::Off;
        EXPECT_EQ(CompileTriggerEffectRaw(effect)[0], 0x05);

        effect.m_mode = TriggerEffectMode::Feedback;
        EXPECT_EQ(CompileTriggerEffectRaw(effect)[0], 0x21);

        effect.m_mode = TriggerEffectMode::Weapon;
        EXPECT_EQ(CompileTriggerEffectRaw(effect)[0], 0x25);

        effect.m_mode = TriggerEffectMode::Vibration;
        EXPECT_EQ(CompileTriggerEffectRaw(effect)[0], 0x26);

        effect.m_mode = TriggerEffectMode::MultiPositionFeedback;
        EXPECT_EQ(CompileTriggerEffectRaw(effect)[0], 0x21);

        effect.m_mode = TriggerEffectMode::MultiPositionVibration;
        EXPECT_EQ(CompileTriggerEffectRaw(effect)[0], 0x26);

        effect.m_mode = TriggerEffectMode::SlopeFeedback;
        EXPECT_EQ(CompileTriggerEffectRaw(effect)[0], 0x21);
    }

    // The three 12.3+-only modes degrade (via DegradeToBaselineApi) to a baseline mode before
    // ever reaching CompileTriggerEffectRaw on hardware that needs it; verify the degraded
    // effect still compiles to the expected baseline mode byte (0x21 Feedback / 0x26
    // Vibration / 0x21 Feedback respectively) rather than silently producing 0x00 or crashing.
    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_DegradedMultiPositionFeedback_CompilesToFeedbackByte)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::MultiPositionFeedback;
        effect.m_positionalValues[3] = 0.75f;

        const TriggerEffect degraded = DegradeToBaselineApi(effect);
        EXPECT_EQ(degraded.m_mode, TriggerEffectMode::Feedback);
        EXPECT_EQ(CompileTriggerEffectRaw(degraded)[0], 0x21);
    }

    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_DegradedMultiPositionVibration_CompilesToVibrationByte)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::MultiPositionVibration;
        effect.m_positionalValues[3] = 0.75f;
        effect.m_frequency = 0.4f;

        const TriggerEffect degraded = DegradeToBaselineApi(effect);
        EXPECT_EQ(degraded.m_mode, TriggerEffectMode::Vibration);
        EXPECT_EQ(CompileTriggerEffectRaw(degraded)[0], 0x26);
    }

    TEST_F(Ds5ProtocolFixture, CompileTriggerEffectRaw_DegradedSlopeFeedback_CompilesToFeedbackByte)
    {
        TriggerEffect effect;
        effect.m_mode = TriggerEffectMode::SlopeFeedback;
        effect.m_startPosition = 0.0f;
        effect.m_endPosition = 1.0f;
        effect.m_strength = 0.25f;
        effect.m_endStrength = 1.0f;

        const TriggerEffect degraded = DegradeToBaselineApi(effect);
        EXPECT_EQ(degraded.m_mode, TriggerEffectMode::Feedback);
        EXPECT_EQ(CompileTriggerEffectRaw(degraded)[0], 0x21);
    }

} // namespace DualSenseTests
