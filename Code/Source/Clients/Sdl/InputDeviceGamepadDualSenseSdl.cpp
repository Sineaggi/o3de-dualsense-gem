#include "InputDeviceGamepadDualSenseSdl.h"

#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <Clients/DualSenseGamepadButtonMap.h>
#include <DualSense/DualSenseDs5Protocol.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Math/Color.h>
#include <AzCore/std/algorithm.h>

namespace DualSense
{
    namespace
    {
        // SDL's rumble/LED byte ranges are Uint16 (rumble) / Uint8 (LED), independent of this
        // gem's own Ds5EffectsPacket byte layout (that packet is only used for trigger effects
        // via SDL_SendJoystickEffect -- see SetTriggerEffect below).
        AZ::u16 NormalizedToRumbleU16(float normalized)
        {
            const float clamped = AZStd::clamp(normalized, 0.0f, 1.0f);
            return static_cast<AZ::u16>(clamped * 65535.0f + 0.5f);
        }

        AZ::u8 NormalizedToColorByte(float normalized)
        {
            const float clamped = AZStd::clamp(normalized, 0.0f, 1.0f);
            return static_cast<AZ::u8>(clamped * 255.0f + 0.5f);
        }

        // SDL_RumbleJoystick's duration_ms stops the rumble automatically once it elapses --
        // there is no "hold indefinitely" flag. This gem's own SetVibration contract (mirroring
        // every other InputHapticFeedbackRequests::SetVibration implementation, including this
        // gem's own Mac/CoreHaptics one) is "set the motor speed, it stays until the next call or
        // an explicit (0,0)", i.e. level-based, not duration-based. Reconciled here by requesting
        // the longest duration SDL's Uint32 milliseconds parameter can express for any nonzero
        // request (~49.7 days -- long enough that no real play session reaches it) and instead
        // relying on the CALLER re-issuing SetVibration (as game code already does for a level-
        // based API) or an explicit (0,0) call to actually stop it early. A (0,0) request uses
        // durationMs = 0 (immediate stop) rather than this constant.
        constexpr AZ::u32 IndefiniteRumbleDurationMs = 0xFFFFFFFFu;
    } // namespace

    InputDeviceGamepadDualSenseSdl::InputDeviceGamepadDualSenseSdl(
        AzFramework::InputDeviceGamepad& inputDevice, SDL_JoystickID joystickId)
        : AzFramework::InputDeviceGamepad::Implementation(inputDevice)
        , m_rawGamepadState(GetDualSenseDigitalButtonMap())
        , m_joystickId(joystickId)
    {
        if (SDL_WasInit(SDL_INIT_JOYSTICK))
        {
            m_gamepad = SDL_OpenGamepad(joystickId);
            if (!m_gamepad)
            {
                AZLOG_WARN("DualSense (SDL): SDL_OpenGamepad failed for joystick %u: %s", joystickId, SDL_GetError());
            }
        }

        // Same defaults InputDeviceGamepadDualSenseMac uses (see that file's constructor
        // comment): match the engine's own untouched RawGamepadState defaults rather than invent
        // new tuning for this backend.
        m_rawGamepadState.m_triggerMaximumValue = 1.0f;
        m_rawGamepadState.m_triggerDeadZoneValue = 0.0f;
        m_rawGamepadState.m_thumbStickMaximumValue = 1.0f;
        m_rawGamepadState.m_thumbStickLeftDeadZone = 0.0f;
        m_rawGamepadState.m_thumbStickRightDeadZone = 0.0f;

        DualSenseTriggerEffectRequestBus::Handler::BusConnect(
            AzFramework::InputDeviceGamepad::IdForIndexN(GetInputDeviceIndex()));
        DualSenseHapticPulseRequestBus::Handler::BusConnect(
            AzFramework::InputDeviceGamepad::IdForIndexN(GetInputDeviceIndex()));
    }

