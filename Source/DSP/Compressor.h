#pragma once

#include <juce_dsp/juce_dsp.h>

namespace rr::dsp
{
class Compressor
{
public:
    Compressor() = default;

    void prepare (double newSampleRate);
    void reset() noexcept;

    void setParameters (float thresholdDb,
                        float ratio,
                        float attackMs,
                        float releaseMs,
                        float makeupDb,
                        float kneeDb = 6.0f) noexcept;

    void process (float* left, float* right, int numSamples) noexcept;
    void processMono (float* data, int numSamples) noexcept;

    float getGainReductionDb() const noexcept { return lastGainReductionDb; }
    bool isEnabled() const noexcept { return enabled; }
    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

private:
    float computeTargetGain (float detectorLevel) const noexcept;

    double sampleRate = 48000.0;
    float threshold = -18.0f;
    float ratio = 3.0f;
    float knee = 6.0f;
    float makeup = 1.0f;
    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    float lastAttackMs = 10.0f;
    float lastReleaseMs = 120.0f;
    float envelope = 1.0f;
    float lastGainReductionDb = 0.0f;
    bool enabled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Compressor)
};

} // namespace rr::dsp
