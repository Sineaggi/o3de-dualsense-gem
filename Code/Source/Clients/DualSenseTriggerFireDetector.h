#pragma once

#include <AzCore/base.h>

namespace DualSense
{
    //! Gem-side (ObjC-free) mirror of the subset of GCDualSenseAdaptiveTriggerStatus that
    //! matters for Weapon-mode fire-edge detection. The Mac tick loop maps the raw
    //! GCDualSenseAdaptiveTrigger.status integral value into this enum (unknown/unsupported
    //! statuses, and any @try/@catch failure reading .status, both map to Unknown) before
    //! calling IsWeaponFireEdge below, so this header and its .cpp stay free of
    //! GameController.framework types and are plain-C++ unit-testable.
    enum class WeaponTriggerStatus : AZ::u8
    {
        Unknown,
        Ready,
        Firing,
        Fired
    };

    //! Pure, unit-testable: true exactly on the transition INTO the fired state FROM an
    //! actually-observed prior Weapon-mode state (i.e. `current == Fired && (previous == Ready
    //! || previous == Firing)`). `previous == Unknown` is explicitly excluded and NEVER fires,
    //! even when `current == Fired`: `Unknown` means "no real prior observation" (a trigger's
    //! previous-state member is default-constructed to Unknown, and is reset back to Unknown on
    //! every pad-nil/below-OS-floor tick -- not just cold start, but also transient Bluetooth
    //! reconnect blips). Treating the first observation after such a reset as a baseline rather
    //! than an edge avoids a phantom notification + phantom auto-recoil kick on the exact frame
    //! a pad reconnects mid-fire-animation, when the freshly-read status can already be Fired.
    bool IsWeaponFireEdge(WeaponTriggerStatus previous, WeaponTriggerStatus current);

} // namespace DualSense
