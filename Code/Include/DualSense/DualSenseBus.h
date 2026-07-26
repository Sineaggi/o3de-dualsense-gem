
#pragma once

#include <DualSense/DualSenseTypeIds.h>

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace DualSense
{
    class DualSenseRequests
    {
    public:
        AZ_RTTI(DualSenseRequests, DualSenseRequestsTypeId);
        virtual ~DualSenseRequests() = default;
        // Put your public methods here
    };

    class DualSenseBusTraits
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static constexpr AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
        static constexpr AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////
    };

    using DualSenseRequestBus = AZ::EBus<DualSenseRequests, DualSenseBusTraits>;
    using DualSenseInterface = AZ::Interface<DualSenseRequests>;

} // namespace DualSense
