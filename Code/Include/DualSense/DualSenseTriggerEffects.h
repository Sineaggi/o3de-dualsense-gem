#pragma once

#include <DualSense/DualSenseTypeIds.h>

#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Math/Color.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>
#include <AzFramework/Input/Buses/Requests/InputLightBarRequestBus.h>
#include <AzFramework/Input/Devices/InputDeviceId.h>
#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

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

        if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
        {
            behaviorContext->EnumProperty<static_cast<AZ::u8>(Trigger::L2)>("DualSenseTrigger_L2")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext->EnumProperty<static_cast<AZ::u8>(Trigger::R2)>("DualSenseTrigger_R2")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext->EnumProperty<static_cast<AZ::u8>(Trigger::Both)>("DualSenseTrigger_Both")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");

            behaviorContext->EnumProperty<static_cast<AZ::u8>(TriggerEffectMode::Off)>("DualSenseTriggerEffectMode_Off")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext->EnumProperty<static_cast<AZ::u8>(TriggerEffectMode::Feedback)>("DualSenseTriggerEffectMode_Feedback")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext->EnumProperty<static_cast<AZ::u8>(TriggerEffectMode::Weapon)>("DualSenseTriggerEffectMode_Weapon")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext->EnumProperty<static_cast<AZ::u8>(TriggerEffectMode::Vibration)>("DualSenseTriggerEffectMode_Vibration")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext
                ->EnumProperty<static_cast<AZ::u8>(TriggerEffectMode::MultiPositionFeedback)>(
                    "DualSenseTriggerEffectMode_MultiPositionFeedback")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext
                ->EnumProperty<static_cast<AZ::u8>(TriggerEffectMode::MultiPositionVibration)>(
                    "DualSenseTriggerEffectMode_MultiPositionVibration")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");
            behaviorContext->EnumProperty<static_cast<AZ::u8>(TriggerEffectMode::SlopeFeedback)>("DualSenseTriggerEffectMode_SlopeFeedback")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense");

            behaviorContext->Class<TriggerEffect>("DualSenseTriggerEffect")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Attribute(AZ::Script::Attributes::Storage, AZ::Script::Attributes::StorageType::Value)
                ->Constructor()
                ->Property("mode", BehaviorValueProperty(&TriggerEffect::m_mode))
                ->Property("startPosition", BehaviorValueProperty(&TriggerEffect::m_startPosition))
                ->Property("endPosition", BehaviorValueProperty(&TriggerEffect::m_endPosition))
                ->Property("strength", BehaviorValueProperty(&TriggerEffect::m_strength))
                ->Property("endStrength", BehaviorValueProperty(&TriggerEffect::m_endStrength))
                ->Property("frequency", BehaviorValueProperty(&TriggerEffect::m_frequency))
                // AZStd::array<float, 10> is not a plain scalar: it round-trips to/from Lua only
                // because AZStd::array has an OnDemandReflection specialization
                // (AzCore/RTTI/AzStdOnDemandReflection.inl) that BehaviorContext queues
                // automatically for this property's type, exposing script-side
                // `effect.positionalValues:Fill(v)` / `:At(i)` / `:Replace(i, v)` / `:size()`
                // that mutate the real member (BehaviorValueProperty's getter returns T&, not a
                // copy). Verified end-to-end via a real Lua script in
                // Tests/Clients/DualSenseScriptReflectionTests.cpp
                // (ScriptRoundTripFixture.Lua_PositionalValuesArrayFillRoundTripsThroughBehaviorContext) —
                // no extra RegisterGenericType<AZStd::array<float,10>>() call is required.
                ->Property("positionalValues", BehaviorValueProperty(&TriggerEffect::m_positionalValues))
                ;

            AZ::BehaviorParameterOverrides setTriggerEffectTriggerParam = { "Trigger", "Which trigger: L2, R2, or Both" };
            AZ::BehaviorParameterOverrides setTriggerEffectEffectParam = { "Effect", "The trigger effect to apply" };

            behaviorContext->EBus<DualSenseTriggerEffectRequestBus>("DualSenseTriggerEffectRequestBus")
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ->Event(
                    "SetTriggerEffect",
                    &DualSenseTriggerEffectRequestBus::Events::SetTriggerEffect,
                    { setTriggerEffectTriggerParam, setTriggerEffectEffectParam })
                ->Event("ClearTriggerEffects", &DualSenseTriggerEffectRequestBus::Events::ClearTriggerEffects)
                ;

            // Script-safe way to obtain the InputDeviceId to address DualSenseTriggerEffectRequestBus
            // at a given gamepad slot. Constructing InputDeviceId directly from Lua
            // (`InputDeviceId(InputDeviceGamepad.name, slotIndex)`) does NOT work: InputDeviceId is
            // reflected with two Constructor<>() overloads but is also default-constructible, and
            // AZ::ScriptContext's Lua binding only supports overload-free constructor dispatch (see
            // Code/Framework/AzCore/AzCore/Script/ScriptContext.cpp:5096-5137 in the engine) --
            // AzFramework::InputDeviceId::Reflect() never registers a
            // Script::Attributes::ConstructorOverride to fix that, so Lua construction silently
            // invokes the default constructor and discards both arguments, producing a garbage id.
            // This helper sidesteps that engine gap entirely by doing the construction in C++ and
            // handing Lua a ready-made, correctly-identified InputDeviceId. This is the documented
            // (README) way to get a deviceId for DualSenseTriggerEffectRequestBus from script.
            behaviorContext->Method(
                "DualSense_GetGamepadDeviceId",
                [](AZ::u32 slotIndex) { return AzFramework::InputDeviceGamepad::IdForIndexN(slotIndex); },
                {{ { "SlotIndex", "Gamepad slot (0-3)" } }})
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ;

            // The engine's own rumble output bus (AzFramework::InputHapticFeedbackRequestBus) is
            // not behavior-reflected (see spec §2, docs/superpowers/specs/2026-07-26-dualsense-
            // gem-design.md) -- this bridges it for scripts/test scenes (Phase 2.6 keyboard scene
            // coexistence checks).
            behaviorContext->Method(
                "DualSense_SetRumble",
                [](AZ::u32 slotIndex, float left, float right)
                {
                    AzFramework::InputHapticFeedbackRequestBus::Event(
                        AzFramework::InputDeviceGamepad::IdForIndexN(slotIndex),
                        &AzFramework::InputHapticFeedbackRequests::SetVibration,
                        left,
                        right);
                },
                {{ { "SlotIndex", "Gamepad slot (0-3)" },
                   { "Left", "Left (large/low frequency) motor speed, normalized [0,1]" },
                   { "Right", "Right (small/high frequency) motor speed, normalized [0,1]" } }})
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ;

            // The engine's own light bar output bus (AzFramework::InputLightBarRequestBus) is not
            // behavior-reflected (see spec §2) -- this bridges it for scripts/test scenes.
            behaviorContext->Method(
                "DualSense_SetLightBar",
                [](AZ::u32 slotIndex, float r, float g, float b)
                {
                    AzFramework::InputLightBarRequestBus::Event(
                        AzFramework::InputDeviceGamepad::IdForIndexN(slotIndex),
                        &AzFramework::InputLightBarRequests::SetLightBarColor,
                        AZ::Color(r, g, b, 1.0f));
                },
                {{ { "SlotIndex", "Gamepad slot (0-3)" },
                   { "R", "Red, normalized [0,1]" },
                   { "G", "Green, normalized [0,1]" },
                   { "B", "Blue, normalized [0,1]" } }})
                ->Attribute(AZ::Script::Attributes::Module, "dualsense")
                ->Attribute(AZ::Script::Attributes::Category, "DualSense")
                ->Attribute(AZ::Script::Attributes::Scope, AZ::Script::Attributes::ScopeFlags::Common)
                ;
        }
    }

} // namespace DualSense
