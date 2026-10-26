#pragma once

#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace rr::dsp
{
class Limiter
{
public:
    Limiter() = default;

    void prepare (double newSampleRate, int maximumBlockSize, bool useTruePeakDetector = true);
    void reset() noexcept;

    void setCeilingDb (float newCeilingDb) noexcept;
    void setReleaseMs (float newReleaseMs) noexcept;
    void setLookaheadMs (float newLookaheadMs) noexcept;

    void process (float* left, float* right, int numSamples) noexcept;

    float getGainReductionDb() const noexcept { return lastGainReductionDb; }
    bool getLastBlockClipped() const noexcept { return lastBlockClipped; }
    int getLatencySamples() const noexcept { return delayLength; }
    float getCeilingLinear() const noexcept { return ceilingGain; }

private:
    void rebuildDelay();
    void updateCoefficients() noexcept;

    double sampleRate = 48000.0;
    float ceilingDb = -1.0f;
    float ceilingGain = 0.891f;
    float releaseMs = 120.0f;
    float lookaheadMs = 5.0f;
    float releaseCoefficient = 0.0f;
    float attackStep = 0.02f;

    int delayLength = 1;
    int writeIndex = 0;
    std::vector<float> delayLeft;
    std::vector<float> delayRight;
    std::vector<float> targetGains;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    bool truePeakEnabled = true;
    int preparedBlockSize = 0;

    float currentGain = 1.0f;
    float lastGainReductionDb = 0.0f;
    bool lastBlockClipped = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Limiter)
};

} // namespace rr::dsp
