#include <Clients/DualSenseSystemImpl.h>
#include <Clients/DualSenseSystemComponent.h>
#include <Clients/DualSenseSlotTracker.h>
#include <Clients/DualSenseBackendSelection.h>
#include "DualSenseMacGamepadImplFactory.h"

#if defined(DUALSENSE_SDL_BACKEND_ENABLED)
#include <Clients/Sdl/DualSenseSdlRuntime.h>
#include <Clients/Sdl/DualSenseSdlMonitor.h>
#endif

#include <AzCore/Console/ILogger.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#import <GameController/GameController.h>

#include <dispatch/dispatch.h>

#include <memory>

namespace DualSense
{
    class DualSenseSystemImplMac
        : public DualSenseSystemImpl
        , public DualSenseBackendCVarNotificationBus::Handler
    {
    public:
        explicit DualSenseSystemImplMac(DualSenseSystemComponent& owner)
            : DualSenseSystemImpl(owner)
        {
            DualSenseBackendCVarNotificationBus::Handler::BusConnect();
            // Consult the cvar once at activation (BarrierInput cvar-callback pattern -- see
            // OnDualSenseBackendCVarChanged() below, which the bus connect above wires to the
            // AZ_CVAR callback in DualSenseSystemComponent.cpp) and bring up whichever stack it
            // names. m_activeStack starts at None, so this always takes the "bring up" branch of
            // ApplyBackendSelection below, never a teardown of nothing.
            ApplyBackendSelection(GetDualSenseBackendSelection());
        }

        ~DualSenseSystemImplMac() override
        {
            DualSenseBackendCVarNotificationBus::Handler::BusDisconnect();
            TeardownNative();
            TeardownSdl();
        }

        void Tick() override
        {
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)
            if (m_sdlMonitor)
            {
                m_sdlMonitor->Tick();
            }
#endif
            // The native path is entirely GameController-notification-driven (see the
            // constructor/destructor below) -- nothing to pump here for it.
        }

        ////////////////////////////////////////////////////////////////////////////////////////
        // DualSenseBackendCVarNotificationBus::Handler
        void OnDualSenseBackendCVarChanged() override
        {
            ApplyBackendSelection(GetDualSenseBackendSelection());
        }

    private:
        enum class Stack
        {
            None,
            Native,
            Sdl
        };

        //! Switches the live stack to match `selection`, restoring every slot the CURRENTLY
        //! active stack owns before bringing up the new one (per the task brief: "switching:
        //! restore all slots, tear down old monitor/stack, bring up new"). No-op if `selection`
        //! already matches what's live (e.g. the console echoing the same value back, or the
        //! constructor's initial call landing on the cvar's own default). Also called once from
        //! the constructor with m_activeStack still at None, which always takes the "bring up"
        //! branch below with nothing to tear down first.
        void ApplyBackendSelection(BackendSelection selection)
        {
            const Stack target = (selection == BackendSelection::Sdl) ? Stack::Sdl : Stack::Native;
            if (target == m_activeStack)
            {
                return;
            }

            switch (m_activeStack)
            {
            case Stack::Native:
                TeardownNative();
                break;
            case Stack::Sdl:
                TeardownSdl();
                break;
            case Stack::None:
                break;
            }
            m_activeStack = Stack::None;

            switch (target)
            {
            case Stack::Native:
                SetupNative();
                break;
            case Stack::Sdl:
                SetupSdl();
                break;
            case Stack::None:
                break;
            }
            m_activeStack = target;
        }

        //! Brings up the SDL3 joystick-layer backend: activates DualSenseSdlRuntime (lazy
        //! SDL_Init happens here, and ONLY here -- see DualSenseSdlRuntime.h) and constructs the
        //! monitor. On DUALSENSE_SDL_BACKEND_ENABLED-undefined platforms (SDL3 not linked at
        //! all), this is a hard compile-time gate: `dualsense_backend sdl` logs a warning and the
        //! gem stays passive, with zero SDL calls possible even in principle (there is no SDL3
        //! symbol reachable from this translation unit to call).
        void SetupSdl()
        {
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)
            if (!m_sdlRuntime.Activate())
            {
                AZLOG_WARN("DualSense: dualsense_backend=sdl selected but SDL_Init failed; "
                           "DualSense support stays inactive until the backend is reselected");
                return;
            }
            m_sdlMonitor = AZStd::make_unique<DualSenseSdlMonitor>(m_sdlRuntime);
#else
            AZLOG_WARN("DualSense: dualsense_backend=sdl selected, but this build was not compiled with "
                       "DUALSENSE_SDL_BACKEND_ENABLED (PAL_TRAIT_DUALSENSE_SDL_BACKEND was FALSE for this "
                       "platform) -- SDL3 is not linked, so no SDL call can be made. DualSense support stays "
                       "inactive under this backend selection.");
#endif
        }

        //! Tears down the SDL stack, if any: destroying the monitor first restores every slot it
        //! still owns (see DualSenseSdlMonitor's destructor), THEN the runtime is deactivated
        //! (SDL_Quit) -- that ordering matters, restoring a slot swaps a *live* Implementation
        //! back to the platform default, which must happen while SDL is still active enough for
        //! the outgoing InputDeviceGamepadDualSenseSdl's destructor to do its best-effort
        //! trigger-clear/haptics-stop.
        void TeardownSdl()
        {
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)
            m_sdlMonitor.reset();
            m_sdlRuntime.Deactivate();
