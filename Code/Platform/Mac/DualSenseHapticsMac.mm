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
            BOOL started = NO;
            @try
            {
                // Symmetry with UpdateSide/the destructor below: a dead engine (e.g. the
                // controller disconnecting during handshake) can make CoreHaptics throw an
                // NSException here instead of only populating `error`.
                started = [engine startAndReturnError:&error];
            }
            @catch (NSException* exception)
            {
                AZLOG_DEBUG("DualSense: haptic engine startAndReturnError: threw, ignoring: %s",
                            exception.reason ? exception.reason.UTF8String : "unknown");
                started = NO;
            }
            if (!started)
            {
                AZLOG_WARN("DualSense: haptic engine start failed: %s",
                           error ? error.localizedDescription.UTF8String : "unknown");
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
                @try
                {
                    // A dead engine (the controller unplugged mid-rumble) makes
                    // -[PatternPlayer stopAtTime:error:] throw an NSException instead of only
                    // returning an NSError; uncaught, it unwinds through this C++ frame into
                    // std::terminate. This is the exact crash from hardware forensics.
                    [old stopAtTime:0 error:nil];
                }
                @catch (NSException* exception)
                {
                    AZLOG_DEBUG("DualSense: stopAtTime: threw on a dead haptic engine, ignoring: %s",
                                exception.reason ? exception.reason.UTF8String : "unknown");
                }
                // `old` is a __bridge (non-owning) reference into *playerSlot; the CFRelease
                // below is the only owning release for this player and must run whether or not
                // stop threw, so it sits after the @try/@catch, unconditional.
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
            id<CHHapticPatternPlayer> player = nil;
            BOOL playerStarted = NO;
            @try
            {
                // A dead engine (the controller unplugged) can make
                // createPlayerWithPattern:error: and/or startAtTime:error: throw an
                // NSException instead of only populating `error`. `player` returned here is
                // autoreleased (not an owned reference, see the CFRetain comment below), so a
                // throw before or after it's assigned leaves nothing extra to release: on
                // catch this is simply treated as a failed start, and the player is never
                // stored.
                player = [engine createPlayerWithPattern:pattern error:&error];
                if (player)
                {
                    playerStarted = [player startAtTime:0 error:&error];
                }
            }
            @catch (NSException* exception)
            {
                AZLOG_DEBUG("DualSense: haptic player create/start threw on a dead engine, ignoring: %s",
                            exception.reason ? exception.reason.UTF8String : "unknown");
                player = nil;
                playerStarted = NO;
            }
            // `pattern` is only referenced inside the @try body (createPlayerWithPattern: does
            // not retain it beyond the call); release our +1 unconditionally now that the call
            // has either returned normally or been caught, so it happens on every path exactly
            // once.
            [pattern release];
            if (!player || !playerStarted)
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

        // One-shot counterpart to UpdateSide: replaces *playerSlot with a new transient
        // (single "kick", non-looping) player built from `intensity` + `sharpness`, or
        // clears it when intensity is ~0. Mirrors UpdateSide's rolling-slot MRC idiom and
        // dead-engine hardening exactly, but builds a CHHapticEventTypeHapticTransient event
        // (with Intensity + Sharpness parameters) instead of UpdateSide's
        // CHHapticEventTypeHapticContinuous (infinite rumble) event.
        void PlayTransientOnSide(void* engineOpaque, void** playerSlot, float intensity, float sharpness) API_AVAILABLE(macos(11.3))
        {
            if (!engineOpaque)
            {
                return;
            }
            CHHapticEngine* engine = (__bridge CHHapticEngine*)engineOpaque;

            if (*playerSlot)
            {
                id<CHHapticPatternPlayer> old = (__bridge id<CHHapticPatternPlayer>)*playerSlot;
                @try
                {
                    // Same dead-engine hardening as UpdateSide: a controller that has gone
                    // away can make stopAtTime:error: throw an NSException instead of only
                    // returning an NSError.
                    [old stopAtTime:0 error:nil];
                }
                @catch (NSException* exception)
                {
                    AZLOG_DEBUG("DualSense: transient stopAtTime: threw on a dead haptic engine, ignoring: %s",
                                exception.reason ? exception.reason.UTF8String : "unknown");
                }
                // `old` is a __bridge (non-owning) reference into *playerSlot; the CFRelease
                // below is the only owning release for this player and must run whether or not
                // stop threw, so it sits after the @try/@catch, unconditional.
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
            CHHapticEventParameter* sharpnessParam =
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness
                                                              value:sharpness];
            CHHapticEvent* event =
                [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticTransient
                                              parameters:@[ intensityParam, sharpnessParam ]
                                            relativeTime:0];
            // `event` retains what it needs from the parameters internally (per the
            // CHHapticEvent contract); our +1s from alloc/init are no longer needed.
            [intensityParam release];
            [sharpnessParam release];

            CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ]
                                                               parameterCurves:@[]
                                                                         error:&error];
            // Likewise, `pattern` does not retain the array we pass it, only what it
            // needs from the elements, so our +1 on `event` is no longer needed.
            [event release];
            if (!pattern)
            {
                AZLOG_WARN("DualSense: transient haptic pattern creation failed: %s",
                           error.localizedDescription.UTF8String);
                return;
            }
            id<CHHapticPatternPlayer> player = nil;
            BOOL playerStarted = NO;
            @try
            {
                // Same autoreleased-return reasoning as UpdateSide: `player` is not an
                // owned reference here, so a throw before or after it's assigned leaves
                // nothing extra to release; on catch this is simply a failed start, and the
                // player is never stored.
                player = [engine createPlayerWithPattern:pattern error:&error];
                if (player)
                {
                    playerStarted = [player startAtTime:0 error:&error];
                }
            }
            @catch (NSException* exception)
            {
                AZLOG_DEBUG("DualSense: transient haptic player create/start threw on a dead engine, ignoring: %s",
                            exception.reason ? exception.reason.UTF8String : "unknown");
                player = nil;
                playerStarted = NO;
            }
            // `pattern` is only referenced inside the @try body (createPlayerWithPattern: does
            // not retain it beyond the call); release our +1 unconditionally now that the call
            // has either returned normally or been caught, so it happens on every path exactly
            // once.
            [pattern release];
            if (!player || !playerStarted)
            {
                AZLOG_WARN("DualSense: transient haptic player start failed: %s",
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
                    // A dead engine (the controller already gone) can throw from
                    // stopWithCompletionHandler: or from the resetHandler property setter
                    // instead of failing silently. An exception must NEVER escape a
                    // destructor -- that is exactly what aborted the Editor in the field (see
                    // the stopAtTime: crash forensics above) -- so every remaining engine call
                    // here is guarded. Each call gets its OWN @try/@catch (rather than sharing
                    // one) so a throw from one can never skip the other's mitigation --
                    // resetHandler neutralization in particular must still run even when stop
                    // throws, since a thrown stop is the realistic case this exists for.
                    @try
                    {
                        // resetHandler is declared nonnull by the SDK (NS_ASSUME_NONNULL region
                        // in CHHapticEngine.h), so it can't be set to nil under -Werror -Wnonnull;
                        // a no-op block achieves the same goal (drop the reference to `weakEngine`
                        // so a post-release reset can never touch a dangling pointer). Done first
                        // and independently of stopWithCompletionHandler: below.
                        engine.resetHandler = ^{};
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: haptic engine resetHandler teardown threw on a dead engine, ignoring: %s",
                                    exception.reason ? exception.reason.UTF8String : "unknown");
                    }
                    @try
                    {
                        [engine stopWithCompletionHandler:nil];
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: haptic engine stopWithCompletionHandler: threw on a dead engine, ignoring: %s",
                                    exception.reason ? exception.reason.UTF8String : "unknown");
                    }
                    // CFRelease must run whether or not the calls above threw: it balances the
                    // CFRetain taken in CreateStartedEngine and is the only thing that actually
                    // frees the engine, so it sits after the @try/@catch, unconditional --
                    // skipping it on an exception would leak the engine.
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
        if (@available(macOS 11.3, *))
        {
            // Also release any in-flight transient (pulse) players, same as SetVibration(0,0)
            // does for the continuous rumble players above -- both player slot pairs must be
            // torn down before the destructor releases the engines below.
            PlayTransientOnSide(m_leftEngine, &m_leftTransientPlayer, 0.0f, 0.0f);
            PlayTransientOnSide(m_rightEngine, &m_rightTransientPlayer, 0.0f, 0.0f);
        }
    }

    void DualSenseHapticsMac::PlayTransientPulse(float leftIntensity, float rightIntensity, float sharpness)
    {
        if (@available(macOS 11.3, *))
        {
            const float clampedSharpness = AZ::GetClamp(sharpness, 0.0f, 1.0f);
            PlayTransientOnSide(m_leftEngine, &m_leftTransientPlayer, AZ::GetClamp(leftIntensity, 0.0f, 1.0f), clampedSharpness);
            PlayTransientOnSide(m_rightEngine, &m_rightTransientPlayer, AZ::GetClamp(rightIntensity, 0.0f, 1.0f), clampedSharpness);
        }
    }
} // namespace DualSense
