#pragma once

#include <AzCore/std/smart_ptr/shared_ptr.h>

namespace DualSense
{
    //! Drives the DualSense voice-coil actuators through CoreHaptics engines
    //! created per handle locality (GCDeviceHaptics). Emulates classic two-motor
    //! rumble: one continuous haptic player per side, intensity = motor speed.
    //! Also supports one-shot transient "kick" pulses (PlayTransientPulse) for
    //! discrete events (e.g. recoil), independent of the continuous rumble players,
    //! and sustained "buzz" pulses (PlayHapticBuzz) that SHARE the continuous slot
    //! with rumble (see PlayHapticBuzz below).
    //!
    //! Lifecycle hardening (Phase 2.6, Task 1; see ~/pong/docs/dualsense-porting-guide.md
    //! "CoreHaptics lifecycle gotchas", read-only reference, gotchas #2/#3/#5): per-side
    //! state (engine + cached players + a generation counter) is heap-allocated and
    //! reference-counted independently of this object via HapticSideState, specifically so
    //! that CHHapticEngine's resetHandler/stoppedHandler blocks -- which fire on a
    //! non-main CoreHaptics thread (gotcha #3) and may run arbitrarily late, including
    //! after this object has already been destroyed -- can always safely check `alive`
    //! and touch `generation`/cached players through their own shared_ptr copy, without
    //! ever dereferencing a potentially-freed DualSenseHapticsMac* this. See
    //! DualSenseHapticsMac.mm for the full struct definition and handler bodies.
    class DualSenseHapticsMac
    {
    public:
        explicit DualSenseHapticsMac(void* gcController); // GCController*, not retained
        ~DualSenseHapticsMac();

        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized);
        void Stop();

        //! One sharp transient kick per side, reusing the same per-handle engines as
        //! SetVibration. Intensities/sharpness are normalized [0,1]; an intensity of ~0
        //! stops that side's cached transient player and starts nothing new (no new player
        //! is created/started, but any existing cached one for that side is torn down first
        //! -- see PlayHapticBuzz below for why this is "stops", not "skips").
        void PlayTransientPulse(float leftIntensity, float rightIntensity, float sharpness);

        //! Sustained buzz per side: a single continuous CHHapticEvent with a finite
        //! duration (clamped to (0, 30], CoreHaptics' ceiling), Intensity + Sharpness
        //! parameters. SHARES the continuous player slot with SetVibration's rumble --
        //! whichever call landed most recently simply replaces the cached player for
        //! that side (last writer wins; see the porting guide's CoreHaptics-wins
        //! contention finding, applied here between this class's own two CoreHaptics
        //! callers of the shared slot). Intensity ~0 stops that side's cached player and
        //! starts nothing new -- Stop() (which relies on this clear-before-check behavior)
        //! is implemented as exactly this call with both intensities at 0.
        void PlayHapticBuzz(float leftIntensity, float rightIntensity, float sharpness, float durationSeconds);

        //! Stops gem-issued haptics (transient pulses and buzzes/rumble) on both sides:
        //! clears both the continuous and transient cached players for left and right.
        //! There is no trigger-effect state in this class to touch.
        void StopHaptics();

        //! Forward-declared; fully defined in DualSenseHapticsMac.mm (holds ObjC-typed fields as
        //! void*, matching this header's existing ObjC-free convention). Public only so the free
        //! functions in DualSenseHapticsMac.mm's anonymous namespace (UpdateContinuousSide,
        //! PlayTransientOnSide, PlayBuzzOnSide, CreateStartedSide, ClearCachedPlayer) can name it;
        //! it is not otherwise part of this class's public API.
        struct HapticSideState;

    private:
        AZStd::shared_ptr<HapticSideState> m_left;  // left-handle engine + cached players + generation
        AZStd::shared_ptr<HapticSideState> m_right; // right-handle engine + cached players + generation
    };
} // namespace DualSense
