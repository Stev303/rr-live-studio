#include "DSP/Limiter.h"

namespace rr::dsp
{
void Limiter::prepare (double newSampleRate, int maximumBlockSize, bool useTruePeakDetector)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    preparedBlockSize = juce::jmax (1, maximumBlockSize);
    truePeakEnabled = useTruePeakDetector;
    targetGains.assign ((size_t) preparedBlockSize, 1.0f);
    rebuildDelay();

    if (truePeakEnabled)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, 1,
                                                                         juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                         true);
        oversampling->initProcessing ((size_t) preparedBlockSize);
    }
    else
    {
        oversampling.reset();
    }

    updateCoefficients();
    reset();
}

void Limiter::rebuildDelay()
{
    delayLength = juce::jmax (1, (int) std::ceil (lookaheadMs * 0.001 * sampleRate) + 1);
    delayLeft.assign ((size_t) delayLength, 0.0f);
    delayRight.assign ((size_t) delayLength, 0.0f);
    writeIndex = 0;
}

void Limiter::updateCoefficients() noexcept
{
    ceilingGain = (float) juce::Decibels::decibelsToGain (ceilingDb, -100.0f);
    const auto releaseSeconds = juce::jmax (0.005, (double) releaseMs / 1000.0);
    releaseCoefficient = (float) std::exp (-1.0 / (releaseSeconds * sampleRate));
    const auto attackSeconds = 0.0012;
    attackStep = (float) juce::jlimit (1.0e-5, 1.0, 1.0 / (attackSeconds * sampleRate));
}

void Limiter::reset() noexcept
{
    std::fill (delayLeft.begin(), delayLeft.end(), 0.0f);
    std::fill (delayRight.begin(), delayRight.end(), 0.0f);
    writeIndex = 0;
    currentGain = 1.0f;
    lastGainReductionDb = 0.0f;
    lastBlockClipped = false;
}

void Limiter::setCeilingDb (float newCeilingDb) noexcept
{
    const auto clamped = juce::jlimit (-60.0f, 0.0f, newCeilingDb);

    if (! juce::approximatelyEqual (clamped, ceilingDb))
    {
        ceilingDb = clamped;
        updateCoefficients();
    }
}

void Limiter::setReleaseMs (float newReleaseMs) noexcept
{
    const auto clamped = juce::jlimit (5.0f, 2000.0f, newReleaseMs);

    if (! juce::approximatelyEqual (clamped, releaseMs))
    {
        releaseMs = clamped;
        updateCoefficients();
    }
}

void Limiter::setLookaheadMs (float newLookaheadMs) noexcept
{
    const auto clamped = juce::jlimit (0.0f, 20.0f, newLookaheadMs);

    if (! juce::approximatelyEqual (clamped, lookaheadMs))
    {
        lookaheadMs = clamped;
        rebuildDelay();
        reset();
    }
}

void Limiter::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    lastGainReductionDb = 0.0f;
    lastBlockClipped = false;

    const bool hasRight = right != nullptr;
    const auto usableSamples = juce::jmin (numSamples, (int) targetGains.size());

    for (int i = 0; i < usableSamples; ++i)
    {
        auto peak = std::abs (left[i]);

        if (hasRight)
            peak = juce::jmax (peak, std::abs (right[i]));

        targetGains[(size_t) i] = peak > ceilingGain ? ceilingGain / peak : 1.0f;
    }

    if (truePeakEnabled && oversampling != nullptr && numSamples <= preparedBlockSize)
    {
        const float* inputChannels[2] = { left, hasRight ? right : left };
        juce::dsp::AudioBlock<const float> inputBlock (inputChannels, 2, (size_t) numSamples);
        auto oversampled = oversampling->processSamplesUp (inputBlock);
        const auto oversampledLength = (int) oversampled.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            const auto baseIndex = i * 2;
            float truePeak = 0.0f;

            for (int offset = -2; offset <= 3; ++offset)
            {
                const auto index = baseIndex + offset;

                if (index >= 0 && index < oversampledLength)
                    truePeak = juce::jmax (truePeak, std::abs (oversampled.getSample (0, (size_t) index)),
                                                      std::abs (oversampled.getSample (1, (size_t) index)));
            }

            if (truePeak > ceilingGain)
                targetGains[(size_t) i] = juce::jmin (targetGains[(size_t) i], ceilingGain / truePeak);
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const auto target = i < usableSamples ? targetGains[(size_t) i] : 1.0f;

        if (target < currentGain)
            currentGain = juce::jmax (target, currentGain - attackStep);
        else
            currentGain = juce::jmin (1.0f, currentGain + (1.0f - currentGain) * (1.0f - releaseCoefficient));

        currentGain = juce::jlimit (0.0f, 1.0f, currentGain);

        delayLeft[(size_t) writeIndex] = left[i];

        if (hasRight)
            delayRight[(size_t) writeIndex] = right[i];

        const auto readIndex = (writeIndex + 1) % delayLength;
        auto outLeft = delayLeft[(size_t) readIndex] * currentGain;
        auto outRight = hasRight ? delayRight[(size_t) readIndex] * currentGain : 0.0f;

        if (std::abs (outLeft) > ceilingGain)
        {
            outLeft = juce::jlimit (-ceilingGain, ceilingGain, outLeft);
            lastBlockClipped = true;
        }

        if (hasRight && std::abs (outRight) > ceilingGain)
        {
            outRight = juce::jlimit (-ceilingGain, ceilingGain, outRight);
            lastBlockClipped = true;
        }

        left[i] = std::isfinite (outLeft) ? outLeft : 0.0f;

        if (hasRight)
            right[i] = std::isfinite (outRight) ? outRight : 0.0f;

        writeIndex = (writeIndex + 1) % delayLength;

        const auto reduction = currentGain < 1.0f
                                 ? -juce::Decibels::gainToDecibels (currentGain, -100.0f)
                                 : 0.0f;
        lastGainReductionDb = juce::jmax (lastGainReductionDb, reduction);
    }
}

} // namespace rr::dsp
