#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <DualSense/DualSenseTriggerEffects.h>
#include <DualSense/DualSenseHaptics.h>

namespace DualSense
{
    class DualSenseHapticsMac;

    //! Standard-gamepad backend for a DualSense driven by GameController.framework.
    class InputDeviceGamepadDualSenseMac
        : public AzFramework::InputDeviceGamepad::Implementation
        , public DualSenseTriggerEffectRequestBus::Handler
        , public DualSenseHapticPulseRequestBus::Handler
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

        ////////////////////////////////////////////////////////////////////////////////////////////
        // DualSenseHapticPulseRequestBus::Handler
        void PlayHapticPulse(float leftIntensity, float rightIntensity, float sharpness) override;
        void SetAutoRecoil(Trigger trigger, bool enabled, float intensity, float sharpness) override;

    private:
        //! Applies an already-clamped (and, if necessary, degraded) effect to a single
        //! adaptive trigger. `gcAdaptiveTrigger` is a GCDualSenseAdaptiveTrigger*, passed
        //! as void* so this header stays free of GameController.framework types.
        void ApplyEffectToTrigger(void* gcAdaptiveTrigger, const TriggerEffect& clamped);

        //! Per-trigger auto-recoil configuration set via SetAutoRecoil. This task (Phase 2.5,
        //! Task 1) only stores the values; Task 2 consumes them to fire PlayTransientPulse
        //! automatically on Weapon-mode fire edges. Plain data -- nothing to tear down.
        struct AutoRecoilConfig
        {
            bool m_enabled = false;
            float m_intensity = 0.0f;
            float m_sharpness = 0.0f;
        };

        RawGamepadState m_rawGamepadState;
        void* m_controller = nullptr; // GCController*, retained
        AZStd::unique_ptr<DualSenseHapticsMac> m_haptics;
        bool m_wasConnected = false;
        AutoRecoilConfig m_leftAutoRecoil;  // L2
        AutoRecoilConfig m_rightAutoRecoil; // R2
    };
} // namespace DualSense
