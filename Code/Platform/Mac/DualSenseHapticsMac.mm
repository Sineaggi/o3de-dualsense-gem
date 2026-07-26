#include "DualSenseHapticsMac.h"

#include <AzCore/Console/ILogger.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/parallel/atomic.h>
#include <AzCore/std/smart_ptr/make_shared.h>

#import <GameController/GameController.h>
#import <CoreHaptics/CoreHaptics.h>

namespace DualSense
{
    // This translation unit is compiled without ARC (matching the rest of this gem's Mac
    // sources, see InputDeviceGamepadDualSenseMac.mm), so __bridge_retained / __bridge_transfer
    // are unavailable (they require ARC to insert the retain/release). CFRetain/CFRelease work
    // directly on Objective-C objects because their retain counts are toll-free bridged with
    // CoreFoundation on Apple platforms, so plain `(__bridge T*)` casts plus explicit
    // CFRetain/CFRelease reproduce the same ownership-transfer contract without ARC.

    // Per-side (left/right handle) CoreHaptics state. Heap-allocated and reference-counted via
    // AZStd::shared_ptr, deliberately independent of DualSenseHapticsMac's own lifetime.
    //
    // Why: the porting guide's CoreHaptics lifecycle gotcha #3 documents that
    // resetHandler/stoppedHandler fire on a non-main CoreHaptics thread. A handler block
    // captures a shared_ptr<HapticSideState> BY VALUE (a normal, non-__block capture -- see
    // CreateStartedSide below), which keeps this struct alive for as long as the block exists,
    // even after the owning DualSenseHapticsMac has been destroyed and its own m_left/m_right
    // members have been reset. That lets a handler safely read `alive`/`generation` (both
    // AZStd::atomic, safe to touch from any thread) from ANY thread, at ANY time, without ever
    // dereferencing a potentially-freed DualSenseHapticsMac* this -- the exact latent race the
    // previous implementation had (its resetHandler captured a raw, non-retaining
    // `__block CHHapticEngine* weakEngine` with no liveness check at all).
    //
    // `engine`/`continuousPlayer`/`transientPlayer` are NOT thread-safe on their own: they are
    // only ever read/written on the main thread (every public DualSenseHapticsMac method runs
    // on the caller's thread, which in this gem is always the main/game thread, same as the
    // dispatch_async(main queue) hop both handlers take before touching this state -- see
    // gotcha #3 below). `alive` and `generation` are the only fields touched off-main (a
    // handler snapshots them before/without dispatching), so only those two need to be atomic.
    struct DualSenseHapticsMac::HapticSideState
    {
        AZStd::atomic<bool> alive{ true };       // false once the owning DualSenseHapticsMac is
                                                  // destroyed; checked before touching `engine` or
                                                  // any cached player (gotcha #3).
        AZStd::atomic<uint32_t> generation{ 0 }; // bumped on every cached-player store/clear;
                                                  // deferred handler clears snapshot this and
                                                  // re-check it before nil-ing anything, so a
                                                  // reconnect-and-replay that already installed a
                                                  // *new* player in between is never clobbered
                                                  // (gotcha #5).
        void* engine = nullptr;           // CHHapticEngine*, retained; owned by this struct
        void* continuousPlayer = nullptr; // id<CHHapticPatternPlayer>, retained; SHARED slot for
                                           // both SetVibration (rumble) and PlayHapticBuzz
        void* transientPlayer = nullptr;  // id<CHHapticPatternPlayer>, retained; one-shot pulses
    };

