#pragma once

namespace DualSense
{
    //! Drives the DualSense voice-coil actuators through CoreHaptics engines
    //! created per handle locality (GCDeviceHaptics). Emulates classic two-motor
    //! rumble: one continuous haptic player per side, intensity = motor speed.
    class DualSenseHapticsMac
    {
    public:
        explicit DualSenseHapticsMac(void* gcController); // GCController*, not retained
        ~DualSenseHapticsMac();

        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized);
        void Stop();

    private:
        void* m_leftEngine = nullptr;   // CHHapticEngine*, retained
        void* m_rightEngine = nullptr;  // CHHapticEngine*, retained
        void* m_leftPlayer = nullptr;   // id<CHHapticPatternPlayer>, retained
        void* m_rightPlayer = nullptr;  // id<CHHapticPatternPlayer>, retained
    };
} // namespace DualSense
