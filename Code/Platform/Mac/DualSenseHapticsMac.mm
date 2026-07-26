#include "DualSenseHapticsMac.h"

#include <AzCore/Console/ILogger.h>
#include <AzCore/Math/MathUtils.h>

#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

namespace DualSense
{
    namespace
    {
        // This translation unit is compiled without ARC (matching the rest of this gem's
        // Mac sources, see InputDeviceGamepadDualSenseMac.mm), so __bridge_retained /
        // __bridge_transfer are unavailable (they require ARC to insert the retain/release).
        // CFRetain/CFRelease work directly on Objective-C objects because their retain
        // counts are toll-free bridged with CoreFoundation on Apple platforms, so plain
        // `(__bridge T*)` casts plus explicit CFRetain/CFRelease reproduce the same
        // ownership-transfer contract without ARC.

        void* CreateStartedEngine(GCController* controller, GCHapticsLocality locality) API_AVAILABLE(macos(11.3))
        {
            CHHapticEngine* engine = [controller.haptics createEngineWithLocality:locality];
            if (!engine)
            {
                return nullptr;
            }
            NSError* error = nil;
            if (![engine startAndReturnError:&error])
            {
                AZLOG_WARN("DualSense: haptic engine start failed: %s",
                           error.localizedDescription.UTF8String);
                return nullptr;
            }
            // __block under MRC is not retained by the block copy — breaks the
            // engine<->resetHandler retain cycle; engine outlives its handler.
            __block CHHapticEngine* weakEngine = engine;
            engine.resetHandler = ^{ [weakEngine startAndReturnError:nil]; };

            // `engine` is autoreleased (createEngineWithLocality: does not return an
            // owned reference). Take ownership for the lifetime of the opaque pointer.
            void* opaqueEngine = (__bridge void*)engine;
            CFRetain(opaqueEngine);
            return opaqueEngine;
        }

        // Replaces *playerSlot with a new infinite continuous player at `intensity`,
        // or stops/clears it when intensity is ~0.
        void UpdateSide(void* engineOpaque, void** playerSlot, float intensity) API_AVAILABLE(macos(11.3))
        {
            if (!engineOpaque)
            {
                return;
            }
            CHHapticEngine* engine = (__bridge CHHapticEngine*)engineOpaque;

            if (*playerSlot)
            {
                id<CHHapticPatternPlayer> old = (__bridge id<CHHapticPatternPlayer>)*playerSlot;
                [old stopAtTime:0 error:nil];
                CFRelease(*playerSlot); // balances the CFRetain taken when this player was stored
                *playerSlot = nullptr;
            }
            if (intensity <= 0.001f)
            {
                return;
            }

            NSError* error = nil;
            CHHapticEventParameter* intensityParam =
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                              value:intensity];
            CHHapticEvent* event =
                [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                              parameters:@[ intensityParam ]
                                            relativeTime:0
                                                duration:GCHapticDurationInfinite];
            // `event` retains what it needs from intensityParam internally (per the
            // CHHapticEvent contract); our +1 from alloc/init is no longer needed.
            [intensityParam release];

            CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ]
                                                               parameterCurves:@[]
                                                                         error:&error];
            // Likewise, `pattern` does not retain the array we pass it, only what it
            // needs from the elements, so our +1 on `event` is no longer needed.
            [event release];
            if (!pattern)
            {
                AZLOG_WARN("DualSense: haptic pattern creation failed: %s",
                           error.localizedDescription.UTF8String);
                return;
            }
            id<CHHapticPatternPlayer> player = [engine createPlayerWithPattern:pattern error:&error];
            // `pattern` is no longer needed once the player has been created from it.
            [pattern release];
            if (!player || ![player startAtTime:0 error:&error])
            {
                AZLOG_WARN("DualSense: haptic player start failed: %s",
                           error ? error.localizedDescription.UTF8String : "unknown");
                return;
            }

            // `player` is autoreleased (createPlayerWithPattern:error: does not return an
            // owned reference). Take ownership for the lifetime of the opaque pointer.
            void* opaquePlayer = (__bridge void*)player;
            CFRetain(opaquePlayer);
            *playerSlot = opaquePlayer;
        }
    } // namespace

    DualSenseHapticsMac::DualSenseHapticsMac(void* gcController)
    {
        if (@available(macOS 11.3, *))
        {
            GCController* controller = (__bridge GCController*)gcController;
            m_leftEngine = CreateStartedEngine(controller, GCHapticsLocalityLeftHandle);
            m_rightEngine = CreateStartedEngine(controller, GCHapticsLocalityRightHandle);
        }
    }

    DualSenseHapticsMac::~DualSenseHapticsMac()
    {
        Stop();
        if (@available(macOS 11.3, *))
        {
            for (void** engineSlot : { &m_leftEngine, &m_rightEngine })
            {
                if (*engineSlot)
                {
                    CHHapticEngine* engine = (__bridge CHHapticEngine*)*engineSlot;
                    [engine stopWithCompletionHandler:nil];
                    // resetHandler is declared nonnull by the SDK (NS_ASSUME_NONNULL region
                    // in CHHapticEngine.h), so it can't be set to nil under -Werror -Wnonnull;
                    // a no-op block achieves the same goal (drop the reference to `weakEngine`
                    // so a post-release reset can never touch a dangling pointer).
                    engine.resetHandler = ^{};
                    CFRelease(*engineSlot); // balances the CFRetain in CreateStartedEngine
                    *engineSlot = nullptr;
                }
            }
        }
    }

    void DualSenseHapticsMac::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        if (@available(macOS 11.3, *))
        {
            UpdateSide(m_leftEngine, &m_leftPlayer, AZ::GetClamp(leftMotorSpeedNormalized, 0.0f, 1.0f));
            UpdateSide(m_rightEngine, &m_rightPlayer, AZ::GetClamp(rightMotorSpeedNormalized, 0.0f, 1.0f));
        }
    }

    void DualSenseHapticsMac::Stop()
    {
        SetVibration(0.0f, 0.0f);
    }
} // namespace DualSense
