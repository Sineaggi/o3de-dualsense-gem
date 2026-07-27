#pragma once

// Phase 3a Task 3. Whole-file guarded -- see DualSenseSdlRuntime.h's header comment for why.
//
// Deliberately has NO #include <SDL3/SDL.h> (unlike this directory's other headers): it exists
// specifically so Tests/Clients/DualSenseSdlAxisTests.cpp can include it without pulling in
// SDL3's headers at all. DualSense.Tests only links Gem::DualSense.Private.Object, and
// 3rdParty::SDL3 is a PRIVATE build dependency of THAT target (Code/CMakeLists.txt) -- CMake
// does not propagate a PRIVATE dependency's own INTERFACE_INCLUDE_DIRECTORIES to Private.Object's
// consumers (only DUALSENSE_SDL_BACKEND_ENABLED itself reaches Tests, because that one compile
// definition is deliberately re-declared PUBLIC on Private.Object). This is exactly why
// DualSenseSdlBackendProbe.h (Task 1) also avoids including <SDL3/SDL.h> and why its test file
// says so explicitly -- this header follows the same rule for the same reason. Everything here
// takes/returns plain numeric types (AZ::s16/float) so no SDL type is needed regardless.
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <AzCore/base.h>

namespace DualSense
{
    //! Pure axis-normalization helpers for the SDL3 SDL_Gamepad axis ranges (verified against
    //! the fetched SDL3 3.4.12 SDL_gamepad.h doc comments on SDL_GetGamepadAxis -- NOTE this is
    //! the gamepad-LAYER range, deliberately different from the lower-level SDL_GetJoystickAxis
    //! range a dlsym-only reference would have had to normalize by hand). Used by
    //! InputDeviceGamepadDualSenseSdl::TickInputDevice; unit-tested directly in
    //! Tests/Clients/DualSenseSdlAxisTests.cpp.
    namespace SdlAxisMath
    {
        //! Thumbstick raw range: [-32768, 32767] ("up/left" to "down/right" per SDL's doc
        //! comment). Symmetric mapping -- the negative side divides by 32768, the positive side
        //! by 32767 -- so the two extremes map to exactly -1.0f/1.0f rather than leaving the
        //! positive extreme short of 1.0 (a single-divisor /32768 mapping would do that).
        float NormalizeStickAxis(AZ::s16 raw);

        //! Trigger raw range: [0, 32767], never negative per SDL's own doc comment on
        //! SDL_GetGamepadAxis ("Triggers range from 0 when released to 32767 when fully pressed,
        //! and never return a negative value"). Clamped defensively even though SDL guarantees
        //! the range, consistent with this gem's existing DualSense::TriggerEffect::Clamped()
        //! defensive-clamping style.
        float NormalizeTriggerAxis(AZ::s16 raw);
    } // namespace SdlAxisMath
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
