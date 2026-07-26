#include <Clients/DualSenseSystemImpl.h>
#include <Clients/DualSenseSystemComponent.h>
#include <Clients/DualSenseSlotTracker.h>
#include "DualSenseMacGamepadImplFactory.h"

#include <AzCore/Console/ILogger.h>

#import <GameController/GameController.h>

#include <memory>

namespace DualSense
{
    class DualSenseSystemImplMac
        : public DualSenseSystemImpl
    {
    public:
        explicit DualSenseSystemImplMac(DualSenseSystemComponent& owner)
            : DualSenseSystemImpl(owner)
        {
            if (@available(macOS 11.3, *))
            {
                // Both blocks below run on the main queue and copy-capture m_alive (a
                // std::shared_ptr<bool>, a C++ object, which MRC/non-ARC blocks copy-capture
                // correctly by value/refcount just like ARC does). A block that is already
                // enqueued on the run loop can still execute after this object's destructor
                // has run and freed `this` -- removeObserver only prevents *future*
                // notifications from being posted, it does not cancel a block already queued
                // for this pass of the run loop. The shared_ptr<bool> flag lets a
                // late-executing block detect that `this` is gone and bail out instead of
                // touching freed memory.
                NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
                std::shared_ptr<bool> aliveForConnect = m_alive;
                m_connectObserver = [center addObserverForName:GCControllerDidConnectNotification
                                                        object:nil
                                                         queue:[NSOperationQueue mainQueue]
                                                    usingBlock:^(NSNotification* note) {
                                                        if (!*aliveForConnect) { return; }
                                                        this->OnControllerConnected((GCController*)note.object);
                                                    }];
                std::shared_ptr<bool> aliveForDisconnect = m_alive;
                m_disconnectObserver = [center addObserverForName:GCControllerDidDisconnectNotification
                                                           object:nil
                                                            queue:[NSOperationQueue mainQueue]
                                                       usingBlock:^(NSNotification* note) {
                                                           if (!*aliveForDisconnect) { return; }
                                                           this->OnControllerDisconnected((GCController*)note.object);
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

        ~DualSenseSystemImplMac() override
        {
            // Flip the alive flag first, before anything else can run: any block already
            // enqueued on the main queue (see constructor comment) will see this false and
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
        }

    private:
        void OnControllerConnected(GCController* controller) API_AVAILABLE(macos(11.3))
        {
            if (![controller.extendedGamepad isKindOfClass:[GCDualSenseGamepad class]])
            {
                return; // not a DualSense; leave it to the stock engine backend
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
    };

    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent& owner)
    {
        return AZStd::make_unique<DualSenseSystemImplMac>(owner);
    }
} // namespace DualSense
