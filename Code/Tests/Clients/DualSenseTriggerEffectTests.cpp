#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <DualSense/DualSenseTriggerEffects.h>

namespace DualSenseTests
{
    using TriggerEffectFixture = UnitTest::LeakDetectionFixture;

    TEST_F(TriggerEffectFixture, Defaults_AreOffAndNeutral)
    {
        DualSense::TriggerEffect effect;

        EXPECT_EQ(effect.m_mode, DualSense::TriggerEffectMode::Off);
        EXPECT_EQ(effect.m_startPosition, 0.0f);
        EXPECT_EQ(effect.m_endPosition, 1.0f);
        EXPECT_EQ(effect.m_strength, 0.0f);
        EXPECT_EQ(effect.m_endStrength, 0.0f);
        EXPECT_EQ(effect.m_frequency, 0.0f);

        for (size_t i = 0; i < effect.m_positionalValues.size(); ++i)
        {
            EXPECT_EQ(effect.m_positionalValues[i], 0.0f);
        }
    }

    TEST_F(TriggerEffectFixture, Clamped_ClampsAllFieldsToUnitRange)
    {
        DualSense::TriggerEffect effect;
        effect.m_startPosition = -2.0f;
        effect.m_endPosition = 3.0f;
        effect.m_strength = -2.0f;
        effect.m_endStrength = 3.0f;
        effect.m_frequency = -2.0f;
        effect.m_positionalValues[0] = -2.0f;
        effect.m_positionalValues[1] = 3.0f;

        DualSense::TriggerEffect clamped = effect.Clamped();

        EXPECT_EQ(clamped.m_startPosition, 0.0f);
        EXPECT_EQ(clamped.m_endPosition, 1.0f);
        EXPECT_EQ(clamped.m_strength, 0.0f);
        EXPECT_EQ(clamped.m_endStrength, 1.0f);
        EXPECT_EQ(clamped.m_frequency, 0.0f);
        EXPECT_EQ(clamped.m_positionalValues[0], 0.0f);
        EXPECT_EQ(clamped.m_positionalValues[1], 1.0f);
    }

    TEST_F(TriggerEffectFixture, Clamped_EnforcesEndPositionNotBeforeStart)
    {
        DualSense::TriggerEffect effect1;
        effect1.m_startPosition = 0.8f;
        effect1.m_endPosition = 0.2f;

        DualSense::TriggerEffect clamped1 = effect1.Clamped();
        EXPECT_EQ(clamped1.m_startPosition, 0.8f);
        EXPECT_EQ(clamped1.m_endPosition, 0.8f);

        DualSense::TriggerEffect effect2;
        effect2.m_startPosition = 0.3f;
        effect2.m_endPosition = 0.9f;

        DualSense::TriggerEffect clamped2 = effect2.Clamped();
        EXPECT_EQ(clamped2.m_startPosition, 0.3f);
        EXPECT_EQ(clamped2.m_endPosition, 0.9f);
    }

    TEST_F(TriggerEffectFixture, SerializeReflect_RegistersType)
    {
        AZ::SerializeContext sc;
        DualSense::TriggerEffect::Reflect(&sc);

        EXPECT_NE(sc.FindClassData(azrtti_typeid<DualSense::TriggerEffect>()), nullptr);
    }

} // namespace DualSenseTests
