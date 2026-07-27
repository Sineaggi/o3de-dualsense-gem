#include "DualSenseSdlMonitor.h"

#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include "DualSenseSdlRuntime.h"
#include "InputDeviceGamepadDualSenseSdl.h"

#include <Clients/DualSenseSystemComponent.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/std/algorithm.h>

namespace DualSense
{
    namespace
    {
        //! DualSenseSlotTracker's key is an opaque `const void*` identity, never dereferenced --
        //! only ever compared for equality (see DualSenseSlotTracker.h/.cpp). GCController on
        //! Mac supplies a real Objective-C object pointer for that key; SDL_JoystickID is a plain
        //! Uint32, so it is encoded directly as a pointer VALUE (not a real pointer) here. Safe
        //! by the same "never dereferenced" contract, and stable for the lifetime of the id
        //! (SDL_JoystickID values are not reused while still connected).
        const void* AsDeviceKey(SDL_JoystickID id)
        {
            return reinterpret_cast<const void*>(static_cast<uintptr_t>(id));
        }
    } // namespace

    AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> DualSenseSdlGamepadImplFactory::Create(
        AzFramework::InputDeviceGamepad& inputDevice)
    {
        if (m_pendingJoystickId == 0)
        {
            AZLOG_WARN("DualSense (SDL): factory invoked with no pending joystick id");
            return nullptr;
        }
        return AZStd::make_unique<InputDeviceGamepadDualSenseSdl>(inputDevice, m_pendingJoystickId);
    }

    DualSenseSdlMonitor::DualSenseSdlMonitor(DualSenseSdlRuntime& runtime)
        : m_runtime(runtime)
    {
    }

    DualSenseSdlMonitor::~DualSenseSdlMonitor()
    {
        // Restore every slot this monitor still owns before tearing down, same as
        // DualSenseSystemImplMac's destructor does for its GameController-notification tracking.
        // OnJoystickDisconnected erases from m_trackedIds as it goes, so iterate a copy.
        const AZStd::vector<SDL_JoystickID> stillTracked = m_trackedIds;
        for (SDL_JoystickID id : stillTracked)
        {
            OnJoystickDisconnected(id);
        }
    }

    void DualSenseSdlMonitor::Tick()
    {
        if (!m_runtime.IsActive())
        {
            return;
        }

        m_runtime.PumpEvents();
        const AZStd::vector<SDL_JoystickID> current = m_runtime.EnumeratePs5Joysticks();

        // New connections: present now, not yet tracked. Collected first (rather than mutating
        // m_trackedIds mid-scan) so the loop below has a stable snapshot to compare against.
        AZStd::vector<SDL_JoystickID> newlyConnected;
        for (SDL_JoystickID id : current)
        {
            if (AZStd::find(m_trackedIds.begin(), m_trackedIds.end(), id) == m_trackedIds.end())
            {
                newlyConnected.push_back(id);
            }
        }
        for (SDL_JoystickID id : newlyConnected)
        {
            OnJoystickConnected(id);
        }

        // Disconnections: previously tracked, no longer present. OnJoystickDisconnected erases
        // from m_trackedIds, so collect the ids to drop first, then apply.
        AZStd::vector<SDL_JoystickID> newlyDisconnected;
        for (SDL_JoystickID id : m_trackedIds)
        {
            if (AZStd::find(current.begin(), current.end(), id) == current.end())
            {
                newlyDisconnected.push_back(id);
            }
        }
        for (SDL_JoystickID id : newlyDisconnected)
        {
            OnJoystickDisconnected(id);
        }

        // Drop any slot-exhaustion warning entries for ids that vanished from the enumeration
        // entirely (device unplugged while no slot was ever free for it) -- so if/when the same
        // physical id reappears later, it gets a fresh warning rather than being silently
        // suppressed forever by a stale entry. (A slot-exhausted id is never added to
        // m_trackedIds -- see OnJoystickConnected -- so it can't be picked up by the
        // newlyDisconnected loop above; it needs this separate pass.)
        for (auto it = m_slotExhaustionWarnedIds.begin(); it != m_slotExhaustionWarnedIds.end();)
        {
            if (AZStd::find(current.begin(), current.end(), *it) == current.end())
            {
                it = m_slotExhaustionWarnedIds.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void DualSenseSdlMonitor::OnJoystickConnected(SDL_JoystickID id)
    {
        // Per-tick list-diffing calls this every tick for as long as `id` isn't in m_trackedIds
        // -- which, for a slot-exhausted id, is EVERY tick it stays plugged in (it never makes it
        // into m_trackedIds since no slot was assigned). Gate both the transport log and the
        // "all slots occupied" warning on "have we already warned about this id" so a long-lived
        // 5th pad logs each exactly once, not once per tick, until something changes (a slot
        // frees up, or the device is unplugged -- see Tick()'s cleanup pass for the latter).
        const bool alreadyWarned =
            AZStd::find(m_slotExhaustionWarnedIds.begin(), m_slotExhaustionWarnedIds.end(), id) != m_slotExhaustionWarnedIds.end();

        if (!alreadyWarned)
        {
            // GUID bus-byte trick, logged once per newly-seen connection attempt (not once per
            // tick retry).
            DualSenseSdlRuntime::LogTransport(id);
        }

        const AZ::u32 slot = m_slotTracker.Assign(AsDeviceKey(id), 0);
        if (slot == DualSenseSlotTracker::InvalidSlot)
        {
            if (!alreadyWarned)
            {
                AZLOG_WARN("DualSense (SDL): controller detected but all 4 gamepad slots occupied");
                m_slotExhaustionWarnedIds.push_back(id);
            }
            return;
        }

        // A slot is now available for `id` (either it was never warned about, or one freed up
        // since the last warned tick) -- clear any stale warned-entry so a possible future
        // re-exhaustion for this same id (e.g. after a disconnect/reconnect this Tick() pass
        // didn't happen to purge first) starts from a clean slate.
        if (const auto it = AZStd::find(m_slotExhaustionWarnedIds.begin(), m_slotExhaustionWarnedIds.end(), id);
            it != m_slotExhaustionWarnedIds.end())
        {
            m_slotExhaustionWarnedIds.erase(it);
        }

        AZLOG_INFO("DualSense (SDL): controller detected, taking over gamepad slot %u", slot);
        m_factory.m_pendingJoystickId = id;
        DualSenseSystemComponent::SwapSlotToFactory(slot, &m_factory);
        m_factory.m_pendingJoystickId = 0;
        m_trackedIds.push_back(id);
    }

    void DualSenseSdlMonitor::OnJoystickDisconnected(SDL_JoystickID id)
    {
        const AZ::u32 slot = m_slotTracker.Release(AsDeviceKey(id));
        if (slot != DualSenseSlotTracker::InvalidSlot)
        {
            AZLOG_INFO("DualSense (SDL): controller left slot %u, restoring platform default", slot);
            DualSenseSystemComponent::RestoreSlotToPlatformDefault(slot);
        }

        if (const auto it = AZStd::find(m_trackedIds.begin(), m_trackedIds.end(), id); it != m_trackedIds.end())
        {
            m_trackedIds.erase(it);
        }
    }
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
