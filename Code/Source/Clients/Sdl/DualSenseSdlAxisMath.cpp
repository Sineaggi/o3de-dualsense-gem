#include "DualSenseSdlAxisMath.h"

#if defined(DUALSENSE_SDL_BACKEND_ENABLED)

#include <AzCore/std/algorithm.h>

namespace DualSense
{
    namespace SdlAxisMath
    {
        float NormalizeStickAxis(AZ::s16 raw)
        {
            return raw < 0 ? (static_cast<float>(raw) / 32768.0f) : (static_cast<float>(raw) / 32767.0f);
        }

        float NormalizeTriggerAxis(AZ::s16 raw)
        {
            const float normalized = static_cast<float>(raw) / 32767.0f;
            return AZStd::clamp(normalized, 0.0f, 1.0f);
        }
    } // namespace SdlAxisMath
} // namespace DualSense

#endif // DUALSENSE_SDL_BACKEND_ENABLED