    namespace
    {
        // Clears *playerSlot if non-null: stops the cached player (dead-engine hardened, see the
        // @try/@catch comment below), CFReleases it, nulls the slot, and bumps `generation`. Used
        // both by the "replace the old player before installing a new one" step in
        // UpdateContinuousSide/PlayBuzzOnSide/PlayTransientOnSide below, AND by the deferred
        // stoppedHandler clear in CreateStartedSide -- a single implementation for "drop whatever
        // player is cached here" keeps the MRC bookkeeping (retain/release counts) in exactly one
        // place.
        void ClearCachedPlayer(void** playerSlot, AZStd::atomic<uint32_t>& generation, const char* debugLabel) API_AVAILABLE(macos(11.3))
        {
            if (!*playerSlot)
            {
                return;
            }
            id<CHHapticPatternPlayer> old = (__bridge id<CHHapticPatternPlayer>)*playerSlot;
            @try
            {
                // A dead engine (the controller unplugged, or the engine already stopped) can
                // make -[PatternPlayer stopAtTime:error:] throw an NSException instead of only
                // returning an NSError; uncaught, it unwinds through this C++ frame into
                // std::terminate. This is the exact crash from hardware forensics (see the
                // matching comment history on the pre-Task-1 UpdateSide/PlayTransientOnSide).
                [old stopAtTime:0 error:nil];
            }
            @catch (NSException* exception)
            {
                AZLOG_DEBUG("DualSense: %s stopAtTime: threw on a dead haptic engine, ignoring: %s",
                            debugLabel, exception.reason ? exception.reason.UTF8String : "unknown");
            }
            // `old` is a __bridge (non-owning) reference into *playerSlot; the CFRelease below is
            // the only owning release for this player and must run whether or not stop threw, so
            // it sits after the @try/@catch, unconditional.
            CFRelease(*playerSlot); // balances the CFRetain taken when this player was stored
            *playerSlot = nullptr;
            generation.fetch_add(1, AZStd::memory_order_release);
        }

        // Replaces side->continuousPlayer with a new infinite continuous player at `intensity`,
        // or stops/clears it when intensity is ~0. This is the rumble (SetVibration) path; it
        // shares side->continuousPlayer with PlayBuzzOnSide below -- see PlayHapticBuzz's header
        // doc comment for the last-writer-wins contract this implements structurally (both
        // functions read/clear/store the exact same field).
        void UpdateContinuousSide(DualSenseHapticsMac::HapticSideState* side, float intensity) API_AVAILABLE(macos(11.3))
        {
            if (!side || !side->engine)
            {
                return;
            }
            ClearCachedPlayer(&side->continuousPlayer, side->generation, "continuous(rumble)");
            if (intensity <= 0.001f)
            {
                return;
            }
            CHHapticEngine* engine = (__bridge CHHapticEngine*)side->engine;

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
            // Likewise, `pattern` does not retain the array we pass it, only what it needs from
            // the elements, so our +1 on `event` is no longer needed.
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
                // A dead engine (the controller unplugged) can make createPlayerWithPattern:error:
                // and/or startAtTime:error: throw an NSException instead of only populating
                // `error`. `player` returned here is autoreleased (not an owned reference, see the
                // CFRetain comment below), so a throw before or after it's assigned leaves nothing
                // extra to release: on catch this is simply treated as a failed start, and the
                // player is never stored.
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
            // not retain it beyond the call); release our +1 unconditionally now that the call has
            // either returned normally or been caught, so it happens on every path exactly once.
            [pattern release];
            if (!player || !playerStarted)
            {
                AZLOG_WARN("DualSense: haptic player start failed: %s",
                           error ? error.localizedDescription.UTF8String : "unknown");
                return;
            }

            // `player` is autoreleased (createPlayerWithPattern:error: does not return an owned
            // reference). Take ownership for the lifetime of the cached slot.
            void* opaquePlayer = (__bridge void*)player;
            CFRetain(opaquePlayer);
            side->continuousPlayer = opaquePlayer;
            side->generation.fetch_add(1, AZStd::memory_order_release);
        }

