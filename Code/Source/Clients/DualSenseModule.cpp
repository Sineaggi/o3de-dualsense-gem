
#include <DualSense/DualSenseTypeIds.h>
#include <DualSenseModuleInterface.h>
#include "DualSenseSystemComponent.h"

namespace DualSense
{
    class DualSenseModule
        : public DualSenseModuleInterface
    {
    public:
        AZ_RTTI(DualSenseModule, DualSenseModuleTypeId, DualSenseModuleInterface);
        AZ_CLASS_ALLOCATOR(DualSenseModule, AZ::SystemAllocator);
    };
}// namespace DualSense

#if defined(O3DE_GEM_NAME)
AZ_DECLARE_MODULE_CLASS(AZ_JOIN(Gem_, O3DE_GEM_NAME), DualSense::DualSenseModule)
#else
AZ_DECLARE_MODULE_CLASS(Gem_DualSense, DualSense::DualSenseModule)
#endif
