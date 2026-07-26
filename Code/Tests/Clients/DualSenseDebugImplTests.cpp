#include <AzCore/UnitTest/TestTypes.h>
#include <AzTest/AzTest.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>
#include <Clients/DualSenseDebugGamepadImpl.h>
#include <Clients/DualSenseGamepadButtonMap.h>

namespace DualSenseTests
{
    //! Global test environment to warm up the button map after allocators are initialized.
    //! This ensures the one-time allocation of the static map happens after AZ::AllocatorInstance
    //! is guaranteed to exist, and outside the scope of any LeakDetectionFixture.
    class DualSenseTestEnvironment : public ::testing::Environment
    {
    public:
        void SetUp() override
        {
            // Access the button map once to initialize the static unordered_map.
            // This happens during gtest environment setup, after AZ's unit test hook
            // has created allocators, and before any fixture starts tracking allocations.
            DualSense::GetDualSenseDigitalButtonMap();
        }
    };

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

    // Register the global environment
    [[maybe_unused]] static int DualSenseTestEnvironmentRegistration = []()
    {
        ::testing::AddGlobalTestEnvironment(new DualSenseTestEnvironment());
        return 0;
    }();
} // namespace DualSenseTests
