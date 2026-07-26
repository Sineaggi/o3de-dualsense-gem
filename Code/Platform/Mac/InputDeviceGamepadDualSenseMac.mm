#include "InputDeviceGamepadDualSenseMac.h"
#include <Clients/DualSenseGamepadButtonMap.h>
#include "DualSenseMacGamepadImplFactory.h"
#include "DualSenseHapticsMac.h"

#include <DualSense/DualSenseTriggerEffectMapping.h>

#include <AzCore/Console/ILogger.h>
#include <AzCore/Math/Color.h>

#import <GameController/GameController.h>

namespace DualSense
{
    namespace
    {
        // Maps the raw GameController.framework status enum to the gem-side, ObjC-free
        // WeaponTriggerStatus (Code/Source/Clients/DualSenseTriggerFireDetector.h). Only the
        // three Weapon-mode statuses map to a known WeaponTriggerStatus; every other status
        // (Off/Feedback/Vibration/SlopeFeedback statuses, and GCDualSenseAdaptiveTriggerStatusUnknown
        // itself) maps to Unknown, since IsWeaponFireEdge only cares about the Weapon-mode
        // ready/firing/fired progression.
        WeaponTriggerStatus MapWeaponTriggerStatus(GCDualSenseAdaptiveTriggerStatus status) API_AVAILABLE(macos(11.3))
        {
            switch (status)
            {
            case GCDualSenseAdaptiveTriggerStatusWeaponReady:
                return WeaponTriggerStatus::Ready;
            case GCDualSenseAdaptiveTriggerStatusWeaponFiring:
                return WeaponTriggerStatus::Firing;
            case GCDualSenseAdaptiveTriggerStatusWeaponFired:
                return WeaponTriggerStatus::Fired;
            default:
                return WeaponTriggerStatus::Unknown;
            }
        }
    } // namespace

