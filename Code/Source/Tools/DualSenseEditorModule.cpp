
#include <DualSense/DualSenseTypeIds.h>
#include <DualSenseModuleInterface.h>
#include "DualSenseEditorSystemComponent.h"

namespace DualSense
{
    class DualSenseEditorModule
        : public DualSenseModuleInterface
    {
    public:
        AZ_RTTI(DualSenseEditorModule, DualSenseEditorModuleTypeId, DualSenseModuleInterface);
        AZ_CLASS_ALLOCATOR(DualSenseEditorModule, AZ::SystemAllocator);

        DualSenseEditorModule()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            // Add ALL components descriptors associated with this gem to m_descriptors.
            // This will associate the AzTypeInfo information for the components with the the SerializeContext, BehaviorContext and EditContext.
            // This happens through the [MyComponent]::Reflect() function.
            m_descriptors.insert(m_descriptors.end(), {
                DualSenseEditorSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         * Non-SystemComponents should not be added here
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {
                azrtti_typeid<DualSenseEditorSystemComponent>(),
            };
        }
    };
}// namespace DualSense

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME, _Editor), DualSense::DualSenseEditorModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_DualSense_Editor, DualSense::DualSenseEditorModule)
#endif
