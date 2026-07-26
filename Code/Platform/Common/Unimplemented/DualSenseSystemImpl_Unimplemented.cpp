#include <Clients/DualSenseSystemImpl.h>

namespace DualSense
{
    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent&)
    {
        return nullptr; // Platform has no DualSense backend; gem stays passive.
    }
} // namespace DualSense
