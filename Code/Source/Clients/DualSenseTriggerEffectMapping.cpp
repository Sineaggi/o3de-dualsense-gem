#include <DualSense/DualSenseTriggerEffectMapping.h>

namespace DualSense
{
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
        TriggerEffect degraded = effect;

        switch (effect.m_mode)
        {
        case TriggerEffectMode::MultiPositionFeedback:
        {
            // Find the index of the first value > 0
            int firstNonZeroIndex = -1;
            for (int i = 0; i < static_cast<int>(effect.m_positionalValues.size()); ++i)
            {
                if (effect.m_positionalValues[i] > 0.0f)
                {
                    firstNonZeroIndex = i;
                    break;
                }
            }

            // Convert to Feedback mode
            degraded.m_mode = TriggerEffectMode::Feedback;

            // Set start position: (index of first value > 0) / 9.0f, or 1.0 if none
            if (firstNonZeroIndex >= 0)
            {
                degraded.m_startPosition = static_cast<float>(firstNonZeroIndex) / 9.0f;
            }
            else
            {
                degraded.m_startPosition = 1.0f;
            }

            // Set strength to max of all values
            float maxValue = 0.0f;
            for (const auto& value : effect.m_positionalValues)
            {
                if (value > maxValue)
                {
                    maxValue = value;
                }
            }
            degraded.m_strength = maxValue;
            break;
        }

        case TriggerEffectMode::MultiPositionVibration:
        {
            // Find the index of the first value > 0
            int firstNonZeroIndex = -1;
            for (int i = 0; i < static_cast<int>(effect.m_positionalValues.size()); ++i)
            {
                if (effect.m_positionalValues[i] > 0.0f)
                {
                    firstNonZeroIndex = i;
                    break;
                }
            }

            // Convert to Vibration mode
            degraded.m_mode = TriggerEffectMode::Vibration;

            // Set start position: (index of first value > 0) / 9.0f, or 1.0 if none
            if (firstNonZeroIndex >= 0)
            {
                degraded.m_startPosition = static_cast<float>(firstNonZeroIndex) / 9.0f;
            }
            else
            {
                degraded.m_startPosition = 1.0f;
            }

            // Set strength to max of all values
            float maxValue = 0.0f;
            for (const auto& value : effect.m_positionalValues)
            {
                if (value > maxValue)
                {
                    maxValue = value;
                }
            }
            degraded.m_strength = maxValue;

            // Preserve frequency
            degraded.m_frequency = effect.m_frequency;
            break;
        }

        case TriggerEffectMode::SlopeFeedback:
        {
            // Convert to Feedback mode
            degraded.m_mode = TriggerEffectMode::Feedback;

            // Preserve start position
            degraded.m_startPosition = effect.m_startPosition;

            // Set strength to average of m_strength and m_endStrength
            degraded.m_strength = (effect.m_strength + effect.m_endStrength) / 2.0f;
            break;
        }

        case TriggerEffectMode::Off:
        case TriggerEffectMode::Feedback:
        case TriggerEffectMode::Weapon:
        case TriggerEffectMode::Vibration:
        default:
        {
            // Baseline modes pass through unchanged (after clamping)
            break;
        }
        }

        // Output of degradation is always itself Clamped()
        return degraded.Clamped();
    }

} // namespace DualSense
