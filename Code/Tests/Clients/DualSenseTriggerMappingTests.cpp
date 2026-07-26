#include <AzCore/UnitTest/TestTypes.h>
#include <DualSense/DualSenseTriggerEffects.h>
#include <DualSense/DualSenseTriggerEffectMapping.h>

namespace DualSenseTests
{
    using TriggerMappingFixture = UnitTest::LeakDetectionFixture;

    // RequiresExtendedTriggerApi tests - truth table for all 7 modes
    TEST_F(TriggerMappingFixture, RequiresExtendedTriggerApi_OffReturnsFalse)
    {
        EXPECT_FALSE(DualSense::RequiresExtendedTriggerApi(DualSense::TriggerEffectMode::Off));
    }

    TEST_F(TriggerMappingFixture, RequiresExtendedTriggerApi_FeedbackReturnsFalse)
    {
        EXPECT_FALSE(DualSense::RequiresExtendedTriggerApi(DualSense::TriggerEffectMode::Feedback));
    }

    TEST_F(TriggerMappingFixture, RequiresExtendedTriggerApi_WeaponReturnsFalse)
    {
        EXPECT_FALSE(DualSense::RequiresExtendedTriggerApi(DualSense::TriggerEffectMode::Weapon));
    }

    TEST_F(TriggerMappingFixture, RequiresExtendedTriggerApi_VibrationReturnsFalse)
    {
        EXPECT_FALSE(DualSense::RequiresExtendedTriggerApi(DualSense::TriggerEffectMode::Vibration));
    }

    TEST_F(TriggerMappingFixture, RequiresExtendedTriggerApi_MultiPositionFeedbackReturnsTrue)
    {
        EXPECT_TRUE(DualSense::RequiresExtendedTriggerApi(DualSense::TriggerEffectMode::MultiPositionFeedback));
    }

    TEST_F(TriggerMappingFixture, RequiresExtendedTriggerApi_MultiPositionVibrationReturnsTrue)
    {
        EXPECT_TRUE(DualSense::RequiresExtendedTriggerApi(DualSense::TriggerEffectMode::MultiPositionVibration));
    }

    TEST_F(TriggerMappingFixture, RequiresExtendedTriggerApi_SlopeFeedbackReturnsTrue)
    {
        EXPECT_TRUE(DualSense::RequiresExtendedTriggerApi(DualSense::TriggerEffectMode::SlopeFeedback));
    }

