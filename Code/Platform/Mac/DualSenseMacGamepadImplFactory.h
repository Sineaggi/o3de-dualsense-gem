#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Creates the Mac (GameController.framework) gamepad implementation.
    //! m_pendingController must be set to the target GCController* immediately
    //! before the swap bus event fires (synchronous dispatch), and cleared after.
    struct DualSenseMacGamepadImplFactory
        : public AzFramework::InputDeviceGamepad::ImplementationFactory
    {
        AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> Create(
            AzFramework::InputDeviceGamepad& inputDevice) override;
        AZ::u32 GetMaxSupportedGamepads() const override { return 4; }

        void* m_pendingController = nullptr; // GCController*
    };
} // namespace DualSense
