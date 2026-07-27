#pragma once

// Phase 3a Task 3. Whole-file guarded -- see DualSenseSdlRuntime.h's header comment for why.
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <Clients/DualSenseSlotTracker.h>

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <AzCore/std/containers/vector.h>

#include <SDL3/SDL.h>

namespace DualSense
{
    class DualSenseSdlRuntime;

    //! Factory for InputDeviceGamepadDualSenseSdl. m_pendingJoystickId must be set to the target
    //! SDL_JoystickID immediately before the swap bus event fires (synchronous dispatch) and
    //! cleared after -- same contract as DualSenseMacGamepadImplFactory::m_pendingController.
    //! 0 is SDL's own "invalid id" sentinel (SDL_JoystickID instance ids start at 1), used here
    //! the same way DualSenseMacGamepadImplFactory uses a null m_pendingController.
    struct DualSenseSdlGamepadImplFactory
        : public AzFramework::InputDeviceGamepad::ImplementationFactory
    {
        AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> Create(
            AzFramework::InputDeviceGamepad& inputDevice) override;
        AZ::u32 GetMaxSupportedGamepads() const override { return 4; }

        SDL_JoystickID m_pendingJoystickId = 0;
    };

    //! SDL counterpart of DualSenseSystemImplMac's GameController-notification-driven monitor.
    //! SDL3 has no push-based hotplug callback reachable without owning SDL's global event
    //! queue (SDL_PollEvent et al.) -- and this gem deliberately does not want to become SDL's
    //! event-queue owner (that would put it in a "who pumps SDL_Event" turf war with anything
    //! else in-process that might also want SDL events; the joystick STATE side, which is all
    //! this gem needs, is fully covered by SDL_UpdateJoysticks() + polling SDL_GetJoysticks(),
    //! neither of which requires draining SDL_Event at all). So detection here is per-tick
    //! list-diffing: enumerate current PS5 joystick ids (DualSenseSdlRuntime::EnumeratePs5Joysticks)
    //! and diff against the set this monitor itself swapped in, exactly the same slot-tracker +
    //! DualSenseSystemComponent::SwapSlotToFactory/RestoreSlotToPlatformDefault calls as the Mac
    //! monitor's notification handlers use.
    class DualSenseSdlMonitor
    {
    public:
        explicit DualSenseSdlMonitor(DualSenseSdlRuntime& runtime);
        ~DualSenseSdlMonitor();

        //! Called from DualSenseSystemImplMac::Tick() while the sdl backend is selected.
        void Tick();

    private:
        void OnJoystickConnected(SDL_JoystickID id);
        void OnJoystickDisconnected(SDL_JoystickID id);

        DualSenseSdlRuntime& m_runtime;
        DualSenseSdlGamepadImplFactory m_factory;
        DualSenseSlotTracker m_slotTracker;
        //! Ids this monitor has swapped in, so Tick() can diff SDL's current enumeration against
        //! "what we already know about" instead of re-deriving that set from m_slotTracker
        //! (which only exposes lookups, not full enumeration).
        AZStd::vector<SDL_JoystickID> m_trackedIds;
    };
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