    // Degradation rule: Off passes through with clamping
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_OffPassesThrough)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::Off;
        effect.m_startPosition = 0.3f;
        effect.m_strength = 0.5f;

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Off);
        EXPECT_EQ(degraded.m_startPosition, 0.3f);
        EXPECT_EQ(degraded.m_strength, 0.5f);
    }

    // Degradation rule: Feedback passes through with clamping
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_FeedbackPassesThrough)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::Feedback;
        effect.m_startPosition = 0.4f;
        effect.m_strength = 0.7f;

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Feedback);
        EXPECT_EQ(degraded.m_startPosition, 0.4f);
        EXPECT_EQ(degraded.m_strength, 0.7f);
    }

    // Degradation rule: Weapon passes through with clamping (including out-of-range fields)
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_WeaponPassesThroughWithClamping)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::Weapon;
        effect.m_startPosition = -0.5f;  // out of range
        effect.m_endPosition = 1.5f;      // out of range
        effect.m_strength = 0.6f;

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Weapon);
        EXPECT_EQ(degraded.m_startPosition, 0.0f);   // clamped from -0.5
        EXPECT_EQ(degraded.m_endPosition, 1.0f);     // clamped from 1.5
        EXPECT_EQ(degraded.m_strength, 0.6f);
    }

    // Degradation rule: Vibration passes through with clamping
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_VibrationPassesThrough)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::Vibration;
        effect.m_strength = 0.5f;    // amplitude
        effect.m_frequency = 0.8f;

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Vibration);
        EXPECT_EQ(degraded.m_strength, 0.5f);
        EXPECT_EQ(degraded.m_frequency, 0.8f);
    }

    // Degradation rule: MultiPositionFeedback -> Feedback with start position and max strength
    // Case: {0, 0, 0.5, 0.9, 0, ...} -> start = 2/9.0f, strength = 0.9
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_MultiPositionFeedbackWithValues)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::MultiPositionFeedback;
        effect.m_positionalValues[0] = 0.0f;
        effect.m_positionalValues[1] = 0.0f;
        effect.m_positionalValues[2] = 0.5f;
        effect.m_positionalValues[3] = 0.9f;
        effect.m_positionalValues[4] = 0.0f;
        // rest default to 0

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Feedback);
        EXPECT_FLOAT_EQ(degraded.m_startPosition, 2.0f / 9.0f);
        EXPECT_FLOAT_EQ(degraded.m_strength, 0.9f);
    }

    // Degradation rule: MultiPositionFeedback all zeros -> start = 1.0, strength = 0
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_MultiPositionFeedbackAllZeros)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::MultiPositionFeedback;
        // all positional values default to 0

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Feedback);
        EXPECT_FLOAT_EQ(degraded.m_startPosition, 1.0f);
        EXPECT_FLOAT_EQ(degraded.m_strength, 0.0f);
    }

    // Degradation rule: MultiPositionVibration -> Vibration with start position, max amplitude, preserved frequency
    // Case: {0.4, 0, ...}, freq 0.6 -> start = 0, amplitude = 0.4, freq = 0.6
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_MultiPositionVibrationWithValues)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::MultiPositionVibration;
        effect.m_positionalValues[0] = 0.4f;
        effect.m_positionalValues[1] = 0.0f;
        // rest default to 0
        effect.m_frequency = 0.6f;

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Vibration);
        EXPECT_FLOAT_EQ(degraded.m_startPosition, 0.0f);  // first index with value > 0
        EXPECT_FLOAT_EQ(degraded.m_strength, 0.4f);       // max amplitude
        EXPECT_FLOAT_EQ(degraded.m_frequency, 0.6f);
    }

    // Degradation rule: SlopeFeedback -> Feedback with preserved start, averaged strength
    // Case: start .2, end .8, strengths .4/.8 -> Feedback start .2 strength .6
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_SlopeFeedback)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::SlopeFeedback;
        effect.m_startPosition = 0.2f;
        effect.m_endPosition = 0.8f;
        effect.m_strength = 0.4f;         // startStrength
        effect.m_endStrength = 0.8f;

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Feedback);
        EXPECT_FLOAT_EQ(degraded.m_startPosition, 0.2f);
        EXPECT_FLOAT_EQ(degraded.m_strength, 0.6f);  // (0.4 + 0.8) / 2
    }

    // Test that degradation output is always clamped
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_OutputIsClamped)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::Feedback;
        effect.m_startPosition = -0.5f;  // out of range
        effect.m_strength = 1.5f;         // out of range

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_startPosition, 0.0f);  // clamped
        EXPECT_EQ(degraded.m_strength, 1.0f);       // clamped
    }

    // Test that MultiPositionVibration with all zeros has start = 1.0
    TEST_F(TriggerMappingFixture, DegradeToBaselineApi_MultiPositionVibrationAllZeros)
    {
        DualSense::TriggerEffect effect;
        effect.m_mode = DualSense::TriggerEffectMode::MultiPositionVibration;
        effect.m_frequency = 0.5f;
        // all positional values default to 0

        DualSense::TriggerEffect degraded = DualSense::DegradeToBaselineApi(effect);

        EXPECT_EQ(degraded.m_mode, DualSense::TriggerEffectMode::Vibration);
        EXPECT_FLOAT_EQ(degraded.m_startPosition, 1.0f);  // no values > 0
        EXPECT_FLOAT_EQ(degraded.m_strength, 0.0f);
        EXPECT_FLOAT_EQ(degraded.m_frequency, 0.5f);
    }

} // namespace DualSenseTests
