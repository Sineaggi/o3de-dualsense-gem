#pragma once

#include <DualSense/DualSenseTypeIds.h>

#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Input/Devices/InputDeviceId.h>

namespace DualSense
{
    enum class Trigger : AZ::u8
    {
        L2,
        R2,
        Both
    };

    enum class TriggerEffectMode : AZ::u8
    {
        Off,
        Feedback,
        Weapon,
        Vibration,
        MultiPositionFeedback,
        MultiPositionVibration,
        SlopeFeedback
    };

    struct TriggerEffect
    {
        AZ_TYPE_INFO(TriggerEffect, DualSenseTriggerEffectTypeId);
        static void Reflect(AZ::ReflectContext* context);

        TriggerEffectMode m_mode = TriggerEffectMode::Off;
        float m_startPosition = 0.0f;   // all fields normalized [0,1]
        float m_endPosition = 1.0f;
        float m_strength = 0.0f;        // amplitude for vibration modes; startStrength for slope
        float m_endStrength = 0.0f;     // slope mode only
        float m_frequency = 0.0f;       // vibration modes only (normalized; spec amendment)
        AZStd::array<float, 10> m_positionalValues{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}; // multi-position modes

        TriggerEffect Clamped() const
        {
            TriggerEffect clamped = *this;

            // Helper lambda to clamp a value to [0,1]
            auto clampToUnit = [](float value) -> float
            {
                return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
            };

            // Clamp all float fields to [0,1]
            clamped.m_startPosition = clampToUnit(clamped.m_startPosition);
            clamped.m_endPosition = clampToUnit(clamped.m_endPosition);
            clamped.m_strength = clampToUnit(clamped.m_strength);
            clamped.m_endStrength = clampToUnit(clamped.m_endStrength);
            clamped.m_frequency = clampToUnit(clamped.m_frequency);

            // Clamp all positional values
            for (auto& value : clamped.m_positionalValues)
            {
                value = clampToUnit(value);
            }

            // Enforce endPosition >= startPosition
            if (clamped.m_endPosition < clamped.m_startPosition)
            {
                clamped.m_endPosition = clamped.m_startPosition;
            }

            return clamped;
        }
    };

    inline void TriggerEffect::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<TriggerEffect>()
                ->Version(1)
                ->Field("m_mode", &TriggerEffect::m_mode)
                ->Field("m_startPosition", &TriggerEffect::m_startPosition)
                ->Field("m_endPosition", &TriggerEffect::m_endPosition)
                ->Field("m_strength", &TriggerEffect::m_strength)
                ->Field("m_endStrength", &TriggerEffect::m_endStrength)
                ->Field("m_frequency", &TriggerEffect::m_frequency)
                ->Field("m_positionalValues", &TriggerEffect::m_positionalValues)
                ;
        }
    }

    class DualSenseTriggerEffectRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AzFramework::InputDeviceId;
        virtual ~DualSenseTriggerEffectRequests() = default;
        virtual void SetTriggerEffect(Trigger trigger, const TriggerEffect& effect) = 0;
        virtual void ClearTriggerEffects() = 0;
    };

    using DualSenseTriggerEffectRequestBus = AZ::EBus<DualSenseTriggerEffectRequests>;

} // namespace DualSense
