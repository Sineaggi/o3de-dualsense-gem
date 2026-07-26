#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace DualSense
{
    class DualSenseHapticsMac;

    //! Standard-gamepad backend for a DualSense driven by GameController.framework.
    class InputDeviceGamepadDualSenseMac
        : public AzFramework::InputDeviceGamepad::Implementation
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(InputDeviceGamepadDualSenseMac, AZ::SystemAllocator);

        InputDeviceGamepadDualSenseMac(
            AzFramework::InputDeviceGamepad& inputDevice, void* gcController);
        ~InputDeviceGamepadDualSenseMac() override;

        bool IsConnected() const override;
        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized) override;
        void SetLightBarColor(const AZ::Color& color) override;
        void ResetLightBarColor() override;
        void TickInputDevice() override;

    private:
        RawGamepadState m_rawGamepadState;
        void* m_controller = nullptr; // GCController*, retained
        AZStd::unique_ptr<DualSenseHapticsMac> m_haptics;
        bool m_wasConnected = false;
    };
} // namespace DualSense
