#pragma once

#include <DualSense/DualSenseTriggerEffects.h> // Trigger enum

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/EBus/EBus.h>
#include <AzFramework/Input/Devices/InputDeviceId.h>

namespace DualSense
{
    //! One-shot transient haptic kicks (distinct from the continuous SetVibration rumble
    //! motors) plus per-trigger auto-recoil configuration for Weapon-mode fire edges.
    //! Addressed per-gamepad, same as DualSenseTriggerEffectRequestBus.
    class DualSenseHapticPulseRequests : public AZ::EBusTraits
    {   // traits identical to DualSenseTriggerEffectRequests (ById, Single, BusIdType=InputDeviceId)
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AzFramework::InputDeviceId;

        //! One sharp transient kick. Intensities/sharpness normalized [0,1]; 0 intensity = skip that side.
        virtual void PlayHapticPulse(float leftIntensity, float rightIntensity, float sharpness) = 0;
        //! Enable/disable hardware-synchronized auto-recoil for a trigger's Weapon-mode fire edge.
        virtual void SetAutoRecoil(Trigger trigger, bool enabled, float intensity, float sharpness) = 0;
        virtual ~DualSenseHapticPulseRequests() = default;
    };

    using DualSenseHapticPulseRequestBus = AZ::EBus<DualSenseHapticPulseRequests>;

    //! Reflects DualSenseHapticPulseRequestBus to the BehaviorContext (Script Canvas/Lua).
    //! Owned here as a free function (rather than as a static method on a reflected data
    //! struct, cf. TriggerEffect::Reflect) because this bus has no data type of its own --
    //! PlayHapticPulse/SetAutoRecoil take only scalars plus the already-reflected Trigger
    //! enum (reflected by TriggerEffect::Reflect). Called from
    //! DualSenseSystemComponent::Reflect alongside TriggerEffect::Reflect.
    inline void ReflectDualSenseHapticPulseBus(AZ::ReflectContext* context)
    {
        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            AZ::BehaviorParameterOverrides playHapticPulseLeftParam =
                { "LeftIntensity", "Left actuator intensity [0,1]; 0 = skip that side" };
            AZ::BehaviorParameterOverrides playHapticPulseRightParam =
                { "RightIntensity", "Right actuator intensity [0,1]; 0 = skip that side" };
            AZ::BehaviorParameterOverrides playHapticPulseSharpnessParam =
                { "Sharpness", "Transient sharpness [0,1]" };

            AZ::BehaviorParameterOverrides setAutoRecoilTriggerParam =
                { "Trigger", "Which trigger: L2, R2, or Both" };
            AZ::BehaviorParameterOverrides setAutoRecoilEnabledParam =
                { "Enabled", "Enable or disable hardware-synchronized auto-recoil" };
            AZ::BehaviorParameterOverrides setAutoRecoilIntensityParam =
                { "Intensity", "Recoil kick intensity [0,1]" };
            AZ::BehaviorParameterOverrides setAutoRecoilSharpnessParam =
                { "Sharpness", "Recoil kick sharpness [0,1]" };

            behaviorContext->EBus<DualSenseHapticPulseRequestBus>("DualSenseHapticPulseRequestBus")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Event(
                    "PlayHapticPulse",
                    &DualSenseHapticPulseRequestBus::Events::PlayHapticPulse,
                    { playHapticPulseLeftParam, playHapticPulseRightParam, playHapticPulseSharpnessParam })
                ->Event(
                    "SetAutoRecoil",
                    &DualSenseHapticPulseRequestBus::Events::SetAutoRecoil,
                    { setAutoRecoilTriggerParam, setAutoRecoilEnabledParam, setAutoRecoilIntensityParam,
                      setAutoRecoilSharpnessParam })
                ;
        }
    }

} // namespace DualSense
