#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>
#include <Clients/DualSenseDebugGamepadImpl.h>

namespace DualSenseTests
{
    using SwapBus = AzFramework::InputDeviceImplementationRequest<AzFramework::InputDeviceGamepad>;
    using DualSenseSwapFixture = UnitTest::LeakDetectionFixture;

    TEST_F(DualSenseSwapFixture, SetCustomImplementation_OnDeviceWithNoImpl_InstallsOurs)
    {
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, nullptr);
        EXPECT_FALSE(gamepad.IsSupported());

        DualSense::DualSenseDebugGamepadImplFactory factory;
        SwapBus::Bus::Event(gamepad.GetInputDeviceId(), &SwapBus::SetCustomImplementation, &factory);

        EXPECT_TRUE(gamepad.IsSupported());
        EXPECT_EQ(factory.m_lastCreated != nullptr, true);
    }

    TEST_F(DualSenseSwapFixture, SetCustomImplementation_SecondFactory_ReplacesFirst)
    {
        DualSense::DualSenseDebugGamepadImplFactory factoryA;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factoryA);

        // Record the destruction count before swap
        const AZ::s32 destroyedBefore = DualSense::DualSenseDebugGamepadImpl::s_destructionCount;

        DualSense::DualSenseDebugGamepadImplFactory factoryB;
        SwapBus::Bus::Event(gamepad.GetInputDeviceId(), &SwapBus::SetCustomImplementation, &factoryB);
        ASSERT_NE(factoryB.m_lastCreated, nullptr);

        // Verify that A's implementation was destroyed by the swap
        EXPECT_EQ(DualSense::DualSenseDebugGamepadImpl::s_destructionCount, destroyedBefore + 1);

        // Vibration must now land on B's implementation.
        AzFramework::InputHapticFeedbackRequestBus::Event(
            gamepad.GetInputDeviceId(),
            &AzFramework::InputHapticFeedbackRequests::SetVibration, 1.0f, 1.0f);
        EXPECT_FLOAT_EQ(factoryB.m_lastCreated->m_lastVibrationLeft, 1.0f);
    }

    TEST_F(DualSenseSwapFixture, SetCustomImplementation_NullFactory_IsIgnoredByEngine)
    {
        // Pins the engine quirk the spec documents: null does NOT clear the impl.
        // If this test ever fails after an engine upgrade, the restore path in
        // DualSenseSystemComponent must be re-reviewed.
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);

        SwapBus::Bus::Event(gamepad.GetInputDeviceId(), &SwapBus::SetCustomImplementation, nullptr);
        EXPECT_TRUE(gamepad.IsSupported());
    }

    TEST_F(DualSenseSwapFixture, SwapAddressedToIndex1_DoesNotTouchIndex0)
    {
        DualSense::DualSenseDebugGamepadImplFactory factory0;
        AzFramework::InputDeviceGamepad gamepad0(AzFramework::InputDeviceGamepad::IdForIndex0, &factory0);
        AzFramework::InputDeviceGamepad gamepad1(AzFramework::InputDeviceGamepad::IdForIndexN(1), nullptr);

        DualSense::DualSenseDebugGamepadImplFactory factory1;
        SwapBus::Bus::Event(gamepad1.GetInputDeviceId(), &SwapBus::SetCustomImplementation, &factory1);

        EXPECT_TRUE(gamepad1.IsSupported());
        EXPECT_EQ(factory0.m_lastCreated->m_lastVibrationLeft, -1.0f);
        EXPECT_NE(factory1.m_lastCreated, nullptr);
    }
} // namespace DualSenseTests
