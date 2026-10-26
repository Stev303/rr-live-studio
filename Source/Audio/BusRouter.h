#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace rr
{
/** Small helper describing the gain law and summing used by the mixer.

    Keeping this logic in one place means the audio engine and the unit tests
    exercise exactly the same routing code.
*/
class BusRouter
{
public:
    struct BusState
    {
        float faderDb = 0.0f;
        bool muted = false;
        bool soloed = false;
    };

    static float gainForBus (const BusState& bus, bool anySoloActive) noexcept
    {
        if (bus.muted)
            return 0.0f;

        if (anySoloActive && ! bus.soloed)
            return 0.0f;

        return juce::Decibels::decibelsToGain (bus.faderDb, -100.0f);
    }

    static void sumInto (float* destinationL, float* destinationR,
                         const float* sourceL, const float* sourceR,
                         int numSamples, float gain) noexcept
    {
        if (gain == 0.0f)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            destinationL[i] += sourceL[i] * gain;

            if (destinationR != nullptr && sourceR != nullptr)
                destinationR[i] += sourceR[i] * gain;
        }
    }

    static void copyToStereoOutput (const float* sourceL, const float* sourceR,
                                    float* destinationL, float* destinationR,
                                    int numSamples, int numOutputChannels) noexcept
    {
        if (sourceL != nullptr && destinationL != nullptr)
            juce::FloatVectorOperations::copy (destinationL, sourceL, numSamples);

        if (numOutputChannels > 1 && destinationR != nullptr)
        {
            if (sourceR != nullptr)
                juce::FloatVectorOperations::copy (destinationR, sourceR, numSamples);
            else if (sourceL != nullptr)
                juce::FloatVectorOperations::copy (destinationR, sourceL, numSamples);
        }
    }
};

} // namespace rr