        // One-shot counterpart to UpdateContinuousSide: replaces side->transientPlayer with a new
        // transient (single "kick", non-looping) player built from `intensity` + `sharpness`, or
        // clears it when intensity is ~0. Mirrors UpdateContinuousSide's rolling-slot MRC idiom
        // and dead-engine hardening exactly, but builds a CHHapticEventTypeHapticTransient event
        // (with Intensity + Sharpness parameters) instead of UpdateContinuousSide's
        // CHHapticEventTypeHapticContinuous (infinite rumble) event, and uses the independent
        // side->transientPlayer slot (never shared with the continuous rumble/buzz slot).
        void PlayTransientOnSide(DualSenseHapticsMac::HapticSideState* side, float intensity, float sharpness) API_AVAILABLE(macos(11.3))
        {
            if (!side || !side->engine)
            {
                return;
            }
            ClearCachedPlayer(&side->transientPlayer, side->generation, "transient");
            if (intensity <= 0.001f)
            {
                return;
            }
            CHHapticEngine* engine = (__bridge CHHapticEngine*)side->engine;

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
            // Likewise, `pattern` does not retain the array we pass it, only what it needs from
            // the elements, so our +1 on `event` is no longer needed.
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
                // Same autoreleased-return reasoning as UpdateContinuousSide: `player` is not an
                // owned reference here, so a throw before or after it's assigned leaves nothing
                // extra to release; on catch this is simply a failed start, and the player is
                // never stored.
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
            // not retain it beyond the call); release our +1 unconditionally now that the call has
            // either returned normally or been caught, so it happens on every path exactly once.
            [pattern release];
            if (!player || !playerStarted)
            {
                AZLOG_WARN("DualSense: transient haptic player start failed: %s",
                           error ? error.localizedDescription.UTF8String : "unknown");
                return;
            }

            void* opaquePlayer = (__bridge void*)player;
            CFRetain(opaquePlayer);
            side->transientPlayer = opaquePlayer;
            side->generation.fetch_add(1, AZStd::memory_order_release);
        }

        // Sustained-buzz counterpart to UpdateContinuousSide: replaces side->continuousPlayer
        // (the SAME slot UpdateContinuousSide/rumble uses -- see PlayHapticBuzz's header doc
        // comment) with a finite-duration continuous player built from `intensity` + `sharpness`
        // + `durationSeconds`, or clears it when intensity is ~0. `durationSeconds` is expected to
        // already be clamped to (0, 30] by the caller (DualSenseHapticsMac::PlayHapticBuzz).
        void PlayBuzzOnSide(DualSenseHapticsMac::HapticSideState* side, float intensity, float sharpness, float durationSeconds) API_AVAILABLE(macos(11.3))
        {
            if (!side || !side->engine)
            {
                return;
            }
            // Shares side->continuousPlayer with UpdateContinuousSide: clearing here replaces
            // whichever of the two calls last owned the slot (rumble or a previous buzz), and a
            // subsequent SetVibration call will just as readily replace what this call stores --
            // last writer wins per side, by construction of both functions touching one field.
            ClearCachedPlayer(&side->continuousPlayer, side->generation, "continuous(buzz)");
            if (intensity <= 0.001f)
            {
                return;
            }
            CHHapticEngine* engine = (__bridge CHHapticEngine*)side->engine;

            NSError* error = nil;
            CHHapticEventParameter* intensityParam =
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity
                                                              value:intensity];
            CHHapticEventParameter* sharpnessParam =
                [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness
                                                              value:sharpness];
            CHHapticEvent* event =
                [[CHHapticEvent alloc] initWithEventType:CHHapticEventTypeHapticContinuous
                                              parameters:@[ intensityParam, sharpnessParam ]
                                            relativeTime:0
                                                duration:durationSeconds];
            // `event` retains what it needs from the parameters internally (per the
            // CHHapticEvent contract); our +1s from alloc/init are no longer needed.
            [intensityParam release];
            [sharpnessParam release];

            CHHapticPattern* pattern = [[CHHapticPattern alloc] initWithEvents:@[ event ]
                                                               parameterCurves:@[]
                                                                         error:&error];
            // Likewise, `pattern` does not retain the array we pass it, only what it needs from
            // the elements, so our +1 on `event` is no longer needed.
            [event release];
            if (!pattern)
            {
                AZLOG_WARN("DualSense: buzz haptic pattern creation failed: %s",
                           error.localizedDescription.UTF8String);
                return;
            }
            id<CHHapticPatternPlayer> player = nil;
            BOOL playerStarted = NO;
            @try
            {
                // Same autoreleased-return reasoning as UpdateContinuousSide/PlayTransientOnSide.
                player = [engine createPlayerWithPattern:pattern error:&error];
                if (player)
                {
                    playerStarted = [player startAtTime:0 error:&error];
                }
            }
            @catch (NSException* exception)
            {
                AZLOG_DEBUG("DualSense: buzz haptic player create/start threw on a dead engine, ignoring: %s",
                            exception.reason ? exception.reason.UTF8String : "unknown");
                player = nil;
                playerStarted = NO;
            }
            [pattern release];
            if (!player || !playerStarted)
            {
                AZLOG_WARN("DualSense: buzz haptic player start failed: %s",
                           error ? error.localizedDescription.UTF8String : "unknown");
                return;
            }

