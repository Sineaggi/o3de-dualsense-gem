#pragma once

#include <DualSense/DualSenseTriggerEffects.h>

namespace DualSense
{
    //! True if the mode needs the macOS 12.3+ / firmware multi-position API surface.
    bool RequiresExtendedTriggerApi(TriggerEffectMode mode);

    //! Deterministic approximation of a 12.3+-only effect using only baseline
    //! (11.3) modes. Baseline modes pass through unchanged (after Clamped()).
    TriggerEffect DegradeToBaselineApi(const TriggerEffect& effect);

} // namespace DualSense
