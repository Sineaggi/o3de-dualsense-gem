#pragma once

// Phase 3a Task 1 proof-of-plumbing only: confirms SDL3 was fetched, built, and can be linked
// and called behind PAL_TRAIT_DUALSENSE_SDL_BACKEND, without touching any runtime behavior.
// Nothing in this gem calls SDL_Init anywhere -- Task 3 owns actual backend selection and
// subsystem initialization (driven by the dualsense_backend cvar in DualSenseSystemComponent.cpp).
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

namespace DualSense
{
    //! Returns the linked SDL3 version as SDL3's own packed integer (SDL_VERSIONNUM(major, minor, micro),
    //! i.e. major*1000000 + minor*1000 + micro). Calls only SDL_GetVersion(), which (unlike SDL2's
    //! out-param SDL_GetVersion(SDL_version*)) takes no arguments and requires no SDL_Init.
    //! Declared without including <SDL3/SDL.h> so callers (e.g. unit tests) don't need SDL3's include
    //! path -- only Private.Object (which links 3rdParty::SDL3) needs that, in the .cpp.
    int GetLinkedSdlVersion();

    //! Convenience wrapper so callers can assert a minimum linked SDL3 version without needing
    //! SDL3's SDL_VERSIONNUM macro (and therefore without needing <SDL3/SDL.h>) themselves.
    bool IsLinkedSdlVersionAtLeast(int major, int minor, int micro);
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