            void* opaquePlayer = (__bridge void*)player;
            CFRetain(opaquePlayer);
            side->continuousPlayer = opaquePlayer;
            side->generation.fetch_add(1, AZStd::memory_order_release);
        }

        // Creates and starts a CHHapticEngine for `locality`, wraps it in a HapticSideState, and
        // installs resetHandler/stoppedHandler on it. Returns nullptr (as an empty shared_ptr) if
        // the engine can't be created or started, matching the previous CreateStartedEngine's
        // nullptr-on-failure contract.
        AZStd::shared_ptr<DualSenseHapticsMac::HapticSideState> CreateStartedSide(
            GCController* controller, GCHapticsLocality locality) API_AVAILABLE(macos(11.3))
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
                // Symmetry with UpdateContinuousSide/the destructor below: a dead engine (e.g.
                // the controller disconnecting during handshake) can make CoreHaptics throw an
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

            auto side = AZStd::make_shared<DualSenseHapticsMac::HapticSideState>();
            // `engine` is autoreleased (createEngineWithLocality: does not return an owned
            // reference). Take ownership for the lifetime of `side` (released in
            // DualSenseHapticsMac::~DualSenseHapticsMac).
            void* opaqueEngine = (__bridge void*)engine;
            CFRetain(opaqueEngine);
            side->engine = opaqueEngine;

            // `side` is captured BY VALUE (a plain, non-__block local capture) into both handler
            // blocks below -- Objective-C blocks copy-construct any captured non-trivial C++
            // object (shared_ptr included) when the block itself is copied, which happens
            // implicitly here because engine.resetHandler/stoppedHandler are declared `copy`-
            // semantics properties (CHHapticEngine.h). Each block therefore holds its own
            // independent AZStd::shared_ptr<HapticSideState> reference, keeping the struct (and
            // its atomics) alive for as long as CoreHaptics retains the block -- regardless of
            // whether the owning DualSenseHapticsMac (and its own side->engine-owning reference)
            // still exists. This is what makes it safe for the handler bodies below to touch
            // `alive`/`generation` from a CoreHaptics thread at arbitrary times (gotcha #3/#5).

