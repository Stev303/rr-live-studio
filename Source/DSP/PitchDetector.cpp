#include "DSP/PitchDetector.h"

namespace rr
{
void PitchDetector::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    minimumLag = juce::jmax (2, (int) std::floor (sampleRate / 1200.0));
    maximumLag = juce::jmin (maxLagLimit, (int) std::ceil (sampleRate / 65.0));
    ring.assign (ringSize, 0.0f);
    frame.assign (analysisWindow, 0.0f);
    difference.assign ((size_t) maximumLag + 1, 0.0f);
    cumulative.assign ((size_t) maximumLag + 1, 1.0f);
    reset();
}

void PitchDetector::reset() noexcept
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writeIndex = 0;
    samplesSinceAnalysis = 0;
    frequencyHz.store (0.0f, std::memory_order_relaxed);
    midiNote.store (0.0f, std::memory_order_relaxed);
    confidence.store (0.0f, std::memory_order_relaxed);
    voiced.store (false, std::memory_order_relaxed);
}

void PitchDetector::process (const float* data, int numSamples) noexcept
{
    if (ring.empty() || data == nullptr)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        ring[(size_t) writeIndex] = std::isfinite (data[i]) ? data[i] : 0.0f;
        writeIndex = (writeIndex + 1) % ringSize;

        if (++samplesSinceAnalysis >= analysisHop)
        {
            samplesSinceAnalysis = 0;
            analyseFrame();
        }
    }
}

void PitchDetector::analyseFrame() noexcept
{
    for (int i = 0; i < analysisWindow; ++i)
    {
        const auto index = (writeIndex - analysisWindow + i + ringSize * 2) % ringSize;
        frame[(size_t) i] = ring[(size_t) index];
    }

    double energy = 0.0;

    for (const auto sample : frame)
        energy += (double) sample * (double) sample;

    const auto rms = (float) std::sqrt (energy / (double) analysisWindow);

    if (rms < 1.0e-4f)
    {
        voiced.store (false, std::memory_order_relaxed);
        confidence.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const int effectiveWindow = analysisWindow - maximumLag;

    if (effectiveWindow <= 0)
        return;

    for (int lag = 0; lag <= maximumLag; ++lag)
    {
        float sum = 0.0f;

        for (int j = 0; j < effectiveWindow; ++j)
        {
            const auto delta = frame[(size_t) j] - frame[(size_t) (j + lag)];
            sum += delta * delta;
        }

        difference[(size_t) lag] = sum;
    }

    cumulative[0] = 1.0f;
    double runningSum = 0.0;

    for (int lag = 1; lag <= maximumLag; ++lag)
    {
        runningSum += difference[(size_t) lag];
        cumulative[(size_t) lag] = runningSum > 1.0e-12
                                     ? (float) (((double) difference[(size_t) lag] * lag) / runningSum)
                                     : 1.0f;
    }

    const auto threshold = 0.18f;
    int bestLag = -1;

    for (int lag = minimumLag; lag < maximumLag; ++lag)
    {
        if (cumulative[(size_t) lag] < threshold)
        {
            while (lag + 1 < maximumLag && cumulative[(size_t) (lag + 1)] < cumulative[(size_t) lag])
                ++lag;

            bestLag = lag;
            break;
        }
    }

    if (bestLag < 0)
    {
        auto minValue = 1.0f;
        int minLag = -1;

        for (int lag = minimumLag; lag < maximumLag; ++lag)
        {
            if (cumulative[(size_t) lag] < minValue)
            {
                minValue = cumulative[(size_t) lag];
                minLag = lag;
            }
        }

        if (minLag > 0 && minValue < 0.40f)
            bestLag = minLag;
    }

    if (bestLag <= 0)
    {
        voiced.store (false, std::memory_order_relaxed);
        confidence.store (0.0f, std::memory_order_relaxed);
        return;
    }

    auto refinedLag = (float) bestLag;

    if (bestLag > minimumLag && bestLag < maximumLag - 1)
    {
        const auto y0 = cumulative[(size_t) (bestLag - 1)];
        const auto y1 = cumulative[(size_t) bestLag];
        const auto y2 = cumulative[(size_t) (bestLag + 1)];
        const auto denominator = 2.0f * (2.0f * y1 - y2 - y0);

        if (std::abs (denominator) > 1.0e-8f)
            refinedLag += (y2 - y0) / denominator;
    }

    refinedLag = juce::jlimit (1.0f, (float) maximumLag, refinedLag);
    const auto detectedFrequency = (float) (sampleRate / (double) refinedLag);

    if (detectedFrequency < 60.0f || detectedFrequency > 1300.0f)
    {
        voiced.store (false, std::memory_order_relaxed);
        confidence.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const auto confidenceValue = juce::jlimit (0.0f, 1.0f, 1.0f - cumulative[(size_t) bestLag]);
    frequencyHz.store (detectedFrequency, std::memory_order_relaxed);
    midiNote.store (frequencyToMidi (detectedFrequency), std::memory_order_relaxed);
    confidence.store (confidenceValue, std::memory_order_relaxed);
    voiced.store (confidenceValue > 0.25f, std::memory_order_relaxed);
}

} // namespace rr
