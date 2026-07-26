
#include <AzCore/Serialization/SerializeContext.h>
#include "DualSenseEditorSystemComponent.h"

#include <DualSense/DualSenseTypeIds.h>

namespace DualSense
{
    AZ_COMPONENT_IMPL(DualSenseEditorSystemComponent, "DualSenseEditorSystemComponent",
        DualSenseEditorSystemComponentTypeId, BaseSystemComponent);

    void DualSenseEditorSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<DualSenseEditorSystemComponent, DualSenseSystemComponent>()
                ->Version(0);
        }
    }

    DualSenseEditorSystemComponent::DualSenseEditorSystemComponent() = default;

    DualSenseEditorSystemComponent::~DualSenseEditorSystemComponent() = default;

    void DualSenseEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        BaseSystemComponent::GetProvidedServices(provided);
        provided.push_back(AZ_CRC_CE("DualSenseEditorService"));
    }

    void DualSenseEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        BaseSystemComponent::GetIncompatibleServices(incompatible);
        incompatible.push_back(AZ_CRC_CE("DualSenseEditorService"));
    }

    void DualSenseEditorSystemComponent::GetRequiredServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        BaseSystemComponent::GetRequiredServices(required);
    }

    void DualSenseEditorSystemComponent::GetDependentServices([[maybe_unused]] AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        BaseSystemComponent::GetDependentServices(dependent);
    }

    void DualSenseEditorSystemComponent::Activate()
    {
        DualSenseSystemComponent::Activate();
        AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
    }

    void DualSenseEditorSystemComponent::Deactivate()
    {
        AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
        DualSenseSystemComponent::Deactivate();
    }

} // namespace DualSense
