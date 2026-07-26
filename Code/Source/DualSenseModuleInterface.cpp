
#include "DualSenseModuleInterface.h"
#include <AzCore/Memory/Memory.h>

#include <DualSense/DualSenseTypeIds.h>

#include <Clients/DualSenseSystemComponent.h>

namespace DualSense
{
    AZ_TYPE_INFO_WITH_NAME_IMPL(DualSenseModuleInterface,
        "DualSenseModuleInterface", DualSenseModuleInterfaceTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(DualSenseModuleInterface, AZ::Module);
    AZ_CLASS_ALLOCATOR_IMPL(DualSenseModuleInterface, AZ::SystemAllocator);

    DualSenseModuleInterface::DualSenseModuleInterface()
    {
        // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
        // Add ALL components descriptors associated with this gem to m_descriptors.
        // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
        // This happens through the [MyComponent]::Reflect() function.
        m_descriptors.insert(m_descriptors.end(), {
            DualSenseSystemComponent::CreateDescriptor(),
            });
    }

    AZ::ComponentTypeList DualSenseModuleInterface::GetRequiredSystemComponents() const
    {
        return AZ::ComponentTypeList{
            azrtti_typeid<DualSenseSystemComponent>(),
        };
    }
} // namespace DualSense
