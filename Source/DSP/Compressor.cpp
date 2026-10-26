#include "DSP/Compressor.h"

namespace rr::dsp
{
namespace
{
    inline float coefficientForTime (float milliseconds, double sampleRate) noexcept
    {
        const auto seconds = juce::jmax (0.0001, (double) milliseconds / 1000.0);
        return (float) std::exp (-1.0 / (seconds * sampleRate));
    }
}

void Compressor::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    attackCoefficient = coefficientForTime (lastAttackMs, sampleRate);
    releaseCoefficient = coefficientForTime (lastReleaseMs, sampleRate);
    reset();
}

void Compressor::reset() noexcept
{
    envelope = 1.0f;
    lastGainReductionDb = 0.0f;
}

void Compressor::setParameters (float thresholdDb,
                                float ratioValue,
                                float attackMs,
                                float releaseMs,
                                float makeupDb,
                                float kneeDb) noexcept
{
    threshold = juce::jlimit (-90.0f, 0.0f, thresholdDb);
    ratio = juce::jlimit (1.0f, 40.0f, ratioValue);
    knee = juce::jlimit (0.0f, 24.0f, kneeDb);
    makeup = juce::jlimit (0.0f, 8.0f, (float) juce::Decibels::decibelsToGain (makeupDb, -100.0f));
    lastAttackMs = juce::jmax (0.1f, attackMs);
    lastReleaseMs = juce::jmax (1.0f, releaseMs);
    attackCoefficient = coefficientForTime (lastAttackMs, sampleRate);
    releaseCoefficient = coefficientForTime (lastReleaseMs, sampleRate);
}

float Compressor::computeTargetGain (float detectorLevel) const noexcept
{
    if (detectorLevel <= 1.0e-8f)
        return makeup;

    const auto levelDb = juce::Decibels::gainToDecibels (detectorLevel, -120.0f);
    const auto over = levelDb - threshold;
    float gainReductionDb = 0.0f;

    if (knee > 0.0f && over > -knee * 0.5f && over < knee * 0.5f)
    {
        const auto x = over + knee * 0.5f;
        gainReductionDb = (1.0f - 1.0f / ratio) * (x * x) / (2.0f * knee);
    }
    else if (over >= knee * 0.5f)
    {
        gainReductionDb = over * (1.0f - 1.0f / ratio);
    }

    return makeup * juce::Decibels::decibelsToGain (-gainReductionDb);
}

void Compressor::process (float* left, float* right, int numSamples) noexcept
{
    lastGainReductionDb = 0.0f;

    if (! enabled)
    {
        envelope = 1.0f;
        return;
    }

    float* channels[2] = { left, right };
    const auto numChannels = right != nullptr ? 2 : 1;

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
            peak = juce::jmax (peak, std::abs (channels[ch][i]));

        const auto target = computeTargetGain (peak);
        const auto coefficient = target < envelope ? attackCoefficient : releaseCoefficient;
        envelope = coefficient * envelope + (1.0f - coefficient) * target;
        envelope = juce::jlimit (0.0f, 8.0f, envelope);

        const auto gr = -juce::Decibels::gainToDecibels (envelope / juce::jmax (1.0e-6f, makeup), 0.0f);
        lastGainReductionDb = juce::jmax (lastGainReductionDb, gr);

        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = std::isfinite (channels[ch][i]) ? channels[ch][i] * envelope : 0.0f;
    }
}

void Compressor::processMono (float* data, int numSamples) noexcept
{
    process (data, nullptr, numSamples);
}

} // namespace rr::dsp
