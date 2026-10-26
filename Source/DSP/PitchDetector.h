#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

namespace rr
{
class PitchDetector
{
public:
    PitchDetector() = default;

    void prepare (double newSampleRate);
    void reset() noexcept;

    void process (const float* data, int numSamples) noexcept;

    float getFrequencyHz() const noexcept    { return frequencyHz.load (std::memory_order_relaxed); }
    float getMidiNote() const noexcept       { return midiNote.load (std::memory_order_relaxed); }
    float getConfidence() const noexcept     { return confidence.load (std::memory_order_relaxed); }
    bool isVoiced() const noexcept           { return voiced.load (std::memory_order_relaxed); }

    static float frequencyToMidi (float frequencyHz) noexcept
    {
        return frequencyHz > 1.0f ? 69.0f + 12.0f * std::log2 (frequencyHz / 440.0f) : 0.0f;
    }

    static float midiToFrequency (float midi) noexcept
    {
        return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
    }

private:
    void analyseFrame() noexcept;

    static constexpr int analysisWindow = 2048;
    static constexpr int analysisHop = 1024;
    static constexpr int ringSize = 8192;
    static constexpr int maxLagLimit = 1024;

    double sampleRate = 48000.0;
    std::vector<float> ring;
    std::vector<float> frame;
    std::vector<float> difference;
    std::vector<float> cumulative;
    int writeIndex = 0;
    int samplesSinceAnalysis = 0;
    int minimumLag = 40;
    int maximumLag = 800;

    std::atomic<float> frequencyHz { 0.0f };
    std::atomic<float> midiNote { 0.0f };
    std::atomic<float> confidence { 0.0f };
    std::atomic<bool> voiced { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchDetector)
};

} // namespace rr
