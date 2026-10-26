#pragma once

#include "Audio/ParamStore.h"

namespace rr
{
/** Level-following auto mixer between the YouTube music bus and the vocal bus.

    The processor only ever attenuates the music bus (never boosts the master) and
    publishes its current gain reduction so the UI can display it.
*/
class SmartBalance
{
public:
    explicit SmartBalance (ParamStore& parameters);

    void prepare (double newSampleRate);
    void reset() noexcept;
    void process (float* musicLeft, float* musicRight, const float* vocalMono, int numSamples) noexcept;

    float getGainReductionDb() const noexcept { return publishedGainReductionDb.load (std::memory_order_relaxed); }

private:
    ParamStore& params;
    double sampleRate = 48000.0;
    float vocalEnvelope = 0.0f;
    float musicEnvelope = 0.0f;
    float gainDb = 0.0f;
    float vocalAttackCoefficient = 0.0f;
    float vocalReleaseCoefficient = 0.0f;
    float musicAttackCoefficient = 0.0f;
    float musicReleaseCoefficient = 0.0f;
    float duckAttackCoefficient = 0.0f;
    float duckReleaseCoefficient = 0.0f;
    std::atomic<float> publishedGainReductionDb { 0.0f };

    static float timeToCoefficient (float milliseconds, double sampleRate) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartBalance)
};

} // namespace rr
