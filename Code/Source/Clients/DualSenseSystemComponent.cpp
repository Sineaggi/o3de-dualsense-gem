
#include "DualSenseSystemComponent.h"

#include "DualSenseDebugGamepadImpl.h"

#include <DualSense/DualSenseTypeIds.h>

#include <AzCore/Console/IConsole.h>
#include <AzCore/Console/ILogger.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/Input/Buses/Requests/InputDeviceRequestBus.h>
#include <AzFramework/Input/Buses/Requests/InputHapticFeedbackRequestBus.h>

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
    } // namespace DebugCommands

} // namespace DualSense