    InputDeviceGamepadDualSenseSdl::~InputDeviceGamepadDualSenseSdl()
    {
        // Best-effort return-to-neutral before disconnecting the buses and closing the gamepad,
        // same ordering rationale as InputDeviceGamepadDualSenseMac's destructor.
        ClearTriggerEffects();
        StopHaptics();

        DualSenseHapticPulseRequestBus::Handler::BusDisconnect();
        DualSenseTriggerEffectRequestBus::Handler::BusDisconnect();

        if (m_wasConnected)
        {
            BroadcastInputDeviceDisconnectedEvent();
        }

        if (m_gamepad && SDL_WasInit(SDL_INIT_JOYSTICK))
        {
            AZLOG_DEBUG("DualSense (SDL): closing gamepad handle for joystick %u", m_joystickId);
            SDL_CloseGamepad(m_gamepad); // balances SDL_OpenGamepad (refcounted close)
        }
        m_gamepad = nullptr;
    }

    SDL_Joystick* InputDeviceGamepadDualSenseSdl::JoystickHandle() const
    {
        if (!m_gamepad || !SDL_WasInit(SDL_INIT_JOYSTICK))
        {
            return nullptr;
        }
        return SDL_GetGamepadJoystick(m_gamepad);
    }

    bool InputDeviceGamepadDualSenseSdl::IsConnected() const
    {
        return m_gamepad != nullptr && SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_GamepadConnected(m_gamepad);
    }

    void InputDeviceGamepadDualSenseSdl::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        SDL_Joystick* joystick = JoystickHandle();
        if (!joystick)
        {
            return;
        }

