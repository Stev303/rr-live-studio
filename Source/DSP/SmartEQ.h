#pragma once

#include "Audio/ParamStore.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

namespace rr
{
struct SmartEqBand
{
    float frequencyHz = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    juce::String reason;
};

struct SmartEqAnalysis
{
    static constexpr int numBands = 5;

    std::array<SmartEqBand, numBands> bands;
    juce::StringArray notes;
    float confidence = 0.0f;
    bool hasData = false;
};

/** Rule-based spectrum analyser for the vocal chain.

    The audio thread only runs a windowed FFT and stores a smoothed log-band
    spectrum in atomics.  All detection, recommendation and parameter writing
    happens on the message thread, so the audio callback is never blocked by
    analysis work.  This is a DSP fallback, not a trained AI model.
*/
class SmartEQ
{
public:
    static constexpr int numSpectrumBands = 96;

    explicit SmartEQ (ParamStore& parameters);
    ~SmartEQ() = default;

    void prepare (double newSampleRate);
    void reset() noexcept;

    void process (const float* left, const float* right, int numSamples) noexcept;

    SmartEqAnalysis analyse() const;
    void apply (const SmartEqAnalysis& analysis);
    void undo();
    void resetEq();
    bool canUndo() const noexcept { return undoValid; }

    void getSpectrum (std::array<float, numSpectrumBands>& destination) const noexcept;
    int getAnalysisFrameCount() const noexcept { return analysisFrames.load (std::memory_order_relaxed); }
    bool hasEnoughData() const noexcept { return analysisFrames.load (std::memory_order_relaxed) >= 5; }

    static juce::String methodology();

    static constexpr std::array<ParamId, 15> eqParamIds {
        ParamId::eq1Freq, ParamId::eq1Gain, ParamId::eq1Q,
        ParamId::eq2Freq, ParamId::eq2Gain, ParamId::eq2Q,
        ParamId::eq3Freq, ParamId::eq3Gain, ParamId::eq3Q,
        ParamId::eq4Freq, ParamId::eq4Gain, ParamId::eq4Q,
        ParamId::eq5Freq, ParamId::eq5Gain, ParamId::eq5Q
    };

private:
    void analyseFrame() noexcept;
    float averageLevelBetween (float lowHz, float highHz) const noexcept;
    int bandIndexForFrequency (float frequencyHz) const noexcept;

    ParamStore& params;
    double sampleRate = 48000.0;

    std::unique_ptr<juce::dsp::FFT> fft;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int analysisInterval = fftSize * 2;

    std::vector<float> window;
    std::vector<float> ring;
    std::vector<float> fftData;
    int ringWriteIndex = 0;
    int samplesSinceAnalysis = 0;

    std::array<std::atomic<float>, numSpectrumBands> spectrumDb;
    std::atomic<int> analysisFrames { 0 };

    std::array<float, 15> undoValues {};
    bool undoValid = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmartEQ)
};

} // namespace rr
