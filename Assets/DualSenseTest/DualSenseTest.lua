----------------------------------------------------------------------------------------------------
--
-- DualSense hardware test scene (Phase 2.6).
--
-- Keyboard-driven exercise of every scripted DualSense bus. Ports the key map and validation
-- semantics of ~/pong/trigger_test.gd (the reference Godot hardware harness) to O3DE: attach this
-- component, together with an Input Configuration component pointing at
-- dualsense_test.inputbindings, to any entity in any level. See README "Test scene" for setup and
-- the full key legend.
--
-- Keys:
--   1 feedback | 2 weapon | 3 vibration | 4 multifeedback | 5 slope | 6 multivibration
--   7 autofire | 0 off
--   Q/E/W axis LEFT/RIGHT/BOTH (default BOTH)
--   R rumble 1s (coexistence: does the active trigger effect survive?)
--   L cycle lightbar color (coexistence: does the active trigger effect survive?)
--   T haptic tap (both) | Y haptic buzz 1.5s (both) | U haptic stop
--   G haptic tap LEFT only | H haptic buzz RIGHT only 1.0s
--   [ / ] autofire frequency sweep +-0.02 (re-applies autofire at the new frequency, logs it --
--         this is how the repeated-fire feel gets locked empirically against hardware)
--
----------------------------------------------------------------------------------------------------
local inputMultiHandler = require('scripts.utils.components.inpututils')

local DualSenseTest =
{
    Properties =
    {
        GamepadSlot = 0,
    },
}

local LED_COLORS =
{
    { 1.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0 },
    { 0.0, 0.0, 1.0 },
    { 1.0, 1.0, 1.0 },
}
local LED_COLOR_NAMES = { "RED", "GREEN", "BLUE", "WHITE" }

local RUMBLE_DURATION_SECONDS = 1.0

function DualSenseTest:OnActivate()
    self.deviceId = DualSense_GetGamepadDeviceId(self.Properties.GamepadSlot)

    self.axis = DualSenseTrigger_Both
    self.axisName = "BOTH"

    -- Assumed 25 Hz -> normalized mapping (Hz / 255), same starting point as the "autofire" preset
    -- in DualSenseSystemComponent.cpp's CreateTriggerEffectForMode. Unverified on hardware; sweep
    -- with [ / ] to lock in the value that actually feels like the reference's 25 Hz repeated fire.
    self.frequency = 0.098

    self.ledIndex = 0
    self.rumbleTimeRemaining = 0.0
    self.tickHandler = nil

    self.inputHandlers = inputMultiHandler.ConnectMultiHandlers
    {
        [InputEventNotificationId("ds_axis_left")] = { OnPressed = function() self:SetAxis(DualSenseTrigger_L2, "LEFT") end },
        [InputEventNotificationId("ds_axis_right")] = { OnPressed = function() self:SetAxis(DualSenseTrigger_R2, "RIGHT") end },
        [InputEventNotificationId("ds_axis_both")] = { OnPressed = function() self:SetAxis(DualSenseTrigger_Both, "BOTH") end },

        [InputEventNotificationId("ds_mode_feedback")] = { OnPressed = function() self:ApplyFeedback() end },
        [InputEventNotificationId("ds_mode_weapon")] = { OnPressed = function() self:ApplyWeapon() end },
        [InputEventNotificationId("ds_mode_vibration")] = { OnPressed = function() self:ApplyVibration() end },
        [InputEventNotificationId("ds_mode_multifeedback")] = { OnPressed = function() self:ApplyMultiFeedback() end },
        [InputEventNotificationId("ds_mode_slope")] = { OnPressed = function() self:ApplySlope() end },
        [InputEventNotificationId("ds_mode_multivibration")] = { OnPressed = function() self:ApplyMultiVibration() end },
        [InputEventNotificationId("ds_mode_autofire")] = { OnPressed = function() self:ApplyAutofire() end },
        [InputEventNotificationId("ds_mode_off")] = { OnPressed = function() self:ApplyOff() end },

        [InputEventNotificationId("ds_rumble")] = { OnPressed = function() self:Rumble() end },
        [InputEventNotificationId("ds_lightbar")] = { OnPressed = function() self:CycleLightbar() end },

        [InputEventNotificationId("ds_haptic_tap")] = { OnPressed = function() self:HapticTap() end },
        [InputEventNotificationId("ds_haptic_buzz")] = { OnPressed = function() self:HapticBuzz() end },
        [InputEventNotificationId("ds_haptic_stop")] = { OnPressed = function() self:HapticStop() end },
        [InputEventNotificationId("ds_haptic_tap_left")] = { OnPressed = function() self:HapticTapLeft() end },
        [InputEventNotificationId("ds_haptic_buzz_right")] = { OnPressed = function() self:HapticBuzzRight() end },

        [InputEventNotificationId("ds_freq_down")] = { OnPressed = function() self:SweepFrequency(-0.02) end },
        [InputEventNotificationId("ds_freq_up")] = { OnPressed = function() self:SweepFrequency(0.02) end },
    }

    self.fireHandler = DualSenseTriggerNotificationBus.Connect(self, self.deviceId)

    Debug.Log(string.format(
        "DualSenseTest activated: gamepad slot %d, axis=BOTH, autofire frequency=%.3f",
        self.Properties.GamepadSlot, self.frequency))
