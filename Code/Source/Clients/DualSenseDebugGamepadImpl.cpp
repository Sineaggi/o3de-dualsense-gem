#include <Clients/DualSenseDebugGamepadImpl.h>
#include <Clients/DualSenseGamepadButtonMap.h>

#include <AzCore/Console/ILogger.h>

namespace DualSense
{
    const AzFramework::InputDeviceGamepad::Implementation::DigitalButtonIdByBitMaskMap&
        GetDualSenseDigitalButtonMap()
    {
        using Button = AzFramework::InputDeviceGamepad::Button;
        static const AzFramework::InputDeviceGamepad::Implementation::DigitalButtonIdByBitMaskMap map = {
            { ButtonBits::DPadUp,    &Button::DU },
            { ButtonBits::DPadDown,  &Button::DD },
            { ButtonBits::DPadLeft,  &Button::DL },
            { ButtonBits::DPadRight, &Button::DR },
            { ButtonBits::Start,     &Button::Start },
            { ButtonBits::Select,    &Button::Select },
            { ButtonBits::L3,        &Button::L3 },
            { ButtonBits::R3,        &Button::R3 },
            { ButtonBits::L1,        &Button::L1 },
            { ButtonBits::R1,        &Button::R1 },
            { ButtonBits::A,         &Button::A },
            { ButtonBits::B,         &Button::B },
            { ButtonBits::X,         &Button::X },
            { ButtonBits::Y,         &Button::Y },
        };
        return map;
    }

    DualSenseDebugGamepadImpl::DualSenseDebugGamepadImpl(AzFramework::InputDeviceGamepad& inputDevice)
        : AzFramework::InputDeviceGamepad::Implementation(inputDevice)
        , m_rawGamepadState(GetDualSenseDigitalButtonMap())
    {
        m_rawGamepadState.m_triggerMaximumValue = 1.0f;
        m_rawGamepadState.m_thumbStickMaximumValue = 1.0f;
        AZLOG_INFO("DualSense: debug gamepad implementation installed (device index %u)",
                   GetInputDeviceIndex());
        BroadcastInputDeviceConnectedEvent();
    }

    bool DualSenseDebugGamepadImpl::IsConnected() const
    {
        return true;
    }

    void DualSenseDebugGamepadImpl::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        m_lastVibrationLeft = leftMotorSpeedNormalized;
        m_lastVibrationRight = rightMotorSpeedNormalized;
        AZLOG_INFO("DualSense: debug SetVibration(%.2f, %.2f)", leftMotorSpeedNormalized, rightMotorSpeedNormalized);
    }

    void DualSenseDebugGamepadImpl::TickInputDevice()
    {
        ProcessRawGamepadState(m_rawGamepadState);
    }

    AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> DualSenseDebugGamepadImplFactory::Create(
        AzFramework::InputDeviceGamepad& inputDevice)
    {
        auto impl = AZStd::make_unique<DualSenseDebugGamepadImpl>(inputDevice);
        m_lastCreated = impl.get();
        return impl;
    }

    AZ::u32 DualSenseDebugGamepadImplFactory::GetMaxSupportedGamepads() const
    {
        return 4;
    }
} // namespace DualSense