        const AZ::u16 low = NormalizedToRumbleU16(leftMotorSpeedNormalized);
        const AZ::u16 high = NormalizedToRumbleU16(rightMotorSpeedNormalized);
        // Rumble emulation goes through SDL_RumbleJoystick, NOT a hand-written Ds5EffectsPacket
        // rumble byte -- SDL's own PS5 HIDAPI driver already frames/CRCs (Bluetooth) classic
        // two-motor rumble correctly (porting guide: "SDL wraps transport differences"), so
        // writing those bytes ourselves would be a second place to get the BT wrapping wrong for
        // no benefit. See the IndefiniteRumbleDurationMs comment above for the duration choice.
        const AZ::u32 durationMs = (low == 0 && high == 0) ? 0 : IndefiniteRumbleDurationMs;
        if (!SDL_RumbleJoystick(joystick, low, high, durationMs))
        {
            AZLOG_DEBUG("DualSense (SDL): SDL_RumbleJoystick failed: %s", SDL_GetError());
        }
    }

    void InputDeviceGamepadDualSenseSdl::SetLightBarColor(const AZ::Color& color)
    {
        SDL_Joystick* joystick = JoystickHandle();
        if (!joystick)
        {
            return;
        }

        // SDL_SetJoystickLED, not a hand-written packet LED section -- same "let SDL's own
        // driver own the framing" reasoning as SetVibration above.
        if (!SDL_SetJoystickLED(
                joystick, NormalizedToColorByte(color.GetR()), NormalizedToColorByte(color.GetG()), NormalizedToColorByte(color.GetB())))
        {
            AZLOG_DEBUG("DualSense (SDL): SDL_SetJoystickLED failed: %s", SDL_GetError());
        }
    }

    void InputDeviceGamepadDualSenseSdl::ResetLightBarColor()
    {
        // No OS/SDL-level "reset to default" concept exists on this transport either -- same
        // dim blue-ish-white approximation InputDeviceGamepadDualSenseMac::ResetLightBarColor
        // uses.
        SetLightBarColor(AZ::Color(0.0f, 0.25f, 1.0f, 1.0f));
    }

    void InputDeviceGamepadDualSenseSdl::TickInputDevice()
    {
        const bool connectedNow = m_gamepad && SDL_WasInit(SDL_INIT_JOYSTICK) && SDL_GamepadConnected(m_gamepad);
        if (connectedNow)
        {
            if (!m_wasConnected)
            {
                m_wasConnected = true;
                BroadcastInputDeviceConnectedEvent();
            }

            AZ::u32 buttons = 0;
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))       { buttons |= ButtonBits::DPadUp; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))     { buttons |= ButtonBits::DPadDown; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))     { buttons |= ButtonBits::DPadLeft; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT))    { buttons |= ButtonBits::DPadRight; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_START))         { buttons |= ButtonBits::Start; }  // PS5 "options"
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_BACK))          { buttons |= ButtonBits::Select; } // PS5 "create"
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK))    { buttons |= ButtonBits::L3; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK))   { buttons |= ButtonBits::R3; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) { buttons |= ButtonBits::L1; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)){ buttons |= ButtonBits::R1; }
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_SOUTH))         { buttons |= ButtonBits::A; } // cross
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_EAST))          { buttons |= ButtonBits::B; } // circle
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_WEST))          { buttons |= ButtonBits::X; } // square
            if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_NORTH))         { buttons |= ButtonBits::Y; } // triangle

            m_rawGamepadState.m_digitalButtonStates = buttons;
            m_rawGamepadState.m_triggerButtonLState =
                SdlAxisMath::NormalizeTriggerAxis(SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
            m_rawGamepadState.m_triggerButtonRState =
                SdlAxisMath::NormalizeTriggerAxis(SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
            m_rawGamepadState.m_thumbStickLeftXState =
                SdlAxisMath::NormalizeStickAxis(SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_LEFTX));
            // SDL's Y axes report +32767 = down (per SDL_gamepad.h: "-32768 (up/left) to 32767
            // (down/right)"), but RawGamepadState wants positive = "towards the back of the
            // controller" (up when held normally) -- the engine's own Linux joystick backend
            // negates raw Y for exactly this reason (InputDeviceGamepad_Linux.cpp). Negated here
            // to match; X needs no such flip (positive = right in both conventions).
            m_rawGamepadState.m_thumbStickLeftYState =
                -SdlAxisMath::NormalizeStickAxis(SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_LEFTY));
            m_rawGamepadState.m_thumbStickRightXState =
                SdlAxisMath::NormalizeStickAxis(SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_RIGHTX));
            m_rawGamepadState.m_thumbStickRightYState =
                -SdlAxisMath::NormalizeStickAxis(SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_RIGHTY));

            // No weapon-fire-edge polling here -- see class header comment: SDL exposes no
            // adaptive-trigger status query, so OnWeaponTriggerFired never fires on this backend.
        }
        else
        {
            // Mirrors InputDeviceGamepadDualSenseMac::TickInputDevice's transient-nil handling:
            // zero the raw state so a stale value isn't replayed as "stuck" input, but do NOT
            // broadcast a disconnect here -- that only happens in the destructor (m_wasConnected
            // check), once DualSenseSdlMonitor actually tears this instance down following a real
            // SDL-side disconnect.
            m_rawGamepadState.m_digitalButtonStates = 0;
            m_rawGamepadState.m_triggerButtonLState = 0.0f;
            m_rawGamepadState.m_triggerButtonRState = 0.0f;
            m_rawGamepadState.m_thumbStickLeftXState = 0.0f;
            m_rawGamepadState.m_thumbStickLeftYState = 0.0f;
            m_rawGamepadState.m_thumbStickRightXState = 0.0f;
            m_rawGamepadState.m_thumbStickRightYState = 0.0f;
        }
        ProcessRawGamepadState(m_rawGamepadState);
    }

    void InputDeviceGamepadDualSenseSdl::SetTriggerEffect(Trigger trigger, const TriggerEffect& effect)
    {
        SDL_Joystick* joystick = JoystickHandle();
        if (!joystick)
        {
            return;
        }

        // Unlike the Mac backend, there is no macOS-version-gated "extended API" concept here --
        // the raw DS5 output-report compiler (CompileTriggerEffectRaw) supports every
        // TriggerEffectMode unconditionally over HID, so no RequiresExtendedTriggerApi /
        // DegradeToBaselineApi step is needed on this path.
        const TriggerEffect resolved = effect.Clamped();
        const auto block = CompileTriggerEffectRaw(resolved);

        // All-zero valid flags by default: only the section(s) actually set below get applied,
        // per the porting guide's "set only the flags for what you're changing" rule -- see
        // Ds5EffectsPacket's own doc comment for why this means a right-only write never
        // disturbs left-trigger/rumble/LED state.
        Ds5EffectsPacket packet{};
        if (trigger == Trigger::L2 || trigger == Trigger::Both)
        {
            packet.SetLeftTriggerBlock(block);
        }
        if (trigger == Trigger::R2 || trigger == Trigger::Both)
        {
            packet.SetRightTriggerBlock(block);
        }

        // LockJoysticks/UnlockJoysticks around the send, per the porting guide's discipline (the
        // lock is reentrant). Every individual SDL joystick call used in this file is documented
        // by SDL3 itself as safe from any thread on its own, so in this gem's single-owner,
        // main-thread-only-caller model the lock is not load-bearing for correctness today -- it
        // is kept anyway because it is what the task brief and guide both specify, and it costs
        // nothing to hold across a single call with no engine-API reentry inside the critical
        // section (avoiding the guide's lock-order-inversion warning).
        SDL_LockJoysticks();
        const bool sent = SDL_SendJoystickEffect(joystick, &packet, sizeof(packet));
        SDL_UnlockJoysticks();
        if (!sent)
        {
            AZLOG_DEBUG("DualSense (SDL): SDL_SendJoystickEffect failed: %s", SDL_GetError());
        }
    }

    void InputDeviceGamepadDualSenseSdl::ClearTriggerEffects()
    {
        SetTriggerEffect(Trigger::Both, TriggerEffect{});
    }

    void InputDeviceGamepadDualSenseSdl::LogHapticDegradeOnce()
    {
        if (m_loggedHapticDegradeOnce)
        {
            return;
        }
        m_loggedHapticDegradeOnce = true;
        AZLOG_DEBUG(
            "DualSense (SDL): PlayHapticPulse/PlayHapticBuzz degrade to SDL_RumbleJoystick bursts on this backend "
            "-- there is no CoreHaptics-equivalent voice-coil API reachable through SDL on any platform. Intensity "
            "and duration map onto the rumble call; sharpness has no SDL analog and is ignored. This asymmetry "
            "versus the Mac (CoreHaptics) backend is expected.");
    }

    void InputDeviceGamepadDualSenseSdl::RumbleBurst(float leftIntensity, float rightIntensity, float durationSeconds)
    {
        SDL_Joystick* joystick = JoystickHandle();
        if (!joystick)
        {
            return;
        }

        const AZ::u16 low = NormalizedToRumbleU16(leftIntensity);
        const AZ::u16 high = NormalizedToRumbleU16(rightIntensity);
        const AZ::u32 durationMs =
            (low == 0 && high == 0) ? 0 : static_cast<AZ::u32>(AZStd::clamp(durationSeconds, 0.0f, 30.0f) * 1000.0f);
        if (!SDL_RumbleJoystick(joystick, low, high, durationMs))
        {
            AZLOG_DEBUG("DualSense (SDL): SDL_RumbleJoystick (haptic-degrade burst) failed: %s", SDL_GetError());
        }
    }

    void InputDeviceGamepadDualSenseSdl::PlayHapticPulse(float leftIntensity, float rightIntensity, float sharpness)
    {
        AZ_UNUSED(sharpness); // no SDL rumble analog for sharpness/frequency shaping
        LogHapticDegradeOnce();
        constexpr float PulseBurstSeconds = 0.08f; // short "tap"-like burst
        RumbleBurst(leftIntensity, rightIntensity, PulseBurstSeconds);
    }

    void InputDeviceGamepadDualSenseSdl::SetAutoRecoil(Trigger, bool, float, float)
    {
        // Intentionally a no-op: see the class header comment. Accepted (not asserted) rather
        // than rejected, so callers written against the Mac backend's SetAutoRecoil don't need a
        // backend-specific branch -- they just never see the recoil kick under dualsense_backend
        // sdl (documented degradation, Task 4 README).
    }

    void InputDeviceGamepadDualSenseSdl::PlayHapticBuzz(
        float leftIntensity, float rightIntensity, float sharpness, float durationSeconds)
    {
        AZ_UNUSED(sharpness);
        LogHapticDegradeOnce();
        RumbleBurst(leftIntensity, rightIntensity, durationSeconds);
    }

    void InputDeviceGamepadDualSenseSdl::StopHaptics()
    {
        RumbleBurst(0.0f, 0.0f, 0.0f);
    }
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
