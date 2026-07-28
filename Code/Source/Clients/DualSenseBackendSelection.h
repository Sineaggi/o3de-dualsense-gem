#pragma once

// Common (unguarded by DUALSENSE_SDL_BACKEND_ENABLED) glue between the dualsense_backend cvar
// (declared in DualSenseSystemComponent.cpp -- see that file for the AZ_CVAR itself) and the
// per-platform DualSenseSystemImpl that actually acts on it (Task 3: DualSenseSystemImpl_Mac.mm).
// Lives outside Source/Clients/Sdl/ (and is compiled unconditionally, on every platform) because
// the enum + cvar-read helper have no SDL dependency of their own: a platform with
// DUALSENSE_SDL_BACKEND_ENABLED undefined still needs to be able to read "the cvar says sdl" and
// react (by staying on native and logging once), it just never reaches any SDL call while doing
// so -- see DualSenseSystemImpl_Mac.mm's own #if-gated Setup/TeardownSdl.

#include <AzCore/std/string/string_view.h>
#include <AzCore/EBus/EBus.h>

namespace DualSense
{
    //! Which input backend the dualsense_backend cvar currently resolves to.
    enum class BackendSelection
    {
        Native, //!< Per-platform native backend (Mac: GameController.framework/CoreHaptics).
        Sdl     //!< SDL3 joystick-layer backend (Phase 3a+; cvar value "sdl").
    };

    //! Pure string -> BackendSelection mapping, case-insensitive. Any value other than "sdl"
    //! (including "native", empty, and unrecognized garbage) maps to Native -- this is the same
    //! fail-safe-to-today's-behavior default the cvar's own AZ_CVAR declaration documents.
    //! Extracted as a pure function (no AZ::IConsole dependency) specifically so it is unit
    //! testable without standing up a console context; see GetDualSenseBackendSelection below
    //! for the impure wrapper that actually reads the live cvar.
    BackendSelection ParseBackendSelection(AZStd::string_view value);

    //! Reads the live dualsense_backend cvar (via AZ::Interface<AZ::IConsole>) and returns
    //! ParseBackendSelection of its current value. Defaults to Native if no console is
    //! registered (e.g. very early activation order, or a headless/builder context), matching
    //! ParseBackendSelection's own unrecognized-value fallback.
    BackendSelection GetDualSenseBackendSelection();

    //! Broadcast whenever the dualsense_backend cvar's AZ_CVAR callback fires -- BarrierInput's
    //! cvar-callback-to-notification-bus pattern (see BarrierInputConnectionNotificationBus /
    //! OnBarrierConnectionCVarChanged in Gems/BarrierInput/Code/Source/BarrierInputSystemComponent.cpp),
    //! adapted here so DualSenseSystemImplMac doesn't need to poll the cvar every Tick() to
    //! notice a live `dualsense_backend sdl`/`dualsense_backend native` console command.
    //! Global (Single address), Multiple handler policy: harmless if more than one platform impl
    //! ever subscribes (only one is ever alive at a time in practice).
    class DualSenseBackendCVarNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        virtual ~DualSenseBackendCVarNotifications() = default;

        //! Fired after the cvar's new value has already been committed, so handlers can safely
        //! call GetDualSenseBackendSelection() immediately to read it.
        virtual void OnDualSenseBackendCVarChanged() {}
    };
    using DualSenseBackendCVarNotificationBus = AZ::EBus<DualSenseBackendCVarNotifications>;
} // namespace DualSense
