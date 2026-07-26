
#include "DualSenseSystemComponent.h"

#include <DualSense/DualSenseTypeIds.h>

#include <AzCore/Serialization/SerializeContext.h>

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

    void DualSenseSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
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
    }

    void DualSenseSystemComponent::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
        DualSenseRequestBus::Handler::BusDisconnect();
    }

    void DualSenseSystemComponent::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
    }

} // namespace DualSense
