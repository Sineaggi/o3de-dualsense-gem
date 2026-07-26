#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>
#include <Clients/DualSenseDebugGamepadImpl.h>
#include <Clients/DualSenseGamepadButtonMap.h>

namespace DualSenseTests
{
    // Pre-initialize the static button map at module load time to avoid
    // leak detection during the first test
    namespace
    {
        const auto& PreInitButtonMap = [] { return DualSense::GetDualSenseDigitalButtonMap(); }();
    } // namespace

    using DualSenseDebugFixture = UnitTest::LeakDetectionFixture;

    TEST_F(DualSenseDebugFixture, ButtonMap_Has14UniqueSingleBitEntries)
    {
        const auto& map = DualSense::GetDualSenseDigitalButtonMap();
        EXPECT_EQ(map.size(), 14);
        AZ::u32 combined = 0;
        for (const auto& [bit, channelId] : map)
        {
            EXPECT_NE(channelId, nullptr);
            EXPECT_EQ(bit & (bit - 1), 0u) << "mask must be a single bit";
            EXPECT_EQ(combined & bit, 0u) << "bit used twice";
            combined |= bit;
        }
    }

    TEST_F(DualSenseDebugFixture, DebugFactory_CreatesImplementation_DeviceSupportedAndConnected)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);
        EXPECT_TRUE(gamepad.IsSupported());
        EXPECT_TRUE(gamepad.IsConnected());
        EXPECT_NE(factory.m_lastCreated, nullptr);
    }

    TEST_F(DualSenseDebugFixture, HapticBusSetVibration_ReachesDebugImplementation)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);
        AzFramework::InputHapticFeedbackRequestBus::Event(
            gamepad.GetInputDeviceId(),
            &AzFramework::InputHapticFeedbackRequests::SetVibration, 0.5f, 0.25f);
        ASSERT_NE(factory.m_lastCreated, nullptr);
        EXPECT_FLOAT_EQ(factory.m_lastCreated->m_lastVibrationLeft, 0.5f);
        EXPECT_FLOAT_EQ(factory.m_lastCreated->m_lastVibrationRight, 0.25f);
    }

    TEST_F(DualSenseDebugFixture, TickInputDevice_DoesNotCrashWithNoActivity)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);
        gamepad.TickInputDevice();
        gamepad.TickInputDevice();
    }
} // namespace DualSenseTests
