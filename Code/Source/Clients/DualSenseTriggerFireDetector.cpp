#include <Clients/DualSenseTriggerFireDetector.h>

namespace DualSense
{
    bool IsWeaponFireEdge(WeaponTriggerStatus previous, WeaponTriggerStatus current)
    {
        // previous == Unknown is deliberately excluded (not just previous == Fired): Unknown
        // means "no real prior observation" (default-constructed, or reset on a pad-nil/
        // below-OS-floor tick), so the first observation after one of those resets is a
        // baseline, never an edge -- see the header doc comment for the reconnect-blip
        // rationale this guards against.
        return current == WeaponTriggerStatus::Fired &&
            (previous == WeaponTriggerStatus::Ready || previous == WeaponTriggerStatus::Firing);
    }
} // namespace DualSense
