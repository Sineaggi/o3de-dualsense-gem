#include <AzCore/UnitTest/TestTypes.h>
#include <Clients/DualSenseBackendSelection.h>

// ParseBackendSelection is a pure string -> enum mapping (no AZ::IConsole dependency), extracted
// specifically so it is testable without standing up a console context -- see
// DualSenseBackendSelection.h's doc comment. GetDualSenseBackendSelection() (the impure wrapper
// that actually reads the live dualsense_backend cvar) is exercised indirectly by
// DualSenseSwapTests.cpp / hardware smoke, not here.
namespace DualSenseTests
{
    using BackendSelectionFixture = UnitTest::LeakDetectionFixture;

    TEST_F(BackendSelectionFixture, ParseBackendSelection_Sdl_ReturnsSdl)
    {
        EXPECT_EQ(DualSense::ParseBackendSelection("sdl"), DualSense::BackendSelection::Sdl);
    }

    TEST_F(BackendSelectionFixture, ParseBackendSelection_SdlUpperCase_ReturnsSdl)
    {
        EXPECT_EQ(DualSense::ParseBackendSelection("SDL"), DualSense::BackendSelection::Sdl);
    }

    TEST_F(BackendSelectionFixture, ParseBackendSelection_SdlMixedCase_ReturnsSdl)
    {
        EXPECT_EQ(DualSense::ParseBackendSelection("SdL"), DualSense::BackendSelection::Sdl);
    }

    TEST_F(BackendSelectionFixture, ParseBackendSelection_Native_ReturnsNative)
    {
        EXPECT_EQ(DualSense::ParseBackendSelection("native"), DualSense::BackendSelection::Native);
    }

    TEST_F(BackendSelectionFixture, ParseBackendSelection_Empty_ReturnsNative)
    {
        EXPECT_EQ(DualSense::ParseBackendSelection(""), DualSense::BackendSelection::Native);
    }

    TEST_F(BackendSelectionFixture, ParseBackendSelection_UnrecognizedValue_ReturnsNative)
    {
        EXPECT_EQ(DualSense::ParseBackendSelection("bogus"), DualSense::BackendSelection::Native);
    }
} // namespace DualSenseTests
