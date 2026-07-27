// Pure axis-normalization coverage for the SDL backend (Phase 3a Task 3). Guarded the same way
// the file under test is (DualSenseSdlBackendProbeTests.cpp established this pattern in Task 1):
// only compiled/run when DUALSENSE_SDL_BACKEND_ENABLED is defined, i.e. only where
// PAL_TRAIT_DUALSENSE_SDL_BACKEND is TRUE (Mac, where PAL_TRAIT_DUALSENSE_TEST_SUPPORTED is also
// TRUE, is where this actually runs today).
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <AzCore/UnitTest/TestTypes.h>
#include <Clients/Sdl/DualSenseSdlAxisMath.h>

namespace DualSenseTests
{
    using SdlAxisFixture = UnitTest::LeakDetectionFixture;

    // --- NormalizeStickAxis: raw range [-32768, 32767], per SDL_GetGamepadAxis's doc comment ---

    TEST_F(SdlAxisFixture, NormalizeStickAxis_MinRaw_ReturnsExactlyMinusOne)
    {
        EXPECT_FLOAT_EQ(DualSense::SdlAxisMath::NormalizeStickAxis(-32768), -1.0f);
    }

    TEST_F(SdlAxisFixture, NormalizeStickAxis_MaxRaw_ReturnsExactlyOne)
    {
        EXPECT_FLOAT_EQ(DualSense::SdlAxisMath::NormalizeStickAxis(32767), 1.0f);
    }

    TEST_F(SdlAxisFixture, NormalizeStickAxis_Zero_ReturnsZero)
    {
        EXPECT_FLOAT_EQ(DualSense::SdlAxisMath::NormalizeStickAxis(0), 0.0f);
    }

    TEST_F(SdlAxisFixture, NormalizeStickAxis_HalfPositive_ReturnsApproxHalf)
    {
        EXPECT_NEAR(DualSense::SdlAxisMath::NormalizeStickAxis(16384), 0.5f, 0.0001f);
    }

    TEST_F(SdlAxisFixture, NormalizeStickAxis_HalfNegative_ReturnsApproxNegativeHalf)
    {
        EXPECT_NEAR(DualSense::SdlAxisMath::NormalizeStickAxis(-16384), -0.5f, 0.0001f);
    }

    // --- NormalizeTriggerAxis: raw range [0, 32767], never negative per SDL's doc comment ---

    TEST_F(SdlAxisFixture, NormalizeTriggerAxis_Zero_ReturnsZero)
    {
        EXPECT_FLOAT_EQ(DualSense::SdlAxisMath::NormalizeTriggerAxis(0), 0.0f);
    }

    TEST_F(SdlAxisFixture, NormalizeTriggerAxis_MaxRaw_ReturnsExactlyOne)
    {
        EXPECT_FLOAT_EQ(DualSense::SdlAxisMath::NormalizeTriggerAxis(32767), 1.0f);
    }

    TEST_F(SdlAxisFixture, NormalizeTriggerAxis_HalfPressed_ReturnsApproxHalf)
    {
        EXPECT_NEAR(DualSense::SdlAxisMath::NormalizeTriggerAxis(16384), 0.5f, 0.0001f);
    }

    TEST_F(SdlAxisFixture, NormalizeTriggerAxis_DefensiveClampBelowZero_ClampsToZero)
    {
        // SDL guarantees triggers never report negative, but the function clamps defensively
        // anyway (see its doc comment) -- verify that clamp actually holds for an out-of-contract
        // negative input rather than only trusting the documented range.
        EXPECT_FLOAT_EQ(DualSense::SdlAxisMath::NormalizeTriggerAxis(-100), 0.0f);
    }
} // namespace DualSenseTests

#endif // DUALSENSE_SDL_BACKEND_ENABLED
