#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Standard-gamepad backend for a DualSense driven by GameController.framework.
    class InputDeviceGamepadDualSenseMac
        : public AzFramework::InputDeviceGamepad::Implementation
    {
    public:
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
        bool m_wasConnected = false;
    };
} // namespace DualSense
