#include "Audio/SmartBalance.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace rr
{
SmartBalance::SmartBalance (ParamStore& parameters)
    : params (parameters)
{
    prepare (48000.0);
}

float SmartBalance::timeToCoefficient (float milliseconds, double sr) noexcept
{
    const auto seconds = juce::jmax (0.0005, (double) milliseconds / 1000.0);
    return (float) std::exp (-1.0 / (seconds * sr));
}

void SmartBalance::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    vocalAttackCoefficient = timeToCoefficient (4.0f, sampleRate);
    vocalReleaseCoefficient = timeToCoefficient (120.0f, sampleRate);
    musicAttackCoefficient = timeToCoefficient (5.0f, sampleRate);
    musicReleaseCoefficient = timeToCoefficient (180.0f, sampleRate);
    duckAttackCoefficient = timeToCoefficient (params.get (ParamId::sbAttackMs), sampleRate);
    duckReleaseCoefficient = timeToCoefficient (params.get (ParamId::sbReleaseMs), sampleRate);
    reset();
}

void SmartBalance::reset() noexcept
{
    vocalEnvelope = 0.0f;
    musicEnvelope = 0.0f;
    gainDb = 0.0f;
    publishedGainReductionDb.store (0.0f, std::memory_order_relaxed);
}

void SmartBalance::process (float* musicLeft, float* musicRight, const float* vocalMono, int numSamples) noexcept
{
    if (musicLeft == nullptr || numSamples <= 0)
        return;

    const bool enabled = params.getBool (ParamId::sbEnabled);
    const auto targetDb = params.get (ParamId::sbTargetDb);
    const auto maxDuck = juce::jmin (params.get (ParamId::sbDuckDb), params.get (ParamId::sbMaxReductionDb));
    const auto strength = params.get (ParamId::sbStrength) * 0.01f;
    duckAttackCoefficient = timeToCoefficient (params.get (ParamId::sbAttackMs), sampleRate);
    duckReleaseCoefficient = timeToCoefficient (params.get (ParamId::sbReleaseMs), sampleRate);

    float maxReduction = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto vocalSample = vocalMono != nullptr ? std::abs (vocalMono[i]) : 0.0f;
        const auto musicSample = juce::jmax (std::abs (musicLeft[i]),
                                             musicRight != nullptr ? std::abs (musicRight[i]) : 0.0f);

        vocalEnvelope = vocalSample > vocalEnvelope
                          ? vocalSample + (1.0f - vocalAttackCoefficient) * (vocalSample - vocalEnvelope)
                          : vocalSample + vocalReleaseCoefficient * (vocalEnvelope - vocalSample);
        musicEnvelope = musicSample > musicEnvelope
                          ? musicSample + (1.0f - musicAttackCoefficient) * (musicSample - musicEnvelope)
                          : musicSample + musicReleaseCoefficient * (musicEnvelope - musicSample);

        float desiredReduction = 0.0f;

        if (enabled)
        {
            const auto vocalDb = juce::Decibels::gainToDecibels (vocalEnvelope, -100.0f);
            const auto musicDb = juce::Decibels::gainToDecibels (musicEnvelope, -100.0f);
            const auto activity = juce::jlimit (0.0f, 1.0f, (vocalDb + 55.0f) / 25.0f);

            if (activity > 0.01f && musicDb > -70.0f)
            {
                const auto excess = musicDb - (vocalDb - targetDb);
                desiredReduction = juce::jlimit (0.0f, maxDuck, excess) * strength * activity;
            }
        }

        const auto targetGainDb = -desiredReduction;

        if (targetGainDb < gainDb)
            gainDb = targetGainDb + duckAttackCoefficient * (gainDb - targetGainDb);
        else
            gainDb = targetGainDb + duckReleaseCoefficient * (gainDb - targetGainDb);

        gainDb = juce::jlimit (-36.0f, 0.0f, gainDb);
        const auto gain = juce::Decibels::decibelsToGain (gainDb);
        musicLeft[i] *= gain;

        if (musicRight != nullptr)
            musicRight[i] *= gain;

        maxReduction = juce::jmax (maxReduction, -gainDb);
    }

    publishedGainReductionDb.store (maxReduction, std::memory_order_relaxed);
}

} // namespace rr
