#include <Clients/DualSenseSlotTracker.h>

namespace DualSense
{
    AZ::u32 DualSenseSlotTracker::Assign(const void* deviceKey, AZ::u32 preferredSlot)
    {
        if (const AZ::u32 existing = SlotOf(deviceKey); existing != InvalidSlot)
        {
            return existing;
        }
        if (preferredSlot < MaxSlots && m_slots[preferredSlot] == nullptr)
        {
            m_slots[preferredSlot] = deviceKey;
            return preferredSlot;
        }
        for (AZ::u32 i = 0; i < MaxSlots; ++i)
        {
            if (m_slots[i] == nullptr)
            {
                m_slots[i] = deviceKey;
                return i;
            }
        }
        return InvalidSlot;
    }

    AZ::u32 DualSenseSlotTracker::Release(const void* deviceKey)
    {
        for (AZ::u32 i = 0; i < MaxSlots; ++i)
        {
            if (m_slots[i] == deviceKey)
            {
                m_slots[i] = nullptr;
                return i;
            }
        }
        return InvalidSlot;
    }

    AZ::u32 DualSenseSlotTracker::SlotOf(const void* deviceKey) const
    {
        for (AZ::u32 i = 0; i < MaxSlots; ++i)
        {
            if (m_slots[i] == deviceKey)
            {
                return i;
            }
        }
        return InvalidSlot;
    }
} // namespace DualSense
