// Phase 3a Task 1 proof-of-plumbing: exercises the SDL3 fetch/link/flag path end-to-end without
// touching runtime behavior. Only exists (and only compiles) when DUALSENSE_SDL_BACKEND_ENABLED is
// defined, i.e. only on platforms where PAL_TRAIT_DUALSENSE_SDL_BACKEND is TRUE. Deliberately does
// NOT include <SDL3/SDL.h> itself -- it only calls the gem-internal probe functions, which is all
// this task needs to prove fetch+link+flag plumbing.
#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <AzCore/UnitTest/TestTypes.h>
#include <Clients/DualSenseSdlBackendProbe.h>

namespace DualSenseTests
{
    using SdlBackendProbeFixture = UnitTest::LeakDetectionFixture;

    TEST_F(SdlBackendProbeFixture, GetLinkedSdlVersion_ReturnsAtLeastSdl3_0_0)
    {
        EXPECT_TRUE(DualSense::IsLinkedSdlVersionAtLeast(3, 0, 0));
    }
} // namespace DualSenseTests

#endif // DUALSENSE_SDL_BACKEND_ENABLED