#endif
        }

        void SetupNative()
        {
            if (@available(macOS 11.3, *))
            {
                // Both blocks below register with queue:[NSOperationQueue mainQueue], but that
                // queue: parameter is NOT trusted to guarantee main-thread delivery. Hardware
                // forensics (a real DualSense unplugged mid-rumble, macOS 26) proved
                // GCControllerDidDisconnectNotification can be delivered out *synchronously* on
                // GameController's internal GCDeviceSession thread -- macOS ran it via a
                // CFNotificationCenter callout with no queue hop at all. Our entire restore path
                // (slot tracker, EBus SetCustomImplementation, device pimpl destruction) is not
                // safe to run off the main thread, so each block below does nothing but retain
                // the controller and dispatch_async to dispatch_get_main_queue() -- the
                // dispatch_async hop, not the queue: parameter, is what actually guarantees
                // main-thread execution. The alive-check and the real work happen inside the
                // dispatched block, evaluated *after* the hop: checking `*alive` before the hop
                // would be useless, since teardown can happen during the async gap between the
                // notification firing and the dispatched block running.
                //
                // A block that is already queued (on the run loop, or via dispatch_async) can
                // still execute after this object's destructor has run and freed `this` --
                // removeObserver only prevents *future* notifications from being posted, it does
                // not cancel work already queued. The shared_ptr<bool> flag lets a
                // late-executing block detect that `this` is gone and bail out instead of
                // touching freed memory.
                NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
                std::shared_ptr<bool> aliveForConnect = m_alive;
                m_connectObserver = [center addObserverForName:GCControllerDidConnectNotification
                                                        object:nil
                                                         queue:[NSOperationQueue mainQueue]
                                                    usingBlock:^(NSNotification* note) {
                                                        GCController* controller = (GCController*)note.object;
                                                        [controller retain]; // keep alive across the async hop (MRC)
                                                        dispatch_async(dispatch_get_main_queue(), ^{
                                                            if (*aliveForConnect) // alive-check evaluated ON MAIN, after the hop
                                                            {
                                                                this->OnControllerConnected(controller);
                                                            }
                                                            [controller release];
                                                        });
                                                    }];
                std::shared_ptr<bool> aliveForDisconnect = m_alive;
                m_disconnectObserver = [center addObserverForName:GCControllerDidDisconnectNotification
                                                           object:nil
                                                            queue:[NSOperationQueue mainQueue]
                                                       usingBlock:^(NSNotification* note) {
                                                           GCController* controller = (GCController*)note.object;
                                                           [controller retain]; // keep alive across the async hop (MRC)
                                                           dispatch_async(dispatch_get_main_queue(), ^{
                                                               if (*aliveForDisconnect) // alive-check evaluated ON MAIN, after the hop
                                                               {
                                                                   this->OnControllerDisconnected(controller);
                                                               }
                                                               [controller release];
                                                           });
                                                       }];
                for (GCController* controller in GCController.controllers)
                {
                    OnControllerConnected(controller);
                }
            }
            else
            {
                AZLOG_INFO("DualSense: macOS < 11.3, DualSense support inactive (stock engine behavior)");
            }
        }

        //! Formerly this class's destructor body (pre-Task-3, when the native GameController
        //! path was the only stack that ever existed) -- now invoked from ApplyBackendSelection
        //! and from ~DualSenseSystemImplMac whenever the native stack is the one currently live.
        void TeardownNative()
        {
            // Flip the alive flag first, before anything else can run: any block already
            // enqueued on the main queue (see SetupNative's comment) will see this false and
            // return without touching `this`.
            *m_alive = false;
            if (@available(macOS 11.3, *))
            {
                // Restore every slot we own before tearing down.
                for (GCController* controller in GCController.controllers)
                {
                    OnControllerDisconnected(controller);
                }
                NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
                if (m_connectObserver) { [center removeObserver:m_connectObserver]; }
                if (m_disconnectObserver) { [center removeObserver:m_disconnectObserver]; }
            }
            // Reset so a subsequent SetupNative (a switch back from sdl) starts from the same
            // clean state the constructor did -- a fresh alive-flag and no stale observer ids.
            m_alive = std::make_shared<bool>(true);
            m_connectObserver = nil;
            m_disconnectObserver = nil;
        }

        void OnControllerConnected(GCController* controller) API_AVAILABLE(macos(11.3))
        {
            if (![controller.extendedGamepad isKindOfClass:[GCDualSenseGamepad class]])
            {
                return; // not a DualSense; leave it to the stock engine backend
            }
            // Guard against duplicate connect notifications for a controller we already track.
            // The engine's SetCustomImplementation constructs the new impl BEFORE destroying the
            // old one; on a same-slot re-swap the old impl's BusDisconnect would null the
            // single-handler bus slot the new impl just claimed, silently killing trigger effects.
            // This guard makes that re-swap unreachable from duplicate notifications.
            if (m_slotTracker.SlotOf((__bridge const void*)controller) != DualSenseSlotTracker::InvalidSlot)
            {
                AZLOG_DEBUG("DualSense: duplicate connect notification for an already-tracked controller, ignoring");
                return;
            }
            const AZ::u32 preferred = (controller.playerIndex != GCControllerPlayerIndexUnset)
                ? static_cast<AZ::u32>(controller.playerIndex) : 0;
            const AZ::u32 slot = m_slotTracker.Assign((__bridge const void*)controller, preferred);
            if (slot == DualSenseSlotTracker::InvalidSlot)
            {
                AZLOG_WARN("DualSense: controller detected but all 4 gamepad slots occupied");
                return;
            }
            // Claim the slot on the GCController itself before swapping. The engine's stock
            // Mac gamepad backend enumerates GCController.controllers and skips any
            // controller whose playerIndex != GCControllerPlayerIndexUnset when deciding
            // which physical pad belongs to which of its own slots. If we swap this slot to
            // our implementation without setting playerIndex, the stock impl running on a
            // *different* slot still sees this controller as unclaimed and will happily bind
            // it too -- one physical DualSense would then drive two engine gamepad slots
            // simultaneously. GCControllerPlayerIndex1 == 0 in the SDK, matching our
            // 0-based slot indices, so the cast below needs no offset.
            controller.playerIndex = static_cast<GCControllerPlayerIndex>(slot);
            AZLOG_INFO("DualSense: controller detected, taking over gamepad slot %u", slot);
            m_factory.m_pendingController = (__bridge void*)controller;
            DualSenseSystemComponent::SwapSlotToFactory(slot, &m_factory);
            m_factory.m_pendingController = nullptr;
        }

        void OnControllerDisconnected(GCController* controller) API_AVAILABLE(macos(11.3))
        {
            const AZ::u32 slot = m_slotTracker.Release((__bridge const void*)controller);
            if (slot == DualSenseSlotTracker::InvalidSlot)
            {
                return; // wasn't ours
            }
            // Release our claim on the controller's playerIndex before restoring the slot to
            // the platform-default implementation, so the freshly-installed stock impl is
            // free to claim this (still-attached, e.g. shutdown-time restore) controller
            // instead of treating it as already spoken for. Harmless on a genuine physical
            // disconnect: the GCController object is going away regardless.
            controller.playerIndex = GCControllerPlayerIndexUnset;
            AZLOG_INFO("DualSense: controller left slot %u, restoring platform default", slot);
            DualSenseSystemComponent::RestoreSlotToPlatformDefault(slot);
        }

        DualSenseMacGamepadImplFactory m_factory;
        DualSenseSlotTracker m_slotTracker;
        id m_connectObserver = nil;
        id m_disconnectObserver = nil;
        // Shared with the notification-observer blocks below; see the constructor/destructor
        // comments for why this is needed (an already-enqueued block can outlive `this`).
        std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

        // Backend-selection state (Phase 3a Task 3). m_activeStack starts at None so the
        // constructor's ApplyBackendSelection call always takes the "bring up" branch.
        Stack m_activeStack = Stack::None;
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)
        DualSenseSdlRuntime m_sdlRuntime;
        AZStd::unique_ptr<DualSenseSdlMonitor> m_sdlMonitor;
#endif
    };

    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent& owner)
    {
        return AZStd::make_unique<DualSenseSystemImplMac>(owner);
    }
} // namespace DualSense
