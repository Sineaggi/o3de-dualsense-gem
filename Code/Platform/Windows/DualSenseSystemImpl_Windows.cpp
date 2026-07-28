// Phase 3c: Windows enablement of DualSenseSystemImpl.
//
// UNTESTED ON WINDOWS as of this commit. Authored and reviewed entirely on macOS, with no access
// to a Windows toolchain, Windows SDK, or a DualSense-over-Windows hardware pass. The first real
// Windows build of this gem may need fixes -- see the Phase 3c report
// (.superpowers/sdd/phase-3c-windows-report.md) for the specific assumptions this file relies on
// that could not be verified here (SDL3's Windows HIDAPI/PS5 joystick path, AZLOG_* behavior
// under MSVC in this translation unit, and link-library completeness for 3rdParty::SDL3 on this
// platform -- see Code/Platform/Windows/platform_windows.cmake).
//
// Windows has no first-party DualSense-aware API -- unlike macOS's GameController.framework path,
// there is no OS-level backend this gem could call into here even in principle. The SDL3
// joystick-layer backend (Code/Source/Clients/Sdl/) is therefore the ONLY backend Windows can run,
// and this file wires it up as DualSenseSystemImpl::Create()'s Windows definition, replacing the
// Unimplemented stub (Code/Platform/Common/Unimplemented/DualSenseSystemImpl_Unimplemented.cpp)
// this platform used before Phase 3c. Linux is untouched by this change and still uses that stub
// (Linux enablement is a separate future task).
#include <Clients/DualSenseSystemImpl.h>
#include <Clients/DualSenseBackendSelection.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <Clients/Sdl/DualSenseSdlRuntime.h>
#include <Clients/Sdl/DualSenseSdlMonitor.h>

