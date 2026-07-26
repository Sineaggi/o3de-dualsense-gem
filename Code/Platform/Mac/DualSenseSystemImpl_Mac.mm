#include <Clients/DualSenseSystemImpl.h>
#include <Clients/DualSenseSystemComponent.h>
#include <Clients/DualSenseSlotTracker.h>
#include "DualSenseMacGamepadImplFactory.h"

#include <AzCore/Console/ILogger.h>

#import <GameController/GameController.h>

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
                NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
                m_connectObserver = [center addObserverForName:GCControllerDidConnectNotification
                                                        object:nil
                                                         queue:[NSOperationQueue mainQueue]
                                                    usingBlock:^(NSNotification* note) {
                                                        this->OnControllerConnected((GCController*)note.object);
                                                    }];
                m_disconnectObserver = [center addObserverForName:GCControllerDidDisconnectNotification
                                                           object:nil
                                                            queue:[NSOperationQueue mainQueue]
                                                       usingBlock:^(NSNotification* note) {
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
            AZLOG_INFO("DualSense: controller left slot %u, restoring platform default", slot);
            DualSenseSystemComponent::RestoreSlotToPlatformDefault(slot);
        }

        DualSenseMacGamepadImplFactory m_factory;
        DualSenseSlotTracker m_slotTracker;
        id m_connectObserver = nil;
        id m_disconnectObserver = nil;
    };

    AZStd::unique_ptr<DualSenseSystemImpl> DualSenseSystemImpl::Create(DualSenseSystemComponent& owner)
    {
        return AZStd::make_unique<DualSenseSystemImplMac>(owner);
    }
} // namespace DualSense
