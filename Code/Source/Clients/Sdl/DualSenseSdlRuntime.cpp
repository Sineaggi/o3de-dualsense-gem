#include "DualSenseSdlRuntime.h"

#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <AzCore/Console/ILogger.h>

#if defined(__APPLE__)
// NOTE: the task brief's assumed header (IOKit/hid/IOHIDLib.h) does not declare
// IOHIDCheckAccess/kIOHIDRequestTypeListenEvent/kIOHIDAccessTypeGranted -- verified by grepping
// the actual macOS SDK (Xcode.app's MacOSX.sdk IOKit.framework/Headers tree): those symbols live
// in IOKit/hidsystem/IOHIDLib.h instead (confirmed the hard way: the plain hid/IOHIDLib.h include
// compiled but left kIOHIDRequestTypeListenEvent/kIOHIDAccessTypeGranted as undeclared
// identifiers -- a real build-red, fixed by switching the include, not by declaring the symbols
// by hand).
#include <IOKit/hidsystem/IOHIDLib.h>
#endif

namespace DualSense
{
    namespace
    {
        //! Transport byte values per the porting guide's GUID-first-byte trick.
        constexpr AZ::u8 GuidBusByteUsb = 0x03;
        constexpr AZ::u8 GuidBusByteBluetooth = 0x05;

        const char* TransportLabelForGuidFirstByte(AZ::u8 busByte)
        {
            switch (busByte)
            {
            case GuidBusByteUsb:
                return "USB";
            case GuidBusByteBluetooth:
                return "Bluetooth";
            default:
                return "unknown";
            }
        }
    } // namespace

    DualSenseSdlRuntime::~DualSenseSdlRuntime()
    {
        Deactivate();
    }

    bool DualSenseSdlRuntime::Activate()
    {
        if (m_active)
        {
            return true;
        }

#if defined(__APPLE__)
        // BT-readiness addendum ("3b"): macOS's TCC privacy subsystem gates raw HID access under
        // "Input Monitoring" -- a permission the native GameController.framework backend never
        // needs (it does not talk HID directly) but that SDL3's HIDAPI-based PS5 driver does.
        // IOHIDCheckAccess ONLY reads the current grant state; it never itself triggers the
        // permission prompt (macOS only prompts the first time this process actually opens a
        // matching HID device -- that happens deeper in SDL3's own hidapi_darwin backend, not
        // here). Checked unconditionally on every Activate() (cheap; this function already only
        // runs once per activation thanks to the m_active early-return above), so re-activating
        // the sdl backend after switching away and back re-checks in case the user granted access
        // mid-session and relaunched, or the OS revoked it.
        //
        // Availability: IOHIDCheckAccess ships since macOS 10.15. This repo's actual floor is
        // 11.3 (see the adaptive-trigger/CoreHaptics macOS-version gates already in the native
        // backend, and DualSense.gem.json's platform requirements) -- above 10.15 with no gap, so
        // a plain unguarded call is correct here; no @available/__builtin_available needed.
        m_inputMonitoringAccessGranted = IOHIDCheckAccess(kIOHIDRequestTypeListenEvent) == kIOHIDAccessTypeGranted;
        if (!m_inputMonitoringAccessGranted)
        {
            AZLOG_WARN(
                "DualSense (SDL): Input Monitoring permission has not been granted to this app. Grant it under "
                "System Settings > Privacy & Security > Input Monitoring (add/enable the Editor or this "
                "app), then relaunch. The SDL backend's HID access requires this permission; the native "
                "GameController backend (dualsense_backend native) does not.");
        }
#endif // defined(__APPLE__)

        // Hint names verified against the fetched SDL3 3.4.12 headers
        // (_deps/sdl3-src/include/SDL3/SDL_hints.h in the engine build tree). The task brief's
        // assumed name (SDL_HINT_JOYSTICK_HIDAPI_PS5) matched exactly, no correction needed.
        //
        // Explicitly force the PS5 HIDAPI driver on (it may default off if the umbrella
        // SDL_HINT_JOYSTICK_HIDAPI hint was ever turned off by the embedding application/OS
        // environment) and pin off the sibling console-vendor HIDAPI driver families this gem
        // will never use (PS3/PS4/Xbox/Switch) -- this gem only ever wants to detect and drive a
        // DualSense, so there is no reason to pay the enumeration/probe cost of those drivers.
        // Left at SDL's own default: the long tail of third-party-pad hints (8BitDo, SInput,
        // Zuiki, Flydigi, LG4FF, Steam, GameCube, Wii, Luna, Stadia, Shield, ...) -- none of them
        // can ever match a DualSense's vendor/product id, so disabling them buys nothing beyond
        // what's already covered by the four families above, at the cost of a much longer list
        // to keep in sync with future SDL releases.
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "0");

        // SDL3's SDL_Init returns bool (true = success) -- a genuine shape difference from the
        // int/0-success convention the porting guide's SDL2-era prose might suggest; verified
        // directly against the fetched SDL3 3.4.12 header (SDL_init.h declares
        // `extern SDL_DECLSPEC bool SDLCALL SDL_Init(SDL_InitFlags flags);`).
        if (!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_SENSOR))
        {
            AZLOG_WARN("DualSense (SDL): SDL_Init(JOYSTICK|SENSOR) failed: %s", SDL_GetError());
            return false;
        }

        m_active = true;
        AZLOG_INFO("DualSense (SDL): backend activated (SDL_Init JOYSTICK|SENSOR)");
        return true;
    }

    void DualSenseSdlRuntime::Deactivate()
    {
        if (!m_active)
        {
            return;
        }

        SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_SENSOR);
        // Full SDL_Quit(), not just the QuitSubSystem above: this gem is SDL's sole owner while
        // the sdl backend is selected (see the class header comment), so there is nothing else
        // in-process relying on any other SDL subsystem staying initialized.
        SDL_Quit();
        m_active = false;
        AZLOG_INFO("DualSense (SDL): backend deactivated");
    }

    void DualSenseSdlRuntime::PumpEvents()
    {
        if (!SDL_WasInit(SDL_INIT_JOYSTICK))
        {
            return;
        }
        SDL_UpdateJoysticks();
    }

    AZStd::vector<SDL_JoystickID> DualSenseSdlRuntime::EnumeratePs5Joysticks() const
    {
        AZStd::vector<SDL_JoystickID> result;
        if (!SDL_WasInit(SDL_INIT_JOYSTICK))
        {
            return result;
        }

        int count = 0;
        SDL_JoystickID* ids = SDL_GetJoysticks(&count);
        if (ids)
        {
            for (int i = 0; i < count; ++i)
            {
                if (SDL_GetGamepadTypeForID(ids[i]) == SDL_GAMEPAD_TYPE_PS5)
                {
                    result.push_back(ids[i]);
                }
            }
            SDL_free(ids);
        }
        return result;
    }

    void DualSenseSdlRuntime::LogTransport(SDL_JoystickID id)
    {
        // Defensive WasInit guard, consistent with every other SDL call in this file, even
        // though today's only caller (DualSenseSdlMonitor::OnJoystickConnected) is itself only
        // reachable for ids returned by EnumeratePs5Joysticks(), which already implies WasInit.
        if (!SDL_WasInit(SDL_INIT_JOYSTICK))
        {
            return;
        }
        const SDL_GUID guid = SDL_GetJoystickGUIDForID(id);
        const AZ::u8 busByte = guid.data[0];
        AZLOG_INFO(
            "DualSense (SDL): joystick %u transport byte 0x%02x (%s)", id, busByte, TransportLabelForGuidFirstByte(busByte));
    }
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