namespace DualSense
{
    //! Windows' DualSenseSystemImpl: owns and drives the SDL3 joystick-layer backend -- the only
    //! backend this platform has (see file header comment). Structurally this is the "Sdl half"
    //! of DualSenseSystemImpl_Mac.mm's DualSenseSystemImplMac, with the Native branch
    //! (GameController.framework/CoreHaptics -- Objective-C, Mac-only) removed entirely: there is
    //! nothing on Windows to mirror that branch with.
    //!
    //! DESIGN DECISION -- dualsense_backend cvar semantics on Windows (also documented in the
    //! Phase 3c report): the cvar's two values, "native" (the default) and "sdl", both resolve to
    //! the SAME stack on this platform, because Windows only has the one stack to offer. A
    //! Windows user who never touches the cvar (i.e. stays on the "native" default, exactly as a
    //! macOS user typically would) must still get a working gem -- silently staying inert under
    //! the default would reproduce exactly the "gem does nothing on this platform" problem this
    //! task exists to fix. Both cvar values are therefore treated as "bring up SDL", with an
    //! AZLOG_INFO logged once at construction, and once per subsequent cvar-value change, making
    //! clear that the cvar is accepted but informational-only here -- it is NOT a real backend
    //! switch the way it is on Mac.
    class DualSenseSystemImplWindows
        : public DualSenseSystemImpl
        , public DualSenseBackendCVarNotificationBus::Handler
    {
    public:
        explicit DualSenseSystemImplWindows(DualSenseSystemComponent& owner)
            : DualSenseSystemImpl(owner)
        {
            DualSenseBackendCVarNotificationBus::Handler::BusConnect();
            AZLOG_INFO(
                "DualSense (Windows): this platform has exactly one backend (SDL3); the "
                "dualsense_backend cvar is accepted but informational only here -- both 'native' "
                "(the default) and 'sdl' bring up the same SDL3 stack.");
            ApplyBackendSelection(GetDualSenseBackendSelection());
        }

        ~DualSenseSystemImplWindows() override
        {
            DualSenseBackendCVarNotificationBus::Handler::BusDisconnect();
            TeardownSdl();
        }

        void Tick() override
        {
            if (m_sdlMonitor)
            {
                m_sdlMonitor->Tick();
            }
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        // DualSenseBackendCVarNotificationBus::Handler
        void OnDualSenseBackendCVarChanged() override
        {
            const BackendSelection selection = GetDualSenseBackendSelection();
            if (selection != m_lastLoggedSelection)
            {
                AZLOG_INFO(
                    "DualSense (Windows): dualsense_backend changed to '%s' -- no effect, this "
                    "platform only has the SDL3 backend (see the informational note logged at "
                    "startup).",
                    (selection == BackendSelection::Sdl) ? "sdl" : "native");
                m_lastLoggedSelection = selection;
            }
            // Retry semantics: if a prior activation attempt failed (m_activeStack is still
            // None), re-issuing the cvar -- either value -- is this platform's only way to
            // retry, since there is no "switch to native and back" escape hatch here the way
            // Mac's ApplyBackendSelection has (Mac can bounce through the Native stack; Windows
            // has no second stack to bounce through). See ApplyBackendSelection below.
            ApplyBackendSelection(selection);
        }

    private:
        enum class Stack
        {
            None,
            Sdl
        };

        //! Brings up the SDL stack if it is not already up. `selection`'s value is not consulted
        //! for WHICH stack to bring up (there is only one -- see the class comment); it exists
        //! only so callers read clearly at call sites. No-op if the SDL stack is already active:
        //! unlike Mac's ApplyBackendSelection, there is never a "tear down the old stack, bring up
        //! a different one" transition here, because the target stack never changes.
        void ApplyBackendSelection([[maybe_unused]] BackendSelection selection)
        {
            if (m_activeStack == Stack::Sdl)
            {
                return;
            }

            if (SetupSdl())
            {
                m_activeStack = Stack::Sdl;
            }
            else
            {
                AZLOG_WARN(
                    "DualSense (Windows): SDL3 backend activation failed; DualSense support stays "
                    "inactive until re-attempted (re-issue 'dualsense_backend native' or "
                    "'dualsense_backend sdl' at the console to retry -- either value retries the "
                    "same SDL3 stack on this platform).");
                // m_activeStack stays Stack::None -- deliberately, so a later retry (either cvar
                // value) is not short-circuited by the early-return above. Same retry-ability
                // reasoning as DualSenseSystemImplMac::ApplyBackendSelection's Sdl-failure branch.
            }
        }

        bool SetupSdl()
        {
            if (!m_sdlRuntime.Activate())
            {
                AZLOG_WARN(
                    "DualSense (Windows): SDL_Init failed; DualSense support stays inactive until "
                    "the backend is reselected");
                return false;
            }
            m_sdlMonitor = AZStd::make_unique<DualSenseSdlMonitor>(m_sdlRuntime);
            return true;
        }

        //! Tearing down the monitor first restores every slot it still owns (see
        //! DualSenseSdlMonitor's destructor) before the runtime deactivates (SDL_Quit) -- same
        //! ordering rationale as DualSenseSystemImplMac::TeardownSdl: restoring a slot swaps a
        //! *live* Implementation back to the platform default, which must happen while SDL is
        //! still active enough for the outgoing InputDeviceGamepadDualSenseSdl's destructor to do
        //! its best-effort trigger-clear/haptics-stop.
        void TeardownSdl()
        {
            m_sdlMonitor.reset();
            m_sdlRuntime.Deactivate();
        }

        Stack m_activeStack = Stack::None;
        DualSenseSdlRuntime m_sdlRuntime;
        AZStd::unique_ptr<DualSenseSdlMonitor> m_sdlMonitor;
        //! Last BackendSelection value logged by OnDualSenseBackendCVarChanged, so repeated
        //! console commands with the same value don't spam the log every time. Initialized to
        //! Native to match GetDualSenseBackendSelection()'s own default-cvar-value fallback, so a
        //! no-op "native"->"native" re-issue right after construction doesn't immediately re-log.
        BackendSelection m_lastLoggedSelection = BackendSelection::Native;
    };

    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent& owner)
    {
        return AZStd::make_unique<DualSenseSystemImplWindows>(owner);
    }
} // namespace DualSense

#else // !defined(DUALSENSE_SDL_BACKEND_ENABLED)

// This build was not compiled with the SDL3 backend linked in (PAL_TRAIT_DUALSENSE_SDL_BACKEND
// was FALSE for this platform configuration). Windows has no other backend to fall back to (see
// the file header comment), so there is nothing this Create() can stand up -- warn loudly and
// stay passive, the same failure mode as the Common/Unimplemented stub this file replaces, but
// with a Windows-specific explanation of *why* instead of a generic "not implemented" silence.
namespace DualSense
{
    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent&)
    {
        AZLOG_WARN(
            "DualSense: Windows build was not compiled with DUALSENSE_SDL_BACKEND_ENABLED "
            "(PAL_TRAIT_DUALSENSE_SDL_BACKEND was FALSE for this platform) -- Windows has no "
            "native DualSense backend, only SDL3, so this build has no way to support DualSense "
            "hardware. Gem stays passive.");
        return nullptr;
    }
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
