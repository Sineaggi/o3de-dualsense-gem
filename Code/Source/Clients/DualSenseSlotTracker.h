#pragma once

#include <AzCore/base.h>
#include <AzCore/std/containers/array.h>

namespace DualSense
{
    //! Tracks which engine gamepad slot (0..3) each detected DualSense occupies.
    //! Pure C++ so it is unit-testable and reusable by non-Mac backends.
    class DualSenseSlotTracker
    {
    public:
        static constexpr AZ::u32 InvalidSlot = 0xFFFFFFFF;
        static constexpr AZ::u32 MaxSlots = 4;

        AZ::u32 Assign(const void* deviceKey, AZ::u32 preferredSlot);
        AZ::u32 Release(const void* deviceKey);
        AZ::u32 SlotOf(const void* deviceKey) const;

    private:
        AZStd::array<const void*, MaxSlots> m_slots{{ nullptr, nullptr, nullptr, nullptr }};
    };
} // namespace DualSense
