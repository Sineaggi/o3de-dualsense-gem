#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <DualSense/DualSenseTriggerEffects.h>

namespace DualSense
{
    class DualSenseHapticsMac;

    //! Standard-gamepad backend for a DualSense driven by GameController.framework.
    class InputDeviceGamepadDualSenseMac
        : public AzFramework::InputDeviceGamepad::Implementation
        , public DualSenseTriggerEffectRequestBus::Handler
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

        ////////////////////////////////////////////////////////////////////////////////////////////
        // DualSenseTriggerEffectRequestBus::Handler
        void SetTriggerEffect(Trigger trigger, const TriggerEffect& effect) override;
        void ClearTriggerEffects() override;

    private:
        //! Applies an already-clamped (and, if necessary, degraded) effect to a single
        //! adaptive trigger. `gcAdaptiveTrigger` is a GCDualSenseAdaptiveTrigger*, passed
        //! as void* so this header stays free of GameController.framework types.
        void ApplyEffectToTrigger(void* gcAdaptiveTrigger, const TriggerEffect& clamped);

        RawGamepadState m_rawGamepadState;
        void* m_controller = nullptr; // GCController*, retained
        AZStd::unique_ptr<DualSenseHapticsMac> m_haptics;
        bool m_wasConnected = false;
    };
} // namespace DualSense
