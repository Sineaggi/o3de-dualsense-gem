#include <DualSense/DualSenseTriggerEffectMapping.h>

namespace DualSense
{
    namespace
    {
        // Helper to find first index with value > 0 and maximum value in clamped positional array.
        struct PositionalScanResult
        {
            int firstNonZeroIndex;
            float maxValue;
        };

        PositionalScanResult ScanPositionalValues(const AZStd::array<float, 10>& clampedValues)
        {
            PositionalScanResult result{-1, 0.0f};
            for (int i = 0; i < static_cast<int>(clampedValues.size()); ++i)
            {
                if (clampedValues[i] > 0.0f && result.firstNonZeroIndex < 0)
                {
                    result.firstNonZeroIndex = i;
                }
                if (clampedValues[i] > result.maxValue)
                {
                    result.maxValue = clampedValues[i];
                }
            }
            return result;
        }
    } // namespace

    bool RequiresExtendedTriggerApi(TriggerEffectMode mode)
    {
        switch (mode)
        {
        case TriggerEffectMode::MultiPositionFeedback:
        case TriggerEffectMode::MultiPositionVibration:
        case TriggerEffectMode::SlopeFeedback:
            return true;
        default:
            return false;
        }
    }

    TriggerEffect DegradeToBaselineApi(const TriggerEffect& effect)
    {
        // Start with clamped effect to ensure all fields, including positionals, are in valid range.
        TriggerEffect working = effect.Clamped();
        TriggerEffect degraded = working;

        switch (effect.m_mode)
        {
        case TriggerEffectMode::MultiPositionFeedback:
        {
            // Scan clamped positionals to find first index > 0 and max value.
            PositionalScanResult scan = ScanPositionalValues(working.m_positionalValues);

            // Convert to Feedback mode
            degraded.m_mode = TriggerEffectMode::Feedback;

            // Set start position: (index of first value > 0) / 9.0f, or 1.0 if none
            if (scan.firstNonZeroIndex >= 0)
            {
                degraded.m_startPosition = static_cast<float>(scan.firstNonZeroIndex) / 9.0f;
            }
            else
            {
                degraded.m_startPosition = 1.0f;
            }

            // Set strength to max of all values
            degraded.m_strength = scan.maxValue;

            // Clear stale positional values array
            degraded.m_positionalValues.fill(0.0f);
            break;
        }

        case TriggerEffectMode::MultiPositionVibration:
        {
            // Scan clamped positionals to find first index > 0 and max value.
            PositionalScanResult scan = ScanPositionalValues(working.m_positionalValues);

            // Convert to Vibration mode
            degraded.m_mode = TriggerEffectMode::Vibration;

            // Set start position: (index of first value > 0) / 9.0f, or 1.0 if none
            if (scan.firstNonZeroIndex >= 0)
            {
                degraded.m_startPosition = static_cast<float>(scan.firstNonZeroIndex) / 9.0f;
            }
            else
            {
                degraded.m_startPosition = 1.0f;
            }

            // Set strength to max of all values
            degraded.m_strength = scan.maxValue;

            // Preserve frequency
            degraded.m_frequency = working.m_frequency;

            // Clear stale positional values array
            degraded.m_positionalValues.fill(0.0f);
            break;
        }

        case TriggerEffectMode::SlopeFeedback:
        {
            // Convert to Feedback mode
            degraded.m_mode = TriggerEffectMode::Feedback;

            // Preserve start position
            degraded.m_startPosition = working.m_startPosition;

            // Set strength to average of m_strength and m_endStrength
            degraded.m_strength = (working.m_strength + working.m_endStrength) / 2.0f;
            break;
        }

        case TriggerEffectMode::Off:
        case TriggerEffectMode::Feedback:
        case TriggerEffectMode::Weapon:
        case TriggerEffectMode::Vibration:
        default:
        {
            // Baseline modes pass through unchanged
            break;
        }
        }

        // Output of degradation is always itself Clamped() (safety net, typically no-op at this point)
        return degraded.Clamped();
    }

} // namespace DualSense