            // Guide gotcha #2: engines auto-stop when idle and reset on error. Start-on-demand is
            // handled by every Update*/Play* function above (a running engine's startAndReturnError
            // is a no-op per Apple's docs); resetHandler restarts the engine, stoppedHandler (new
            // in this task) clears this side's cached players so a later Update*/Play* call
            // rebuilds fresh ones instead of reusing now-invalid players.
            //
            // Guide gotcha #3: both handlers fire on a non-main CoreHaptics thread while
            // SetVibration/PlayHapticBuzz/PlayTransientPulse/Stop mutate this same cached state
            // from the main thread -- dispatch_async to the main queue serializes the handler BODY
            // against those calls instead of racing them. (This replaces the previous
            // implementation's unguarded `engine.resetHandler = ^{ [weakEngine
            // startAndReturnError:nil]; };`, which ran on whatever thread CoreHaptics chose, with
            // no liveness check on `weakEngine` at all -- the exact latent race this task fixes.)
            engine.resetHandler = ^{
                dispatch_async(dispatch_get_main_queue(), ^{
                    // `alive` false means the owning DualSenseHapticsMac's destructor already ran
                    // and CFRelease'd side->engine -- touching it here would message a dangling
                    // ObjC object. Bail before reading `engine` at all (gotcha #3).
                    if (!side->alive.load(AZStd::memory_order_acquire))
                    {
                        return;
                    }
                    CHHapticEngine* engineToRestart = (__bridge CHHapticEngine*)side->engine;
                    @try
                    {
                        [engineToRestart startAndReturnError:nil];
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: resetHandler restart threw on a dead engine, ignoring: %s",
                                    exception.reason ? exception.reason.UTF8String : "unknown");
                    }
                });
            };

            engine.stoppedHandler = ^(CHHapticEngineStoppedReason reason)
            {
                (void)reason;
                // Guide gotcha #5: snapshot `generation` NOW, on whatever thread this handler
                // fires on -- it identifies "the cached players as of this stop event". The
                // dispatched block below re-reads it right before acting; any store/clear that
                // lands on the main thread in between (a reconnect-and-replay racing this
                // notification) bumps `generation` again, so the compare below will observe a
                // mismatch and skip the clear instead of nil-ing the *new* player.
                const uint32_t observedGeneration = side->generation.load(AZStd::memory_order_acquire);
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (!side->alive.load(AZStd::memory_order_acquire))
                    {
                        return;
                    }
                    if (side->generation.load(AZStd::memory_order_acquire) != observedGeneration)
                    {
                        // Stale: something already replaced this side's cached player(s) after
                        // the engine reported stopped. Clearing now would nil the new player and
                        // leave it unstoppable from the cache's point of view (gotcha #5).
                        return;
                    }
                    // Clear this side's cached players (both the shared continuous slot and the
                    // transient slot) -- the engine itself is left alone; resetHandler above owns
                    // restarting it.
                    ClearCachedPlayer(&side->continuousPlayer, side->generation, "continuous(stoppedHandler)");
                    ClearCachedPlayer(&side->transientPlayer, side->generation, "transient(stoppedHandler)");
                });
            };

            return side;
        }
    } // namespace

    DualSenseHapticsMac::DualSenseHapticsMac(void* gcController)
    {
        if (@available(macOS 11.3, *))
        {
            GCController* controller = (__bridge GCController*)gcController;
            m_left = CreateStartedSide(controller, GCHapticsLocalityLeftHandle);
            m_right = CreateStartedSide(controller, GCHapticsLocalityRightHandle);
        }
    }

    DualSenseHapticsMac::~DualSenseHapticsMac()
    {
        Stop(); // clears cached players (both slots, both sides) while the engines are still valid
        if (@available(macOS 11.3, *))
        {
            for (AZStd::shared_ptr<HapticSideState>* sidePtr : { &m_left, &m_right })
            {
                AZStd::shared_ptr<HapticSideState>& side = *sidePtr;
                if (!side)
                {
                    continue;
                }
                // Flip `alive` false FIRST, before releasing the engine below: this object's
                // destructor and any already-in-flight dispatch_async(main queue) handler body
                // (see CreateStartedSide) both run on the main thread/queue, so they can never
                // execute concurrently with each other -- either this flip happens-before a
                // pending handler body runs (it will observe alive==false and return before
                // touching `engine`), or the handler body has already finished (and could not
                // have observed anything but the engine's still-valid pre-teardown state). Either
                // way, ordering this flip before CFRelease(side->engine) below is what makes that
                // guarantee hold (gotcha #3). Any handler block still holds its OWN
                // AZStd::shared_ptr<HapticSideState> copy, independent of this object's `side`
                // reference, so the struct itself survives (heap-allocated, ref-counted) even
                // after `side.reset()` at the bottom of this loop -- it just sits inert with
                // alive==false and engine==nullptr until the last block referencing it is
                // disposed.
                side->alive.store(false, AZStd::memory_order_release);

                if (side->engine)
                {
                    CHHapticEngine* engine = (__bridge CHHapticEngine*)side->engine;
                    // A dead engine (the controller already gone) can throw from
                    // stopWithCompletionHandler: or from the resetHandler/stoppedHandler property
                    // setters instead of failing silently. An exception must NEVER escape a
                    // destructor -- that is exactly what aborted the Editor in the field (see the
                    // stopAtTime: crash forensics referenced throughout this file) -- so every
                    // remaining engine call here is guarded. Each call gets its OWN @try/@catch
                    // (rather than sharing one) so a throw from one can never skip the others'
                    // mitigation.
                    @try
                    {
                        // resetHandler/stoppedHandler are declared nonnull by the SDK
                        // (NS_ASSUME_NONNULL region in CHHapticEngine.h), so they can't be set to
                        // nil under -Werror -Wnonnull; no-op blocks achieve the same goal (drop
                        // the block's reference to `side`, and stop calling back into this side's
                        // state at all) without violating that.
                        engine.resetHandler = ^{};
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: haptic engine resetHandler teardown threw on a dead engine, ignoring: %s",
                                    exception.reason ? exception.reason.UTF8String : "unknown");
                    }
                    @try
                    {
                        engine.stoppedHandler = ^(CHHapticEngineStoppedReason reason) { (void)reason; };
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: haptic engine stoppedHandler teardown threw on a dead engine, ignoring: %s",
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
                    // CFRetain taken in CreateStartedSide and is the only thing that actually
                    // frees the engine, so it sits after the @try/@catch, unconditional --
                    // skipping it on an exception would leak the engine.
                    CFRelease(side->engine); // balances the CFRetain in CreateStartedSide
                    side->engine = nullptr;
                }
                side.reset(); // drop this object's reference; see the alive-flip comment above
            }
        }
    }

    void DualSenseHapticsMac::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        if (@available(macOS 11.3, *))
        {
            UpdateContinuousSide(m_left.get(), AZ::GetClamp(leftMotorSpeedNormalized, 0.0f, 1.0f));
            UpdateContinuousSide(m_right.get(), AZ::GetClamp(rightMotorSpeedNormalized, 0.0f, 1.0f));
        }
    }

    void DualSenseHapticsMac::Stop()
    {
        SetVibration(0.0f, 0.0f);
        if (@available(macOS 11.3, *))
        {
            // Also release any in-flight transient (pulse) players, same as SetVibration(0,0)
            // does for the continuous rumble/buzz players above -- both player slots must be
            // torn down before the destructor releases the engines below.
            PlayTransientOnSide(m_left.get(), 0.0f, 0.0f);
            PlayTransientOnSide(m_right.get(), 0.0f, 0.0f);
        }
    }

    void DualSenseHapticsMac::PlayTransientPulse(float leftIntensity, float rightIntensity, float sharpness)
    {
        if (@available(macOS 11.3, *))
        {
            const float clampedSharpness = AZ::GetClamp(sharpness, 0.0f, 1.0f);
            PlayTransientOnSide(m_left.get(), AZ::GetClamp(leftIntensity, 0.0f, 1.0f), clampedSharpness);
            PlayTransientOnSide(m_right.get(), AZ::GetClamp(rightIntensity, 0.0f, 1.0f), clampedSharpness);
        }
    }

    void DualSenseHapticsMac::PlayHapticBuzz(float leftIntensity, float rightIntensity, float sharpness, float durationSeconds)
    {
        if (@available(macOS 11.3, *))
        {
            // Duration clamped to (0, 30] -- CoreHaptics' continuous-event ceiling (porting
            // guide: "Buzz = one continuous event with duration (CoreHaptics ceiling: 30s)").
            // The lower bound is a small epsilon rather than 0 itself: a zero/negative duration
            // would otherwise build a degenerate (or CoreHaptics-rejected) event instead of
            // simply being clamped like every other parameter here.
            constexpr float kMinBuzzDurationSeconds = 0.001f;
            constexpr float kMaxBuzzDurationSeconds = 30.0f;
            const float clampedDuration = AZ::GetClamp(durationSeconds, kMinBuzzDurationSeconds, kMaxBuzzDurationSeconds);
            const float clampedSharpness = AZ::GetClamp(sharpness, 0.0f, 1.0f);
            // Buzz shares the continuous actuator slot with SetVibration (rumble): see this
            // method's declaration comment in DualSenseHapticsMac.h and PlayBuzzOnSide's comment
            // above for the last-writer-wins mechanics.
            PlayBuzzOnSide(m_left.get(), AZ::GetClamp(leftIntensity, 0.0f, 1.0f), clampedSharpness, clampedDuration);
            PlayBuzzOnSide(m_right.get(), AZ::GetClamp(rightIntensity, 0.0f, 1.0f), clampedSharpness, clampedDuration);
        }
    }

    void DualSenseHapticsMac::StopHaptics()
    {
        // Same effect as Stop() above -- a separate public entry point exists to match the
        // DualSenseHapticPulseRequests::StopHaptics contract (Code/Include/DualSense/
        // DualSenseHaptics.h): clears transient + continuous slots both sides, never touches
        // trigger effects (this class has no trigger-effect state at all, so there is nothing
        // extra to guard here).
        Stop();
    }
} // namespace DualSense
