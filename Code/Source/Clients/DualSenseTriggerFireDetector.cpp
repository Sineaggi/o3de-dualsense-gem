#include <Clients/DualSenseTriggerFireDetector.h>

namespace DualSense
{
    bool IsWeaponFireEdge(WeaponTriggerStatus previous, WeaponTriggerStatus current)
    {
        return current == WeaponTriggerStatus::Fired && previous != WeaponTriggerStatus::Fired;
    }
} // namespace DualSense
