#pragma once

// Phase 3a Task 3: owns the process-wide SDL joystick+sensor subsystem lifecycle for the
// dualsense_backend=sdl path. Whole-file guarded (like DualSenseSdlBackendProbe.h before it) so
// this compiles to nothing -- not even an empty translation unit with dead code in it -- on any
// platform where DUALSENSE_SDL_BACKEND_ENABLED is undefined. See DualSenseSystemImpl_Mac.mm for
// the only place that actually constructs one of these today; Linux/Windows compile this same
// file (PAL_TRAIT_DUALSENSE_SDL_BACKEND is TRUE there too) but nothing yet calls it (those
// platforms are still Unimplemented at the DualSenseSystemImpl::Create() level).
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <AzCore/base.h>
#include <AzCore/std/containers/vector.h>

#include <SDL3/SDL.h>

namespace DualSense
{
    //! Lazily SDL_Init's SDL_INIT_JOYSTICK|SDL_INIT_SENSOR on first Activate() -- NEVER at
    //! static init or module load, per the task brief and the porting guide's "gate every call
    //! on SDL_WasInit" discipline (~/pong/docs/dualsense-porting-guide.md, "The SDL call
    //! surface"). This gem links SDL3 directly (3rdParty::SDL3), unlike the guide's dlsym-based
    //! reference (sdl_runtime.{h,cpp} in that repo) -- there is no symbol-resolution table here,
    //! only the lifecycle/WasInit/Lock discipline the guide describes is carried over.
    //!
    //! Exactly one instance is ever owned at a time (by DualSenseSystemImplMac's Sdl-backend
    //! branch), so this class needs no internal refcounting beyond the single m_active flag --
    //! Activate()/Deactivate() are each idempotent, which is all a single-owner needs.
    class DualSenseSdlRuntime
    {
    public:
        ~DualSenseSdlRuntime();

        //! Idempotent (returns true immediately if already active). Sets the HIDAPI hints (see
        //! .cpp for the exact hint names, verified against the fetched SDL3 3.4.12 headers) and
        //! then calls SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_SENSOR). Returns false (and logs a
        //! warning) if SDL_Init fails; the caller (DualSenseSystemImplMac) is expected to leave
        //! the sdl backend inert rather than crash in that case.
        bool Activate();

        //! Idempotent (no-op if not active). SDL_QuitSubSystem for exactly what Activate
        //! initialized, then SDL_Quit() -- a full Quit (not just QuitSubSystem) because this
        //! runtime is this gem's sole SDL owner (the swap architecture makes the gem the only
        //! caller of SDL joystick APIs while the sdl backend is selected -- see the "TRANSPORT
        //! DECISION" memory note / spec amendment -- so there is nothing else in-process that
        //! could still need SDL's other subsystems alive), and SDL_Quit's ref-counted-per-
        //! subsystem internals make this correct even for subsystems SDL_Init implicitly pulled
        //! in (e.g. SDL_INIT_EVENTS, which both JOYSTICK and SENSOR imply).
        void Deactivate();

        bool IsActive() const
        {
            return m_active;
        }

        //! BT-readiness addendum ("3b"). macOS only: true if the process has been granted Input
        //! Monitoring (TCC) access as of the last Activate() call -- see Activate()'s .cpp comment
        //! for why SDL's HIDAPI path needs this and the native GameController backend does not.
        //! Always true on non-Apple platforms (nothing gates HID access there the way macOS does).
        //! Callers (DualSenseSdlMonitor) use this to give a more actionable warning when
        //! enumeration comes back empty for a reason that isn't "no controller is plugged in".
        bool HasInputMonitoringAccess() const
        {
            return m_inputMonitoringAccessGranted;
        }

        //! Pumps joystick (and therefore gamepad-layer) state. Call once per
        //! DualSenseSystemImpl::Tick() from the main thread while active; no-op (SDL_WasInit-
        //! gated) if not active.
        void PumpEvents();

        //! Enumerates currently-connected PS5-type joysticks (SDL_GetGamepadTypeForID ==
        //! SDL_GAMEPAD_TYPE_PS5, the same DualSense-detection test the porting guide's transport
        //! table implies SDL can already do for us). Empty if not active. Pure query -- callers
        //! (DualSenseSdlMonitor) own the connect/disconnect diffing and slot bookkeeping.
        AZStd::vector<SDL_JoystickID> EnumeratePs5Joysticks() const;

        //! Logs (AZLOG_INFO) the transport for a joystick id via the porting guide's GUID-
        //! first-byte trick: 0x03 = USB, 0x05 = Bluetooth, anything else logged as unknown.
        //! Stateless/pure logging -- safe to call repeatedly (DualSenseSdlMonitor calls it once
        //! per newly-seen connection, not once per tick).
        static void LogTransport(SDL_JoystickID id);

    private:
        bool m_active = false;
        //! See HasInputMonitoringAccess(). Defaults true: platforms/builds that never run the
        //! __APPLE__ check in Activate() (non-mac, or a mac process that hasn't activated yet)
        //! should not report a false "denied" before the first real check ever runs.
        bool m_inputMonitoringAccessGranted = true;
    };
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
