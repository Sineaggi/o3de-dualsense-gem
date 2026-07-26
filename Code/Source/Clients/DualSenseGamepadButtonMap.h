#pragma once

#include <AzFramework/Input/Devices/Gamepad/InputDeviceGamepad.h>

namespace DualSense
{
    //! Bit assignments for the gem's platform-agnostic digital button state.
    //! Shared by the debug implementation (Phase 0), the Mac backend (Phase 1),
    //! and the raw-HID backends (Phase 3+).
    namespace ButtonBits
    {
        inline constexpr AZ::u32 DPadUp    = 1u << 0;
        inline constexpr AZ::u32 DPadDown  = 1u << 1;
        inline constexpr AZ::u32 DPadLeft  = 1u << 2;
        inline constexpr AZ::u32 DPadRight = 1u << 3;
        inline constexpr AZ::u32 Start     = 1u << 4;  // menu / options-cluster right
        inline constexpr AZ::u32 Select    = 1u << 5;  // create / options-cluster left
        inline constexpr AZ::u32 L3        = 1u << 6;
        inline constexpr AZ::u32 R3        = 1u << 7;
        inline constexpr AZ::u32 L1        = 1u << 8;
        inline constexpr AZ::u32 R1        = 1u << 9;
        inline constexpr AZ::u32 A         = 1u << 12; // cross
        inline constexpr AZ::u32 B         = 1u << 13; // circle
        inline constexpr AZ::u32 X         = 1u << 14; // square
        inline constexpr AZ::u32 Y         = 1u << 15; // triangle
    } // namespace ButtonBits

    //! Maps ButtonBits to the 14 standard gamepad digital button channels.
    const AzFramework::InputDeviceGamepad::Implementation::DigitalButtonIdByBitMaskMap&
        GetDualSenseDigitalButtonMap();
} // namespace DualSense
