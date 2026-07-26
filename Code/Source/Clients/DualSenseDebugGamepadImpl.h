#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Phase-0 stand-in backend: proves the SetCustomImplementation swap works.
    //! Reports connected, records the last vibration request, ticks an all-zero
    //! raw state so every channel honors the once-per-frame update contract.
    class DualSenseDebugGamepadImpl
        : public AzFramework::InputDeviceGamepad::Implementation
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator
        AZ_CLASS_ALLOCATOR(DualSenseDebugGamepadImpl, AZ::SystemAllocator);

        explicit DualSenseDebugGamepadImpl(AzFramework::InputDeviceGamepad& inputDevice);

        bool IsConnected() const override;
        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized) override;
        void TickInputDevice() override;

        float m_lastVibrationLeft = -1.0f;
        float m_lastVibrationRight = -1.0f;

    private:
        RawGamepadState m_rawGamepadState;
    };

    struct DualSenseDebugGamepadImplFactory
        : public AzFramework::InputDeviceGamepad::ImplementationFactory
    {
        AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> Create(
            AzFramework::InputDeviceGamepad& inputDevice) override;
        AZ::u32 GetMaxSupportedGamepads() const override;

        DualSenseDebugGamepadImpl* m_lastCreated = nullptr; // non-owning, observation only
    };
} // namespace DualSense
