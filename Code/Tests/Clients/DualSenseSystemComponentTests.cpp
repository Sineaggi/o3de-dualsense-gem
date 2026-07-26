#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <Clients/DualSenseSystemComponent.h>
#include <Clients/DualSenseDebugGamepadImpl.h>

namespace DualSenseTests
{
    using DualSenseComponentFixture = UnitTest::LeakDetectionFixture;

    TEST_F(DualSenseComponentFixture, SwapSlotToFactory_InstallsOnMatchingSlotOnly)
    {
        AzFramework::InputDeviceGamepad gamepad0(AzFramework::InputDeviceGamepad::IdForIndex0, nullptr);
        AzFramework::InputDeviceGamepad gamepad1(AzFramework::InputDeviceGamepad::IdForIndexN(1), nullptr);

        DualSense::DualSenseDebugGamepadImplFactory factory;
        DualSense::DualSenseSystemComponent::SwapSlotToFactory(1, &factory);

        EXPECT_FALSE(gamepad0.IsSupported());
        EXPECT_TRUE(gamepad1.IsSupported());
    }

    TEST_F(DualSenseComponentFixture, RestoreSlotToPlatformDefault_WithNoPlatformFactory_LeavesImplInPlace)
    {
        // In unit tests no NativeUISystemComponent has registered a platform factory,
        // so AZ::Interface<ImplementationFactory>::Get() is null and restore must be
        // a safe no-op (engine ignores null factories - pinned in DualSenseSwapTests).
        DualSense::DualSenseDebugGamepadImplFactory factory;
        AzFramework::InputDeviceGamepad gamepad0(AzFramework::InputDeviceGamepad::IdForIndex0, &factory);

        DualSense::DualSenseSystemComponent::RestoreSlotToPlatformDefault(0);
        EXPECT_TRUE(gamepad0.IsSupported());
    }
} // namespace DualSenseTests
