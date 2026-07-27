#pragma once

// Phase 3a Task 3. Whole-file guarded -- see DualSenseSdlRuntime.h's header comment for why.
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>
#include <DualSense/DualSenseTriggerEffects.h>
#include <DualSense/DualSenseHaptics.h>
#include <Clients/Sdl/DualSenseSdlAxisMath.h>

#include <SDL3/SDL.h>

namespace DualSense
{
    //! Standard-gamepad backend for a DualSense driven by SDL3's SDL_Gamepad layer
    //! (dualsense_backend=sdl). Counterpart of InputDeviceGamepadDualSenseMac -- see that file's
    //! header comment for the shape this mirrors; differences from it are called out inline
    //! below and in the .cpp.
    //!
    //! Handle-layering convention used throughout this class (deliberate, not accidental
    //! inconsistency): every WRITE (trigger effects, rumble, light bar) goes through the
    //! underlying SDL_Joystick* (SDL_GetGamepadJoystick(m_gamepad)) via the exact functions the
    //! task brief names (SDL_SendJoystickEffect / SDL_RumbleJoystick / SDL_SetJoystickLED); every
    //! READ (buttons/axes/connected-state) goes through the SDL_Gamepad* handle directly, because
    //! that is the layer that gives normalized axis semantics and named PS5-independent button
    //! ids (SDL_GAMEPAD_BUTTON_SOUTH etc.) instead of raw joystick button/axis indices.
    //!
    //! Weapon-mode fire-edge detection (see InputDeviceGamepadDualSenseMac::ProcessWeaponFireEdge)
    //! has NO equivalent here: SDL exposes no adaptive-trigger status query comparable to
    //! GCDualSenseAdaptiveTrigger.status, so this class never fires
    //! DualSenseTriggerNotificationBus::OnWeaponTriggerFired. dualsense_backend=sdl callers that
    //! depend on that notification (or on SetAutoRecoil actually doing anything) get silent
    //! no-ops, not an error -- documented once here and to be restated in the README by Task 4.
    class InputDeviceGamepadDualSenseSdl
        : public AzFramework::InputDeviceGamepad::Implementation
        , public DualSenseTriggerEffectRequestBus::Handler
        , public DualSenseHapticPulseRequestBus::Handler
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Allocator -- every concrete InputDeviceGamepad::Implementation subclass MUST declare
        // this (hardware-learned rule from the Mac backend: the base macro size-asserts).
        AZ_CLASS_ALLOCATOR(InputDeviceGamepadDualSenseSdl, AZ::SystemAllocator);

        //! `joystickId` is the SDL_JoystickID the monitor detected as a PS5-type pad. Opens its
        //! own SDL_Gamepad handle (SDL_OpenGamepad is refcounted, matching the porting guide's
        //! "already-open joystick" safety-by-construction note) -- this gem is SDL's sole caller
        //! while the sdl backend is active, so there is no other owner to conflict with, but the
        //! refcounting still means a would-be double-open (e.g. a stale factory re-invoked before
        //! the previous instance for this same id is torn down) is harmless.
        InputDeviceGamepadDualSenseSdl(AzFramework::InputDeviceGamepad& inputDevice, SDL_JoystickID joystickId);
        ~InputDeviceGamepadDualSenseSdl() override;

        bool IsConnected() const override;
        void SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized) override;
        void SetLightBarColor(const AZ::Color& color) override;
        void ResetLightBarColor() override;
        void TickInputDevice() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // DualSenseTriggerEffectRequestBus::Handler
        void SetTriggerEffect(Trigger trigger, const TriggerEffect& effect) override;
        void ClearTriggerEffects() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // DualSenseHapticPulseRequestBus::Handler
        //! Degrades to a short SDL_RumbleJoystick burst -- see the class header comment and
        //! LogHapticDegradeOnce()'s AZLOG_DEBUG body for the full reasoning (no CoreHaptics-
        //! equivalent voice-coil API is reachable through SDL on any platform).
        void PlayHapticPulse(float leftIntensity, float rightIntensity, float sharpness) override;
        //! Stored but never consumed on this backend -- see class header comment (no weapon-mode
        //! fire-edge source to trigger it from).
        void SetAutoRecoil(Trigger trigger, bool enabled, float intensity, float sharpness) override;
        //! Degrades to an SDL_RumbleJoystick burst for durationSeconds -- same reasoning as
        //! PlayHapticPulse.
        void PlayHapticBuzz(float leftIntensity, float rightIntensity, float sharpness, float durationSeconds) override;
        void StopHaptics() override;

    private:
        //! SDL_GetGamepadJoystick(m_gamepad), gated by m_gamepad != nullptr and
        //! SDL_WasInit(SDL_INIT_JOYSTICK) -- the single choke point every write path below
        //! resolves the joystick handle through, so the WasInit/null gating logic lives in
        //! exactly one place.
        SDL_Joystick* JoystickHandle() const;

        //! Shared body of PlayHapticPulse/PlayHapticBuzz/StopHaptics: clamps intensities to
        //! [0,1] and durationSeconds to [0,30] (the same ceiling PlayHapticBuzz's Mac/CoreHaptics
        //! counterpart documents, kept for symmetry even though SDL_RumbleJoystick itself has no
        //! such hard limit), converts to SDL's Uint16 rumble range, and issues a single
        //! SDL_RumbleJoystick call. A (0,0) request is sent with durationMs = 0 (immediate stop)
        //! rather than the "hold" duration below, matching StopHaptics()'s "stop now" contract.
        void RumbleBurst(float leftIntensity, float rightIntensity, float durationSeconds);

        //! Logs the CoreHaptics-vs-rumble-emulation asymmetry exactly once per instance
        //! (AZLOG_DEBUG, per the task brief: "this asymmetry is expected and logged once").
        void LogHapticDegradeOnce();

        RawGamepadState m_rawGamepadState;
        SDL_JoystickID m_joystickId = 0;
        SDL_Gamepad* m_gamepad = nullptr; // owned; SDL_OpenGamepad in ctor, SDL_CloseGamepad in dtor
        bool m_wasConnected = false;
        bool m_loggedHapticDegradeOnce = false;
    };
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
