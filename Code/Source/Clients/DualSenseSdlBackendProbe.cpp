#include "DualSenseSdlBackendProbe.h"

#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <SDL3/SDL.h>

namespace DualSense
{
    int GetLinkedSdlVersion()
    {
        // No SDL_Init call here or anywhere else in Phase 3a Task 1 -- SDL_GetVersion() needs none.
        return SDL_GetVersion();
    }

    bool IsLinkedSdlVersionAtLeast(int major, int minor, int micro)
    {
        return GetLinkedSdlVersion() >= SDL_VERSIONNUM(major, minor, micro);
    }
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
