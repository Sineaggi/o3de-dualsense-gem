
#include "DualSenseSystemComponent.h"

#include "DualSenseDebugGamepadImpl.h"

#include <DualSense/DualSenseTypeIds.h>
#include <DualSense/DualSenseTriggerEffects.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>
#include <AzFramework/Input/Buses/Requests/InputLightBarRequestBus.h>

namespace DualSense
{
    AZ_COMPONENT_IMPL(DualSenseSystemComponent, "DualSenseSystemComponent",
        DualSenseSystemComponentTypeId);

    void DualSenseSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DualSenseSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }

        TriggerEffect::Reflect(context);
    }

    void DualSenseSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("DualSenseService"));
    }

    void DualSenseSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("DualSenseService"));
    }

    void DualSenseSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
    }

    void DualSenseSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        // Soft dependencies: order after the input system when present, but do not
        // hard-require it (this component also activates in AssetProcessor/AssetBuilder
        // via the Builders variant, where no input system exists).
        dependent.push_back(AZ_CRC_CE("InputSystemService"));
        dependent.push_back(AZ_CRC_CE("NativeUIInputSystemService"));
    }

    DualSenseSystemComponent::DualSenseSystemComponent()
    {
        if (DualSenseInterface::Get() == nullptr)
        {
            DualSenseInterface::Register(this);
        }
    }

    DualSenseSystemComponent::~DualSenseSystemComponent()
    {
        if (DualSenseInterface::Get() == this)
        {
            DualSenseInterface::Unregister(this);
        }
    }

    void DualSenseSystemComponent::Init()
    {
    }

    void DualSenseSystemComponent::Activate()
    {
        DualSenseRequestBus::Handler::BusConnect();
        AZ::TickBus::Handler::BusConnect();
        m_impl = DualSenseSystemImpl::Create(*this);
    }

    void DualSenseSystemComponent::Deactivate()
    {
        m_impl.reset();
        AZ::TickBus::Handler::BusDisconnect();
        DualSenseRequestBus::Handler::BusDisconnect();
    }

    void DualSenseSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        if (m_impl)
        {
            m_impl->Tick();
        }
    }

    void DualSenseSystemComponent::SwapSlotToFactory(
        AZ::u32 slotIndex, AzFramework::InputDeviceGamepad::ImplementationFactory* factory)
    {
        using SwapBus = AzFramework::InputDeviceImplementationRequest<AzFramework::InputDeviceGamepad>;
        SwapBus::Bus::Event(
            AzFramework::InputDeviceGamepad::IdForIndexN(slotIndex),
            &SwapBus::SetCustomImplementation,
            factory);
    }

    void DualSenseSystemComponent::RestoreSlotToPlatformDefault(AZ::u32 slotIndex)
    {
        auto* platformFactory =
            AZ::Interface<AzFramework::InputDeviceGamepad::ImplementationFactory>::Get();
        if (platformFactory == nullptr)
        {
            AZLOG_WARN("DualSense: no platform gamepad factory registered; restore for slot %u skipped", slotIndex);
            return;
        }
        SwapSlotToFactory(slotIndex, platformFactory);
    }

    namespace DebugCommands
    {
        static DualSenseDebugGamepadImplFactory s_debugFactory;

        static AZ::u32 SlotFromArgs(const AZ::ConsoleCommandContainer& arguments)
        {
            if (!arguments.empty())
            {
                return static_cast<AZ::u32>(strtoul(AZStd::string(arguments.front()).c_str(), nullptr, 10));
            }
            return 0;
        }

        static void dualsense_debug_swap(const AZ::ConsoleCommandContainer& arguments)
        {
            const AZ::u32 slot = SlotFromArgs(arguments);
            AZLOG_INFO("DualSense: swapping gamepad slot %u to debug implementation", slot);
            DualSenseSystemComponent::SwapSlotToFactory(slot, &s_debugFactory);
        }
        AZ_CONSOLEFREEFUNC(dualsense_debug_swap, AZ::ConsoleFunctorFlags::DontReplicate,
            "Swap a gamepad slot (arg, default 0) to the DualSense debug implementation");

        static void dualsense_debug_restore(const AZ::ConsoleCommandContainer& arguments)
        {
            const AZ::u32 slot = SlotFromArgs(arguments);
            AZLOG_INFO("DualSense: restoring gamepad slot %u to platform default", slot);
            DualSenseSystemComponent::RestoreSlotToPlatformDefault(slot);
        }
        AZ_CONSOLEFREEFUNC(dualsense_debug_restore, AZ::ConsoleFunctorFlags::DontReplicate,
            "Restore a gamepad slot (arg, default 0) to the platform-default implementation");

        static void dualsense_rumble(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() == 1)
            {
                AZLOG_INFO("Usage: dualsense_rumble <left 0-1> <right 0-1> [slot]");
                return;
            }
            float left = 0.5f;
            float right = 0.5f;
            AZ::u32 slot = 0;
            if (arguments.size() >= 2)
            {
                left = static_cast<float>(atof(AZStd::string(arguments[0]).c_str()));
                right = static_cast<float>(atof(AZStd::string(arguments[1]).c_str()));
            }
            if (arguments.size() >= 3)
            {
                slot = static_cast<AZ::u32>(strtoul(AZStd::string(arguments[2]).c_str(), nullptr, 10));
            }
            AzFramework::InputHapticFeedbackRequestBus::Event(
                AzFramework::InputDeviceGamepad::IdForIndexN(slot),
                &AzFramework::InputHapticFeedbackRequests::SetVibration, left, right);
        }
        AZ_CONSOLEFREEFUNC(dualsense_rumble, AZ::ConsoleFunctorFlags::DontReplicate,
            "Send SetVibration to a gamepad slot: dualsense_rumble <left 0-1> <right 0-1> [slot]");

        static void dualsense_lightbar(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() < 3)
            {
                AZLOG_INFO("Usage: dualsense_lightbar <r 0-1> <g 0-1> <b 0-1> [slot]");
                return;
            }
            const float r = static_cast<float>(atof(AZStd::string(arguments[0]).c_str()));
            const float g = static_cast<float>(atof(AZStd::string(arguments[1]).c_str()));
            const float b = static_cast<float>(atof(AZStd::string(arguments[2]).c_str()));
            const AZ::u32 slot = arguments.size() >= 4
                ? static_cast<AZ::u32>(strtoul(AZStd::string(arguments[3]).c_str(), nullptr, 10)) : 0;
            AzFramework::InputLightBarRequestBus::Event(
                AzFramework::InputDeviceGamepad::IdForIndexN(slot),
                &AzFramework::InputLightBarRequests::SetLightBarColor,
                AZ::Color(r, g, b, 1.0f));
        }
        AZ_CONSOLEFREEFUNC(dualsense_lightbar, AZ::ConsoleFunctorFlags::DontReplicate,
            "Set a gamepad slot's light bar color: dualsense_lightbar <r> <g> <b> [slot]");

        static TriggerEffect CreateTriggerEffectForMode(const AZStd::string& modeStr)
        {
            TriggerEffect effect;

            if (modeStr == "off")
            {
                effect.m_mode = TriggerEffectMode::Off;
            }
            else if (modeStr == "feedback")
            {
                effect.m_mode = TriggerEffectMode::Feedback;
                effect.m_startPosition = 0.3f;
                effect.m_strength = 0.8f;
            }
            else if (modeStr == "weapon")
            {
                effect.m_mode = TriggerEffectMode::Weapon;
                effect.m_startPosition = 0.2f;
                effect.m_endPosition = 0.6f;
                effect.m_strength = 0.9f;
            }
            else if (modeStr == "vibration")
            {
                effect.m_mode = TriggerEffectMode::Vibration;
                effect.m_startPosition = 0.2f;
                effect.m_strength = 0.75f;
                effect.m_frequency = 0.6f;
            }
            else if (modeStr == "slope")
            {
                effect.m_mode = TriggerEffectMode::SlopeFeedback;
                effect.m_startPosition = 0.2f;
                effect.m_endPosition = 0.9f;
                effect.m_strength = 0.3f;
                effect.m_endStrength = 1.0f;
            }
            else if (modeStr == "multifeedback")
            {
                effect.m_mode = TriggerEffectMode::MultiPositionFeedback;
                effect.m_positionalValues = {{0.0f, 0.0f, 0.3f, 0.3f, 0.6f, 0.6f, 0.9f, 0.9f, 1.0f, 1.0f}};
            }
            else if (modeStr == "multivibration")
            {
                effect.m_mode = TriggerEffectMode::MultiPositionVibration;
                effect.m_positionalValues = {{0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.5f}};
                effect.m_frequency = 0.5f;
            }

            return effect;
        }

        static void dualsense_trigger(const AZ::ConsoleCommandContainer& arguments)
        {
            if (arguments.size() < 2)
            {
                AZLOG_INFO("Usage: dualsense_trigger <l2|r2|both> <off|feedback|weapon|vibration|slope|multifeedback|multivibration> [slot]");
                return;
            }

            AZStd::string triggerStr(arguments[0]);
            AZStd::string modeStr(arguments[1]);
            AZ::u32 slot = 0;

            if (arguments.size() >= 3)
            {
                slot = static_cast<AZ::u32>(strtoul(AZStd::string(arguments[2]).c_str(), nullptr, 10));
            }

            // Parse trigger
            Trigger trigger;
            if (triggerStr == "l2")
            {
                trigger = Trigger::L2;
            }
            else if (triggerStr == "r2")
            {
                trigger = Trigger::R2;
            }
            else if (triggerStr == "both")
            {
                trigger = Trigger::Both;
            }
            else
            {
                AZLOG_INFO("Usage: dualsense_trigger <l2|r2|both> <off|feedback|weapon|vibration|slope|multifeedback|multivibration> [slot]");
                return;
            }

            // Create effect for mode
            TriggerEffect effect = CreateTriggerEffectForMode(modeStr);

            // Check if the mode was valid (only Off has special handling)
            if (effect.m_mode == TriggerEffectMode::Off && modeStr != "off")
            {
                AZLOG_INFO("Usage: dualsense_trigger <l2|r2|both> <off|feedback|weapon|vibration|slope|multifeedback|multivibration> [slot]");
                return;
            }

            DualSenseTriggerEffectRequestBus::Event(
                AzFramework::InputDeviceGamepad::IdForIndexN(slot),
                &DualSenseTriggerEffectRequests::SetTriggerEffect,
                trigger,
                effect);
        }
        AZ_CONSOLEFREEFUNC(dualsense_trigger, AZ::ConsoleFunctorFlags::DontReplicate,
            "Set a trigger effect: dualsense_trigger <l2|r2|both> <off|feedback|weapon|vibration|slope|multifeedback|multivibration> [slot]");

        static void dualsense_trigger_clear(const AZ::ConsoleCommandContainer& arguments)
        {
            const AZ::u32 slot = SlotFromArgs(arguments);
            AZLOG_INFO("DualSense: clearing trigger effects for gamepad slot %u", slot);
            DualSenseTriggerEffectRequestBus::Event(
                AzFramework::InputDeviceGamepad::IdForIndexN(slot),
                &DualSenseTriggerEffectRequests::ClearTriggerEffects);
        }
        AZ_CONSOLEFREEFUNC(dualsense_trigger_clear, AZ::ConsoleFunctorFlags::DontReplicate,
            "Clear all trigger effects for a gamepad slot: dualsense_trigger_clear [slot]");
    } // namespace DebugCommands

} // namespace DualSense
