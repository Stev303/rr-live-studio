#include "DSP/MusicBooster.h"

namespace rr
{
MusicBooster::MusicBooster (ParamStore& parameters)
    : params (parameters)
{
    prepare (48000.0, 512);
}

void MusicBooster::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    blockSize = juce::jmax (32, maximumBlockSize);
    compressor.prepare (sampleRate);
    limiter.prepare (sampleRate, blockSize, true);
    boostTargetSamples = juce::jmax (1, (int) (sampleRate * 1.5));
    preGainSmoother.reset (sampleRate, 0.02);
    postGainSmoother.reset (sampleRate, 0.02);
    reset();
}

void MusicBooster::reset() noexcept
{
    for (auto& filter : bassFilter) filter.reset();
    for (auto& filter : presenceFilter) filter.reset();
    for (auto& filter : trebleFilter) filter.reset();
    for (auto& filter : loudnessLowFilter) filter.reset();
    for (auto& filter : loudnessHighFilter) filter.reset();

    compressor.reset();
    limiter.reset();
    autoLevelGainDb = 0.0f;
    autoBoostGainDb = 0.0f;
    autoBoostAppliedDbValue = 0.0f;
    boostSumSquares = 0.0;
    boostPeak = 0.0f;
    boostSamples = 0;
    peakDb.store (-100.0f, std::memory_order_relaxed);
    rmsDb.store (-100.0f, std::memory_order_relaxed);
    publishedCompressorGr.store (0.0f, std::memory_order_relaxed);
    publishedLimiterGr.store (0.0f, std::memory_order_relaxed);
    clipFlag.store (false, std::memory_order_relaxed);
    autoBoostAppliedDb.store (0.0f, std::memory_order_relaxed);
    autoBoostState.store (0, std::memory_order_relaxed);
    preGainSmoother.setCurrentAndTargetValue (1.0f);
    postGainSmoother.setCurrentAndTargetValue (1.0f);
}

void MusicBooster::requestAutoBoost() noexcept
{
    autoBoostRequested.store (true, std::memory_order_relaxed);
}

void MusicBooster::resetAutoGain() noexcept
{
    autoBoostResetRequested.store (true, std::memory_order_relaxed);
}

void MusicBooster::updateFilters() noexcept
{
    const auto bass = params.get (ParamId::musicBassDb);
    const auto presence = params.get (ParamId::musicPresenceDb);
    const auto treble = params.get (ParamId::musicTrebleDb);
    const auto loudness = params.get (ParamId::musicLoudness) * 0.01f;

    const auto bassCoefficients = dsp::BiquadCoefficients::lowShelf (sampleRate, 120.0, bass);
    const auto presenceCoefficients = dsp::BiquadCoefficients::peaking (sampleRate, 3000.0, 0.9, presence);
    const auto trebleCoefficients = dsp::BiquadCoefficients::highShelf (sampleRate, 8000.0, treble);
    const auto loudnessLowCoefficients = dsp::BiquadCoefficients::lowShelf (sampleRate, 100.0, loudness * 4.0);
    const auto loudnessHighCoefficients = dsp::BiquadCoefficients::highShelf (sampleRate, 7000.0, loudness * 2.0);

    for (int ch = 0; ch < 2; ++ch)
    {
        bassFilter[(size_t) ch].setCoefficients (bassCoefficients);
        presenceFilter[(size_t) ch].setCoefficients (presenceCoefficients);
        trebleFilter[(size_t) ch].setCoefficients (trebleCoefficients);
        loudnessLowFilter[(size_t) ch].setCoefficients (loudnessLowCoefficients);
        loudnessHighFilter[(size_t) ch].setCoefficients (loudnessHighCoefficients);
    }
}

void MusicBooster::updateAutoLevel (float measuredRmsDb) noexcept
{
    if (! params.getBool (ParamId::boosterAutoLevel) || measuredRmsDb < -60.0f)
        return;

    const auto targetRms = -18.0f;
    const auto error = targetRms - measuredRmsDb;
    const auto step = juce::jlimit (-0.05f, 0.05f, error * 0.08f);
    autoLevelGainDb = juce::jlimit (-12.0f, 12.0f, autoLevelGainDb + step);
}

