#include "InputDeviceGamepadDualSenseMac.h"
#include <Clients/DualSenseGamepadButtonMap.h>
#include "DualSenseMacGamepadImplFactory.h"
#include "DualSenseHapticsMac.h"

#include <AzCore/Console/ILogger.h>

#import <GameController/GameController.h>

namespace DualSense
{
    InputDeviceGamepadDualSenseMac::InputDeviceGamepadDualSenseMac(
        AzFramework::InputDeviceGamepad& inputDevice, void* gcController)
        : AzFramework::InputDeviceGamepad::Implementation(inputDevice)
        , m_rawGamepadState(GetDualSenseDigitalButtonMap())
        , m_controller(gcController)
    {
        if (m_controller)
        {
            // This translation unit is compiled without ARC (matching the rest of this
            // gem's Mac sources), so __bridge_retained is unavailable (it requires ARC's
            // cast-bridging to insert the retain). CFRetain/CFRelease work directly on
            // Objective-C objects because their retain counts are toll-free bridged with
            // CoreFoundation on Apple platforms, so this achieves the same "retain on
            // construction, CFRelease on destruction" contract without ARC.
            CFRetain(m_controller);
        }

        m_haptics = AZStd::make_unique<DualSenseHapticsMac>(gcController);

        // Values from the engine's InputDeviceGamepad_Mac.mm (Step 1): that file never
        // overrides these RawGamepadState fields, so it runs with the engine's own
        // untouched defaults (see RawGamepadState::RawGamepadState in
        // AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.cpp). We match those
        // same defaults here rather than invent new tuning.
        m_rawGamepadState.m_triggerMaximumValue = 1.0f;
        m_rawGamepadState.m_triggerDeadZoneValue = 0.0f;
        m_rawGamepadState.m_thumbStickMaximumValue = 1.0f;
        m_rawGamepadState.m_thumbStickLeftDeadZone = 0.0f;
        m_rawGamepadState.m_thumbStickRightDeadZone = 0.0f;
    }

    InputDeviceGamepadDualSenseMac::~InputDeviceGamepadDualSenseMac()
    {
        if (m_wasConnected)
        {
            BroadcastInputDeviceDisconnectedEvent();
        }
        m_haptics.reset(); // must be destroyed before the controller is released below
        if (m_controller)
        {
            CFRelease(m_controller); // balances the CFRetain in the constructor
        }
    }

    bool InputDeviceGamepadDualSenseMac::IsConnected() const
    {
        return m_controller != nullptr;
    }

    void InputDeviceGamepadDualSenseMac::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        if (m_haptics)
        {
            m_haptics->SetVibration(leftMotorSpeedNormalized, rightMotorSpeedNormalized);
        }
    }

    void InputDeviceGamepadDualSenseMac::SetLightBarColor(const AZ::Color&)
    {
        // Implemented in the lightbar task (GCDeviceLight).
    }

    void InputDeviceGamepadDualSenseMac::ResetLightBarColor()
    {
        // Implemented in the lightbar task (GCDeviceLight).
    }

    void InputDeviceGamepadDualSenseMac::TickInputDevice()
    {
        GCController* controller = (__bridge GCController*)m_controller;
        if (@available(macOS 11.3, *))
        {
            GCDualSenseGamepad* pad = (GCDualSenseGamepad*)controller.extendedGamepad;
            if (pad)
            {
                if (!m_wasConnected)
                {
                    m_wasConnected = true;
                    BroadcastInputDeviceConnectedEvent();
                }

                AZ::u32 buttons = 0;
                if (pad.dpad.up.pressed)              { buttons |= ButtonBits::DPadUp; }
                if (pad.dpad.down.pressed)            { buttons |= ButtonBits::DPadDown; }
                if (pad.dpad.left.pressed)            { buttons |= ButtonBits::DPadLeft; }
                if (pad.dpad.right.pressed)           { buttons |= ButtonBits::DPadRight; }
                if (pad.buttonMenu.pressed)           { buttons |= ButtonBits::Start; }
                if (pad.buttonOptions.pressed)        { buttons |= ButtonBits::Select; }
                if (pad.leftThumbstickButton.pressed) { buttons |= ButtonBits::L3; }
                if (pad.rightThumbstickButton.pressed){ buttons |= ButtonBits::R3; }
                if (pad.leftShoulder.pressed)         { buttons |= ButtonBits::L1; }
                if (pad.rightShoulder.pressed)        { buttons |= ButtonBits::R1; }
                if (pad.buttonA.pressed)              { buttons |= ButtonBits::A; }  // cross
                if (pad.buttonB.pressed)              { buttons |= ButtonBits::B; }  // circle
                if (pad.buttonX.pressed)              { buttons |= ButtonBits::X; }  // square
                if (pad.buttonY.pressed)              { buttons |= ButtonBits::Y; }  // triangle

                m_rawGamepadState.m_digitalButtonStates = buttons;
                m_rawGamepadState.m_triggerButtonLState  = pad.leftTrigger.value;
                m_rawGamepadState.m_triggerButtonRState  = pad.rightTrigger.value;
                m_rawGamepadState.m_thumbStickLeftXState  = pad.leftThumbstick.xAxis.value;
                m_rawGamepadState.m_thumbStickLeftYState  = pad.leftThumbstick.yAxis.value;
                m_rawGamepadState.m_thumbStickRightXState = pad.rightThumbstick.xAxis.value;
                m_rawGamepadState.m_thumbStickRightYState = pad.rightThumbstick.yAxis.value;
            }
        }
        ProcessRawGamepadState(m_rawGamepadState);
    }

    AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> DualSenseMacGamepadImplFactory::Create(
        AzFramework::InputDeviceGamepad& inputDevice)
    {
        if (m_pendingController == nullptr)
        {
            AZLOG_WARN("DualSense: Mac factory invoked with no pending controller");
            return nullptr;
        }
        return AZStd::make_unique<InputDeviceGamepadDualSenseMac>(inputDevice, m_pendingController);
    }
} // namespace DualSense