end

function DualSenseTest:OnDeactivate()
    self.inputHandlers:Disconnect()
    self.inputHandlers = nil

    self.fireHandler:Disconnect()
    self.fireHandler = nil

    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

-- DualSenseTriggerNotificationBus handler: fired on the Weapon-mode trigger-break edge.
function DualSenseTest:OnWeaponTriggerFired(trigger)
    Debug.Log("OnWeaponTriggerFired -> trigger=" .. tostring(trigger))
end

function DualSenseTest:SetAxis(axis, axisName)
    self.axis = axis
    self.axisName = axisName
    Debug.Log("axis=" .. axisName)
end

function DualSenseTest:ApplyEffect(effect, label)
    DualSenseTriggerEffectRequestBus.Event.SetTriggerEffect(self.deviceId, self.axis, effect)
    Debug.Log(label .. " -> applied (axis=" .. self.axisName .. ")")
end

-- Trigger effect modes. Values mirror CreateTriggerEffectForMode in DualSenseSystemComponent.cpp
-- (the dualsense_trigger console command's presets) so the console and scripted paths feel
-- identical for the same mode.

function DualSenseTest:ApplyFeedback()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_Feedback
    effect.startPosition = 0.3
    effect.strength = 0.8
    self:ApplyEffect(effect, "feedback")
end

function DualSenseTest:ApplyWeapon()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_Weapon
    effect.startPosition = 0.2
    effect.endPosition = 0.6
    effect.strength = 0.9
    self:ApplyEffect(effect, "weapon")
end

function DualSenseTest:ApplyVibration()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_Vibration
    effect.startPosition = 0.2
    effect.strength = 0.75
    effect.frequency = 0.6
    self:ApplyEffect(effect, "vibration")
end

function DualSenseTest:ApplyMultiFeedback()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_MultiPositionFeedback
    local values = { 0.0, 0.0, 0.3, 0.3, 0.6, 0.6, 0.9, 0.9, 1.0, 1.0 }
    for i = 1, #values do
        effect.positionalValues:Replace(i, values[i])
    end
    self:ApplyEffect(effect, "multifeedback")
end

function DualSenseTest:ApplySlope()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_SlopeFeedback
    effect.startPosition = 0.2
    effect.endPosition = 0.9
    effect.strength = 0.3
    effect.endStrength = 1.0
    self:ApplyEffect(effect, "slope")
end

function DualSenseTest:ApplyMultiVibration()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_MultiPositionVibration
    local values = { 0.0, 0.5, 0.0, 0.5, 0.0, 0.5, 0.0, 0.5, 0.0, 0.5 }
    for i = 1, #values do
        effect.positionalValues:Replace(i, values[i])
    end
    effect.frequency = 0.5
    self:ApplyEffect(effect, "multivibration")
end

function DualSenseTest:ApplyAutofire()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_Vibration
    effect.startPosition = 0.2
    effect.strength = 0.9
    effect.frequency = self.frequency
    self:ApplyEffect(effect, string.format("autofire (freq=%.3f)", self.frequency))
end

function DualSenseTest:ApplyOff()
    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_Off
    self:ApplyEffect(effect, "off")
end

-- [ / ] sweep the autofire frequency by +-0.02 and immediately re-apply the autofire preset at the
-- new value, regardless of the currently-selected mode -- this is the intended workflow for
-- locking the repeated-fire feel against real hardware (press 7 once, then tap [ / ] while firing).
function DualSenseTest:SweepFrequency(delta)
    local newFrequency = self.frequency + delta
    if newFrequency < 0.0 then
        newFrequency = 0.0
    elseif newFrequency > 1.0 then
        newFrequency = 1.0
    end
    self.frequency = newFrequency

    local effect = DualSenseTriggerEffect()
    effect.mode = DualSenseTriggerEffectMode_Vibration
    effect.startPosition = 0.2
    effect.strength = 0.9
    effect.frequency = self.frequency
    DualSenseTriggerEffectRequestBus.Event.SetTriggerEffect(self.deviceId, self.axis, effect)

    Debug.Log(string.format("frequency sweep -> %.3f (autofire re-applied, axis=%s)", self.frequency, self.axisName))
end

-- Coexistence: rumble runs concurrently with any active trigger effect for RUMBLE_DURATION_SECONDS,
-- then stops itself. Mirrors trigger_test.gd's `Input.start_joy_vibration(d, 0.5, 1.0, 1.0)`.
function DualSenseTest:Rumble()
    DualSense_SetRumble(self.Properties.GamepadSlot, 0.5, 1.0)
    Debug.Log("rumble 1s (check: does the active trigger effect survive?)")

    self.rumbleTimeRemaining = RUMBLE_DURATION_SECONDS
    if self.tickHandler == nil then
        self.tickHandler = TickBus.Connect(self, 0)
    end
end

function DualSenseTest:OnTick(deltaTime, scriptTime)
    self.rumbleTimeRemaining = self.rumbleTimeRemaining - deltaTime
    if self.rumbleTimeRemaining <= 0.0 then
        DualSense_SetRumble(self.Properties.GamepadSlot, 0.0, 0.0)
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function DualSenseTest:CycleLightbar()
    self.ledIndex = (self.ledIndex % #LED_COLORS) + 1
    local color = LED_COLORS[self.ledIndex]
    DualSense_SetLightBar(self.Properties.GamepadSlot, color[1], color[2], color[3])
    Debug.Log("lightbar -> " .. LED_COLOR_NAMES[self.ledIndex] .. " (check: trigger effect survives?)")
end

function DualSenseTest:HapticTap()
    DualSenseHapticPulseRequestBus.Event.PlayHapticPulse(self.deviceId, 0.9, 0.9, 0.7)
    Debug.Log("haptic_tap -> intensity=0.9 sharpness=0.7")
end

function DualSenseTest:HapticBuzz()
    DualSenseHapticPulseRequestBus.Event.PlayHapticBuzz(self.deviceId, 0.6, 0.6, 0.3, 1.5)
    Debug.Log("haptic_buzz 1.5s -> intensity=0.6 sharpness=0.3")
end

function DualSenseTest:HapticStop()
    DualSenseHapticPulseRequestBus.Event.StopHaptics(self.deviceId)
    Debug.Log("haptic_stop")
end

function DualSenseTest:HapticTapLeft()
    DualSenseHapticPulseRequestBus.Event.PlayHapticPulse(self.deviceId, 1.0, 0.0, 0.5)
    Debug.Log("haptic_tap LEFT -> intensity=1.0 sharpness=0.5")
end

function DualSenseTest:HapticBuzzRight()
    DualSenseHapticPulseRequestBus.Event.PlayHapticBuzz(self.deviceId, 0.0, 0.8, 0.8, 1.0)
    Debug.Log("haptic_buzz RIGHT 1.0s -> intensity=0.8 sharpness=0.8")
end

return DualSenseTest
