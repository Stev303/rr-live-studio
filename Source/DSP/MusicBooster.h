#pragma once

#include "Audio/ParamStore.h"
#include "DSP/Biquad.h"
#include "DSP/Compressor.h"
#include "DSP/Limiter.h"
#include <array>

namespace rr
{
class MusicBooster
{
public:
    explicit MusicBooster (ParamStore& parameters);
    ~MusicBooster() = default;

    void prepare (double newSampleRate, int maximumBlockSize);
    void reset() noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

    void requestAutoBoost() noexcept;
    void resetAutoGain() noexcept;

    float getPeakDb() const noexcept { return peakDb.load (std::memory_order_relaxed); }
    float getRmsDb() const noexcept { return rmsDb.load (std::memory_order_relaxed); }
    float getCompressorGainReductionDb() const noexcept { return publishedCompressorGr.load (std::memory_order_relaxed); }
    float getLimiterGainReductionDb() const noexcept { return publishedLimiterGr.load (std::memory_order_relaxed); }
    float getAutoBoostAppliedDb() const noexcept { return autoBoostAppliedDb.load (std::memory_order_relaxed); }
    int getAutoBoostState() const noexcept { return autoBoostState.load (std::memory_order_relaxed); }
    bool getClipFlag() const noexcept { return clipFlag.load (std::memory_order_relaxed); }
    void clearClipFlag() noexcept { clipFlag.store (false, std::memory_order_relaxed); }
    float getAutoLevelGainDb() const noexcept { return autoLevelGainDb; }
    int getLatencySamples() const noexcept { return limiter.getLatencySamples(); }

private:
    void updateFilters() noexcept;
    void updateAutoLevel (float blockRmsDb) noexcept;

    ParamStore& params;
    double sampleRate = 48000.0;
    int blockSize = 512;

    std::array<dsp::BiquadFilter, 2> bassFilter;
    std::array<dsp::BiquadFilter, 2> presenceFilter;
    std::array<dsp::BiquadFilter, 2> trebleFilter;
    std::array<dsp::BiquadFilter, 2> loudnessLowFilter;
    std::array<dsp::BiquadFilter, 2> loudnessHighFilter;
    dsp::Compressor compressor;
    dsp::Limiter limiter;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preGainSmoother { 1.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> postGainSmoother { 1.0f };
    float autoLevelGainDb = 0.0f;
    float autoBoostGainDb = 0.0f;
    float autoBoostAppliedDbValue = 0.0f;

    // Auto boost measurement (runs on the audio thread with plain accumulators).
    std::atomic<bool> autoBoostRequested { false };
    std::atomic<bool> autoBoostResetRequested { false };
    std::atomic<int> autoBoostState { 0 };
    std::atomic<float> autoBoostAppliedDb { 0.0f };
    double boostSumSquares = 0.0;
    float boostPeak = 0.0f;
    int boostSamples = 0;
    int boostTargetSamples = 1;

    std::atomic<float> peakDb { -100.0f };
    std::atomic<float> rmsDb { -100.0f };
    std::atomic<float> publishedCompressorGr { 0.0f };
    std::atomic<float> publishedLimiterGr { 0.0f };
    std::atomic<bool> clipFlag { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MusicBooster)
};

} // namespace rr