    InputDeviceGamepadDualSenseMac::InputDeviceGamepadDualSenseMac(
        AzFramework::InputDeviceGamepad& inputDevice, void* gcController)
        : AzFramework::InputDeviceGamepad::Implementation(inputDevice)
        , m_rawGamepadState(GetDualSenseDigitalButtonMap())
        , m_controller(gcController)
    {
        if (m_controller)
        {
            // This translation unit is compiled without ARC (matching the rest of this
            // gem's Mac sources), so __bridge_retained is unavailable (it requires ARC's
            // cast-bridging to insert the retain). CFRetain/CFRelease work directly on
            // Objective-C objects because their retain counts are toll-free bridged with
            // CoreFoundation on Apple platforms, so this achieves the same "retain on
            // construction, CFRelease on destruction" contract without ARC.
            CFRetain(m_controller);
        }

        m_haptics = AZStd::make_unique<DualSenseHapticsMac>(gcController);

        // Values from the engine's InputDeviceGamepad_Mac.mm (Step 1): that file never
        // overrides these RawGamepadState fields, so it runs with the engine's own
        // untouched defaults (see RawGamepadState::RawGamepadState in
        // AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.cpp). We match those
        // same defaults here rather than invent new tuning.
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

    InputDeviceGamepadDualSenseMac::~InputDeviceGamepadDualSenseMac()
    {
        // Best-effort: return both triggers to neutral before disconnecting the bus and
        // tearing down the controller/haptics below. m_controller is still valid at this
        // point, and every ObjC send along this path -- the pad/trigger resolution in
        // SetTriggerEffect as well as the mode-setter calls inside ApplyEffectToTrigger --
        // is individually @try/@catch-guarded, so this is safe even against a controller
        // that has already gone away (dead-controller hardening, same rationale as
        // DualSenseHapticsMac).
        ClearTriggerEffects();
        DualSenseTriggerEffectRequestBus::Handler::BusDisconnect();
        DualSenseHapticPulseRequestBus::Handler::BusDisconnect();

        if (m_wasConnected)
        {
            BroadcastInputDeviceDisconnectedEvent();
        }
        m_haptics.reset(); // must be destroyed before the controller is released below
        if (m_controller)
        {
            CFRelease(m_controller); // balances the CFRetain in the constructor
        }
    }

    bool InputDeviceGamepadDualSenseMac::IsConnected() const
    {
        return m_controller != nullptr;
    }

    void InputDeviceGamepadDualSenseMac::SetVibration(float leftMotorSpeedNormalized, float rightMotorSpeedNormalized)
    {
        if (m_haptics)
        {
            m_haptics->SetVibration(leftMotorSpeedNormalized, rightMotorSpeedNormalized);
        }
    }

    void InputDeviceGamepadDualSenseMac::SetLightBarColor(const AZ::Color& color)
    {
        if (@available(macOS 11.3, *))
        {
            GCController* controller = (__bridge GCController*)m_controller;
            if (controller.light)
            {
                // Create a GCColor with the provided color values.
                // In MRC (non-ARC) mode, the alloc'd GCColor is retained by the property
                // assignment, so we must autorelease it to prevent a leak.
                GCColor* gcColor = [[GCColor alloc] initWithRed:color.GetR()
                                                          green:color.GetG()
                                                           blue:color.GetB()];
                controller.light.color = gcColor;
                [gcColor autorelease];
            }
        }
    }

    void InputDeviceGamepadDualSenseMac::ResetLightBarColor()
    {
        // The DualSense default when attached to a Mac is a dim blue-ish white;
        // there is no OS "reset" API, so approximate the default.
        SetLightBarColor(AZ::Color(0.0f, 0.25f, 1.0f, 1.0f));
    }

    void InputDeviceGamepadDualSenseMac::TickInputDevice()
    {
        GCController* controller = (__bridge GCController*)m_controller;
        if (@available(macOS 11.3, *))
        {
            GCDualSenseGamepad* pad = (GCDualSenseGamepad*)controller.extendedGamepad;
            if (pad)
            {
                if (!m_wasConnected)
                {
                    m_wasConnected = true;
                    BroadcastInputDeviceConnectedEvent();
                }

                AZ::u32 buttons = 0;
                if (pad.dpad.up.pressed)              { buttons |= ButtonBits::DPadUp; }
                if (pad.dpad.down.pressed)            { buttons |= ButtonBits::DPadDown; }
                if (pad.dpad.left.pressed)            { buttons |= ButtonBits::DPadLeft; }
                if (pad.dpad.right.pressed)           { buttons |= ButtonBits::DPadRight; }
                if (pad.buttonMenu.pressed)           { buttons |= ButtonBits::Start; }
                if (pad.buttonOptions.pressed)        { buttons |= ButtonBits::Select; }
                if (pad.leftThumbstickButton.pressed) { buttons |= ButtonBits::L3; }
                if (pad.rightThumbstickButton.pressed){ buttons |= ButtonBits::R3; }
                if (pad.leftShoulder.pressed)         { buttons |= ButtonBits::L1; }
                if (pad.rightShoulder.pressed)        { buttons |= ButtonBits::R1; }
                if (pad.buttonA.pressed)              { buttons |= ButtonBits::A; }  // cross
                if (pad.buttonB.pressed)              { buttons |= ButtonBits::B; }  // circle
                if (pad.buttonX.pressed)              { buttons |= ButtonBits::X; }  // square
                if (pad.buttonY.pressed)              { buttons |= ButtonBits::Y; }  // triangle

                m_rawGamepadState.m_digitalButtonStates = buttons;
                m_rawGamepadState.m_triggerButtonLState  = pad.leftTrigger.value;
                m_rawGamepadState.m_triggerButtonRState  = pad.rightTrigger.value;
                m_rawGamepadState.m_thumbStickLeftXState  = pad.leftThumbstick.xAxis.value;
                m_rawGamepadState.m_thumbStickLeftYState  = pad.leftThumbstick.yAxis.value;
                m_rawGamepadState.m_thumbStickRightXState = pad.rightThumbstick.xAxis.value;
                m_rawGamepadState.m_thumbStickRightYState = pad.rightThumbstick.yAxis.value;

                // Weapon-mode fire-edge detection (Phase 2.5, Task 2). pad.leftTrigger/
                // pad.rightTrigger are declared by the SDK as GCDualSenseAdaptiveTrigger*
                // (GCDualSenseGamepad.h, see the downcast comment in SetTriggerEffect below),
                // not the base GCControllerButtonInput* of GCExtendedGamepad, so no further
                // cast is needed before handing them to ProcessWeaponFireEdge.
                ProcessWeaponFireEdge(
                    (__bridge void*)pad.leftTrigger, Trigger::L2, m_leftPrevWeaponStatus, m_leftAutoRecoil);
                ProcessWeaponFireEdge(
                    (__bridge void*)pad.rightTrigger, Trigger::R2, m_rightPrevWeaponStatus, m_rightAutoRecoil);
            }
            else
            {
                // The pad went away transiently (e.g. briefly nil during a Bluetooth
                // reconnect blip) while the GCController itself is still around. Zero the
                // raw state before the unconditional ProcessRawGamepadState call below so a
                // stale button/trigger/stick value from the last good tick isn't replayed
                // forever as a "stuck" input until the pad comes back.
                m_rawGamepadState.m_digitalButtonStates = 0;
                m_rawGamepadState.m_triggerButtonLState = 0.0f;
                m_rawGamepadState.m_triggerButtonRState = 0.0f;
                m_rawGamepadState.m_thumbStickLeftXState = 0.0f;
                m_rawGamepadState.m_thumbStickLeftYState = 0.0f;
                m_rawGamepadState.m_thumbStickRightXState = 0.0f;
                m_rawGamepadState.m_thumbStickRightYState = 0.0f;

                // The pad is gone, so its Weapon-mode status can no longer be observed --
                // reset previous-state so a reconnect starts clean (see the member comment in
                // the header) instead of comparing fresh reads against stale state.
                m_leftPrevWeaponStatus = WeaponTriggerStatus::Unknown;
                m_rightPrevWeaponStatus = WeaponTriggerStatus::Unknown;
            }
        }
        else
        {
            // Below @available(macOS 11.3, *): the pad can never be read on this OS version,
            // so zero the state for the same stuck-input reason as above.
            m_rawGamepadState.m_digitalButtonStates = 0;
            m_rawGamepadState.m_triggerButtonLState = 0.0f;
            m_rawGamepadState.m_triggerButtonRState = 0.0f;
            m_rawGamepadState.m_thumbStickLeftXState = 0.0f;
            m_rawGamepadState.m_thumbStickLeftYState = 0.0f;
            m_rawGamepadState.m_thumbStickRightXState = 0.0f;
            m_rawGamepadState.m_thumbStickRightYState = 0.0f;
            m_leftPrevWeaponStatus = WeaponTriggerStatus::Unknown;
            m_rightPrevWeaponStatus = WeaponTriggerStatus::Unknown;
        }
        ProcessRawGamepadState(m_rawGamepadState);
    }

    void InputDeviceGamepadDualSenseMac::SetTriggerEffect(Trigger trigger, const TriggerEffect& effect)
    {
        if (@available(macOS 11.3, *))
        {
            // `m_controller.extendedGamepad` is statically a GCExtendedGamepad*; the
            // isKindOfClass check below is what actually proves the runtime object is a
            // GCDualSenseGamepad, making this C-style downcast safe. Once that check
            // passes, `pad.leftTrigger`/`pad.rightTrigger` are declared by the SDK as
            // GCDualSenseAdaptiveTrigger* (GCDualSenseGamepad.h), not the base
            // GCControllerButtonInput* of GCExtendedGamepad, so no further cast is needed
            // to pass them into ApplyEffectToTrigger below.
            // The whole resolution block is @try/@catch-guarded, consistent with the rest of
            // this file's dead-controller hardening: a controller that has gone away can throw
            // on any of these ObjC sends, not just inside ApplyEffectToTrigger.
            GCDualSenseGamepad* pad = nil;
            @try
            {
                pad = (GCDualSenseGamepad*)((__bridge GCController*)m_controller).extendedGamepad;
                if (![pad isKindOfClass:[GCDualSenseGamepad class]])
                {
                    return;
                }
            }
            @catch (NSException* exception)
            {
                AZLOG_DEBUG("DualSense: pad resolution threw on a dead controller, ignoring: %s",
                            exception.reason ? exception.reason.UTF8String : "unknown");
                return;
            }

            TriggerEffect resolved = effect.Clamped();
            bool needsExtended = RequiresExtendedTriggerApi(resolved.m_mode);
            if (needsExtended)
            {
                if (@available(macOS 12.3, *))
                {
                    // Extended API is available on this OS; keep the effect as-is.
                }
                else
                {
                    resolved = DegradeToBaselineApi(resolved);
                    AZLOG_DEBUG("DualSense: degraded trigger effect mode %u to baseline API (macOS < 12.3)",
                                static_cast<AZ::u32>(effect.m_mode));
                }
            }

            void* leftTrigger = nullptr;
            void* rightTrigger = nullptr;
            @try
            {
                leftTrigger = (__bridge void*)pad.leftTrigger;
                rightTrigger = (__bridge void*)pad.rightTrigger;
            }
            @catch (NSException* exception)
            {
                AZLOG_DEBUG("DualSense: trigger property read threw on a dead controller, ignoring: %s",
                            exception.reason ? exception.reason.UTF8String : "unknown");
                return;
            }

            if (trigger == Trigger::L2 || trigger == Trigger::Both)
            {
                ApplyEffectToTrigger(leftTrigger, resolved);
            }
            if (trigger == Trigger::R2 || trigger == Trigger::Both)
            {
                ApplyEffectToTrigger(rightTrigger, resolved);
            }
        }
    }

    void InputDeviceGamepadDualSenseMac::ClearTriggerEffects()
    {
        SetTriggerEffect(Trigger::Both, TriggerEffect{});
    }

    void InputDeviceGamepadDualSenseMac::PlayHapticPulse(float leftIntensity, float rightIntensity, float sharpness)
    {
        if (m_haptics)
        {
            m_haptics->PlayTransientPulse(leftIntensity, rightIntensity, sharpness);
        }
    }

    void InputDeviceGamepadDualSenseMac::SetAutoRecoil(Trigger trigger, bool enabled, float intensity, float sharpness)
    {
        // Plain config storage; ProcessWeaponFireEdge below (Phase 2.5, Task 2) is what
        // consumes it to fire PlayTransientPulse automatically on a Weapon-mode fire edge. No
        // hardware calls here, so there is nothing to guard/teardown.
        const AutoRecoilConfig config{ enabled, intensity, sharpness };
        if (trigger == Trigger::L2 || trigger == Trigger::Both)
        {
            m_leftAutoRecoil = config;
        }
        if (trigger == Trigger::R2 || trigger == Trigger::Both)
        {
            m_rightAutoRecoil = config;
        }
    }

    void InputDeviceGamepadDualSenseMac::ProcessWeaponFireEdge(
        void* gcAdaptiveTrigger, Trigger trigger, WeaponTriggerStatus& prevStatus, const AutoRecoilConfig& config)
    {
        if (@available(macOS 11.3, *))
        {
            WeaponTriggerStatus current = WeaponTriggerStatus::Unknown;
            if (gcAdaptiveTrigger)
            {
                // See the downcast comment in SetTriggerEffect above: the caller only ever
                // passes pad.leftTrigger/pad.rightTrigger from a GCDualSenseGamepad-gated pad,
                // so this __bridge cast back from the opaque void* to the concrete
                // GCDualSenseAdaptiveTrigger* is safe.
                GCDualSenseAdaptiveTrigger* adaptiveTrigger = (__bridge GCDualSenseAdaptiveTrigger*)gcAdaptiveTrigger;
                @try
                {
                    current = MapWeaponTriggerStatus(adaptiveTrigger.status);
                }
                @catch (NSException* exception)
                {
                    AZLOG_DEBUG("DualSense: adaptive trigger status read threw on a dead controller, ignoring: %s",
                                exception.reason ? exception.reason.UTF8String : "unknown");
                    current = WeaponTriggerStatus::Unknown;
                }
            }

            const bool fireEdge = IsWeaponFireEdge(prevStatus, current);
            prevStatus = current;

            if (fireEdge)
            {
                DualSenseTriggerNotificationBus::Event(
                    AzFramework::InputDeviceGamepad::IdForIndexN(GetInputDeviceIndex()),
                    &DualSenseTriggerNotifications::OnWeaponTriggerFired,
                    trigger);

                if (config.m_enabled && m_haptics)
                {
                    // Bias the transient pulse to the side of the trigger that fired: the
                    // firing side gets the configured intensity, the other side gets 0 (which
                    // PlayTransientPulse/PlayTransientOnSide already treat as "skip this side").
                    const bool isLeft = (trigger == Trigger::L2);
                    m_haptics->PlayTransientPulse(
                        isLeft ? config.m_intensity : 0.0f,
                        isLeft ? 0.0f : config.m_intensity,
                        config.m_sharpness);
                }
            }
        }
    }

    void InputDeviceGamepadDualSenseMac::ApplyEffectToTrigger(void* gcAdaptiveTrigger, const TriggerEffect& clamped)
    {
        if (@available(macOS 11.3, *))
        {
            if (!gcAdaptiveTrigger)
            {
                return;
            }
            // See the downcast comment in SetTriggerEffect above: the caller only ever
            // passes pad.leftTrigger/pad.rightTrigger from a GCDualSenseGamepad-gated
            // pad, so this __bridge cast back from the opaque void* to the concrete
            // GCDualSenseAdaptiveTrigger* is safe.
            GCDualSenseAdaptiveTrigger* trigger = (__bridge GCDualSenseAdaptiveTrigger*)gcAdaptiveTrigger;

            switch (clamped.m_mode)
            {
            case TriggerEffectMode::Off:
                @try
                {
                    [trigger setModeOff];
                }
                @catch (NSException* exception)
                {
                    AZLOG_DEBUG("DualSense: setModeOff threw on a dead controller, ignoring: %s",
                                exception.reason ? exception.reason.UTF8String : "unknown");
                }
                break;

            case TriggerEffectMode::Feedback:
                @try
                {
                    [trigger setModeFeedbackWithStartPosition:clamped.m_startPosition
                                             resistiveStrength:clamped.m_strength];
                }
                @catch (NSException* exception)
                {
                    AZLOG_DEBUG("DualSense: setModeFeedbackWithStartPosition:resistiveStrength: threw on a dead controller, ignoring: %s",
                                exception.reason ? exception.reason.UTF8String : "unknown");
                }
                break;

            case TriggerEffectMode::Weapon:
                @try
                {
                    [trigger setModeWeaponWithStartPosition:clamped.m_startPosition
                                                 endPosition:clamped.m_endPosition
                                           resistiveStrength:clamped.m_strength];
                }
                @catch (NSException* exception)
                {
                    AZLOG_DEBUG("DualSense: setModeWeaponWithStartPosition:endPosition:resistiveStrength: threw on a dead controller, ignoring: %s",
                                exception.reason ? exception.reason.UTF8String : "unknown");
                }
                break;

            case TriggerEffectMode::Vibration:
                @try
                {
                    [trigger setModeVibrationWithStartPosition:clamped.m_startPosition
                                                      amplitude:clamped.m_strength
                                                      frequency:clamped.m_frequency];
                }
                @catch (NSException* exception)
                {
                    AZLOG_DEBUG("DualSense: setModeVibrationWithStartPosition:amplitude:frequency: threw on a dead controller, ignoring: %s",
                                exception.reason ? exception.reason.UTF8String : "unknown");
                }
                break;

            case TriggerEffectMode::SlopeFeedback:
                // Intentional belt-and-suspenders: degradation already prevents these modes
                // below 12.3 (see DegradeToBaselineApi/RequiresExtendedTriggerApi above).
                if (@available(macOS 12.3, *))
                {
                    @try
                    {
                        [trigger setModeSlopeFeedbackWithStartPosition:clamped.m_startPosition
                                                            endPosition:clamped.m_endPosition
                                                          startStrength:clamped.m_strength
                                                            endStrength:clamped.m_endStrength];
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: setModeSlopeFeedbackWithStartPosition:... threw on a dead controller, ignoring: %s",
                                    exception.reason ? exception.reason.UTF8String : "unknown");
                    }
                }
                break;

            case TriggerEffectMode::MultiPositionFeedback:
                // Intentional belt-and-suspenders: degradation already prevents these modes
                // below 12.3 (see DegradeToBaselineApi/RequiresExtendedTriggerApi above).
                if (@available(macOS 12.3, *))
                {
                    GCDualSenseAdaptiveTriggerPositionalResistiveStrengths strengths;
                    for (int i = 0; i < 10; ++i)
                    {
                        strengths.values[i] = clamped.m_positionalValues[i];
                    }
                    @try
                    {
                        [trigger setModeFeedbackWithResistiveStrengths:strengths];
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: setModeFeedbackWithResistiveStrengths: threw on a dead controller, ignoring: %s",
                                    exception.reason ? exception.reason.UTF8String : "unknown");
                    }
                }
                break;

            case TriggerEffectMode::MultiPositionVibration:
                // Intentional belt-and-suspenders: degradation already prevents these modes
                // below 12.3 (see DegradeToBaselineApi/RequiresExtendedTriggerApi above).
                if (@available(macOS 12.3, *))
                {
                    GCDualSenseAdaptiveTriggerPositionalAmplitudes amplitudes;
                    for (int i = 0; i < 10; ++i)
                    {
                        amplitudes.values[i] = clamped.m_positionalValues[i];
                    }
                    @try
                    {
                        [trigger setModeVibrationWithAmplitudes:amplitudes frequency:clamped.m_frequency];
                    }
                    @catch (NSException* exception)
                    {
                        AZLOG_DEBUG("DualSense: setModeVibrationWithAmplitudes:frequency: threw on a dead controller, ignoring: %s",
                                    exception.reason ? exception.reason.UTF8String : "unknown");
                    }
                }
                break;

            default:
                break;
            }
        }
    }

    AZStd::unique_ptr<AzFramework::InputDeviceGamepad::Implementation> DualSenseMacGamepadImplFactory::Create(
        AzFramework::InputDeviceGamepad& inputDevice)
    {
        if (m_pendingController == nullptr)
        {
            AZLOG_WARN("DualSense: Mac factory invoked with no pending controller");
            return nullptr;
        }
        return AZStd::make_unique<InputDeviceGamepadDualSenseMac>(inputDevice, m_pendingController);
    }
} // namespace DualSense
