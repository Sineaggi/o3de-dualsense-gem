#pragma once

namespace DualSense
{
    //! Drives the DualSense voice-coil actuators through CoreHaptics engines
    //! created per handle locality (GCDeviceHaptics). Emulates classic two-motor
    //! rumble: one continuous haptic player per side, intensity = motor speed.
    //! Also supports one-shot transient "kick" pulses (PlayTransientPulse) for
    //! discrete events (e.g. recoil), independent of the continuous rumble players.
    class DualSenseHapticsMac
    {
    public:
        explicit DualSenseHapticsMac(void* gcController); // GCController*, not retained
        ~DualSenseHapticsMac();

        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized);
        void Stop();

        //! One sharp transient kick per side, reusing the same per-handle engines as
        //! SetVibration. Intensities/sharpness are normalized [0,1]; an intensity of ~0
        //! skips that side entirely (no player is created/started for it).
        void PlayTransientPulse(float leftIntensity, float rightIntensity, float sharpness);

    private:
        void* m_leftEngine = nullptr;   // CHHapticEngine*, retained
        void* m_rightEngine = nullptr;  // CHHapticEngine*, retained
        void* m_leftPlayer = nullptr;   // id<CHHapticPatternPlayer>, retained (continuous rumble)
        void* m_rightPlayer = nullptr;  // id<CHHapticPatternPlayer>, retained (continuous rumble)
        void* m_leftTransientPlayer = nullptr;  // id<CHHapticPatternPlayer>, retained (one-shot pulse)
        void* m_rightTransientPlayer = nullptr; // id<CHHapticPatternPlayer>, retained (one-shot pulse)
    };
} // namespace DualSense