void MusicBooster::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    const bool hasRight = right != nullptr;
    const bool bypassed = params.getBool (ParamId::boosterBypass);

    auto inputPeak = 0.0f;
    double inputSumSquares = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto peak = juce::jmax (std::abs (left[i]), hasRight ? std::abs (right[i]) : 0.0f);
        inputPeak = juce::jmax (inputPeak, peak);
        inputSumSquares += (double) left[i] * left[i];

        if (hasRight)
            inputSumSquares += (double) right[i] * right[i];
    }

    if (autoBoostResetRequested.exchange (false, std::memory_order_relaxed))
    {
        autoBoostGainDb = 0.0f;
        autoBoostAppliedDbValue = 0.0f;
        autoBoostAppliedDb.store (0.0f, std::memory_order_relaxed);
        autoBoostState.store (0, std::memory_order_relaxed);
        boostSamples = 0;
        boostSumSquares = 0.0;
        boostPeak = 0.0f;
    }

    if (autoBoostRequested.exchange (false, std::memory_order_relaxed))
    {
        boostSamples = 0;
        boostSumSquares = 0.0;
        boostPeak = 0.0f;
        autoBoostState.store (1, std::memory_order_relaxed);
    }

    if (autoBoostState.load (std::memory_order_relaxed) == 1)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            boostPeak = juce::jmax (boostPeak, std::abs (left[i]));

            if (hasRight)
                boostPeak = juce::jmax (boostPeak, std::abs (right[i]));
        }

        boostSumSquares += inputSumSquares;
        boostSamples += numSamples * (hasRight ? 2 : 1);

        if (boostSamples >= boostTargetSamples)
        {
            const auto rms = (float) std::sqrt (boostSumSquares / juce::jmax (1, boostSamples));
            const auto measuredRmsDb = juce::Decibels::gainToDecibels (rms, -100.0f);
            const auto measuredPeakDb = juce::Decibels::gainToDecibels (boostPeak, -100.0f);
            auto desired = -16.0f - measuredRmsDb;
            desired = juce::jmin (desired, -1.5f - measuredPeakDb);
            autoBoostGainDb = juce::jlimit (-12.0f, 12.0f, desired);
            autoBoostAppliedDbValue = autoBoostGainDb;
            autoBoostAppliedDb.store (autoBoostGainDb, std::memory_order_relaxed);
            autoBoostState.store (2, std::memory_order_relaxed);
        }
    }

    if (bypassed)
    {
        peakDb.store (juce::Decibels::gainToDecibels (inputPeak, -100.0f), std::memory_order_relaxed);
        const auto channels = hasRight ? 2 : 1;
        const auto rms = (float) std::sqrt (inputSumSquares / (double) juce::jmax (1, numSamples * channels));
        rmsDb.store (juce::Decibels::gainToDecibels (rms, -100.0f), std::memory_order_relaxed);
        publishedCompressorGr.store (0.0f, std::memory_order_relaxed);
        publishedLimiterGr.store (0.0f, std::memory_order_relaxed);
        return;
    }

    updateFilters();
    compressor.setParameters (params.get (ParamId::boosterCompThresholdDb),
                              params.get (ParamId::boosterCompRatio),
                              params.get (ParamId::boosterCompAttackMs),
                              params.get (ParamId::boosterCompReleaseMs),
                              params.get (ParamId::boosterCompMakeupDb));
    limiter.setCeilingDb (params.get (ParamId::boosterLimiterCeilingDb));
    limiter.setReleaseMs (150.0f);

    const auto preGainDb = params.get (ParamId::musicGainDb)
                         + params.get (ParamId::musicInputTrimDb)
                         + autoLevelGainDb
                         + autoBoostGainDb;
    preGainSmoother.setTargetValue (juce::Decibels::decibelsToGain (juce::jlimit (-48.0f, 24.0f, preGainDb)));
    postGainSmoother.setTargetValue (juce::Decibels::decibelsToGain (params.get (ParamId::boosterOutputGainDb)));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = preGainSmoother.getNextValue();
        auto l = left[i] * gain;
        auto r = hasRight ? right[i] * gain : 0.0f;

        l = loudnessHighFilter[0].processSample (loudnessLowFilter[0].processSample (
                trebleFilter[0].processSample (presenceFilter[0].processSample (bassFilter[0].processSample (l)))));
        r = loudnessHighFilter[1].processSample (loudnessLowFilter[1].processSample (
                trebleFilter[1].processSample (presenceFilter[1].processSample (bassFilter[1].processSample (r)))));

        left[i] = l;

        if (hasRight)
            right[i] = r;
    }

    compressor.process (left, hasRight ? right : nullptr, numSamples);

    const auto preLimiterPeak = [&]
    {
        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (left[i]), hasRight ? std::abs (right[i]) : 0.0f);

        return peak;
    }();

    limiter.process (left, hasRight ? right : nullptr, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = postGainSmoother.getNextValue();
        left[i] *= gain;

        if (hasRight)
            right[i] *= gain;
    }

    auto outputPeak = preLimiterPeak;
    double outputSumSquares = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        outputPeak = juce::jmax (outputPeak, std::abs (left[i]), hasRight ? std::abs (right[i]) : 0.0f);
        outputSumSquares += (double) left[i] * left[i];

        if (hasRight)
            outputSumSquares += (double) right[i] * right[i];
    }

    const auto channels = hasRight ? 2 : 1;
    const auto outputRms = (float) std::sqrt (outputSumSquares / (double) juce::jmax (1, numSamples * channels));

    peakDb.store (juce::Decibels::gainToDecibels (outputPeak, -100.0f), std::memory_order_relaxed);
    rmsDb.store (juce::Decibels::gainToDecibels (outputRms, -100.0f), std::memory_order_relaxed);
    publishedCompressorGr.store (compressor.getGainReductionDb(), std::memory_order_relaxed);
    publishedLimiterGr.store (limiter.getGainReductionDb(), std::memory_order_relaxed);

    if (limiter.getLastBlockClipped() || outputPeak > 1.0f)
        clipFlag.store (true, std::memory_order_relaxed);

    updateAutoLevel (juce::Decibels::gainToDecibels (outputRms, -100.0f));
}

} // namespace rr
