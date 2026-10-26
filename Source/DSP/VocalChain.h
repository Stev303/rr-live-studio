#pragma once

#include "Audio/ParamStore.h"
#include "DSP/Biquad.h"
#include "DSP/Compressor.h"
#include "DSP/Harmonizer.h"
#include "DSP/PitchDetector.h"
#include "DSP/PitchShifter.h"
#include "DSP/SmartEQ.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

namespace rr
{
/** Microphone vocal chain.

    Signal order:
      input gain -> gate/expander -> HPF -> 5-band EQ (Smart EQ) -> de-esser
      -> compressor -> pitch correction -> harmonizer -> saturation
      -> reverb -> delay -> vocal fader.

    All memory is reserved in prepare(). process() performs only arithmetic and
    state updates.  Pitch detection feeds both the correction and the harmony
    engines so the expensive analysis runs once per block.
*/
class VocalChain
{
public:
    explicit VocalChain (ParamStore& parameters);
    ~VocalChain() = default;

    void prepare (double newSampleRate, int maximumBlockSize);
    void reset() noexcept;

    void process (const float* inputL, const float* inputR,
                  float* outputL, float* outputR, int numSamples) noexcept;

    SmartEQ& getSmartEQ() noexcept { return smartEq; }
    Harmonizer& getHarmonizer() noexcept { return harmonizer; }
    PitchDetector& getPitchDetector() noexcept { return pitchDetector; }

    float getInputPeakDb() const noexcept { return inputPeakDb.load (std::memory_order_relaxed); }
    float getOutputPeakDb() const noexcept { return outputPeakDb.load (std::memory_order_relaxed); }
    float getCompressorGainReductionDb() const noexcept { return compGrDb.load (std::memory_order_relaxed); }
    float getDeEsserGainReductionDb() const noexcept { return deEsserGrDb.load (std::memory_order_relaxed); }
    bool getGateOpen() const noexcept { return gateOpen.load (std::memory_order_relaxed); }
    bool getClipped() const noexcept { return clipped.load (std::memory_order_relaxed); }
    float getDetectedMidiNote() const noexcept { return pitchDetector.getMidiNote(); }
    float getDetectedFrequencyHz() const noexcept { return pitchDetector.getFrequencyHz(); }
    float getPitchConfidence() const noexcept { return pitchDetector.getConfidence(); }
    bool isPitchVoiced() const noexcept { return pitchDetector.isVoiced(); }
    float getAppliedCorrectionSemitones() const noexcept { return appliedCorrectionSemitones.load (std::memory_order_relaxed); }
    int getLatencySamples() const noexcept;

    void clearClipFlag() noexcept { clipped.store (false, std::memory_order_relaxed); }

private:
    void updateFilters() noexcept;
    void processGate (float* left, float* right, int numSamples) noexcept;
    void processDeEsser (float* left, float* right, int numSamples) noexcept;
    void processPitchCorrection (float* left, float* right, int numSamples) noexcept;
    void processSaturation (float* left, float* right, int numSamples) noexcept;
    void processDelay (float* left, float* right, int numSamples) noexcept;
    void updateReverbParameters() noexcept;
    float targetCorrectionRatio (float detectedMidi, float confidence) const noexcept;

    ParamStore& params;
    double sampleRate = 48000.0;
    int maxBlockSize = 512;

    juce::SmoothedValue<float> inputGainSmoother { 1.0f };
    juce::SmoothedValue<float> faderSmoother { 1.0f };

    // Gate
    float gateEnvelope = 0.0f;
    float gateGain = 1.0f;
    int gateHoldCounter = 0;
    bool gateOpenState = false;

    std::array<dsp::BiquadFilter, 2> hpfFilters;
    std::array<std::array<dsp::BiquadFilter, 2>, 5> eqFilters;
    std::array<float, 5> eqFrequencies { 80.0f, 300.0f, 1200.0f, 3500.0f, 8000.0f };
    std::array<float, 5> eqGains { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 5> eqQs { 0.9f, 1.0f, 1.0f, 1.0f, 1.0f };
    float lastHpfHz = -1.0f;

    std::array<dsp::BiquadFilter, 2> deEsserLowPass;
    float deEsserEnvelope = 0.0f;
    float deEsserGain = 1.0f;

    dsp::Compressor compressor;
    PitchDetector pitchDetector;
    PhaseVocoderPitchShifter corrector;
    std::vector<float> correctionL, correctionR;
    std::vector<float> pitchMono;
    float correctionRatio = 1.0f;
    bool correctionWasEnabled = false;

    Harmonizer harmonizer;
    SmartEQ smartEq;

    float saturationCurrent = 0.0f;

    juce::Reverb reverb;
    bool reverbWasEnabled = false;

    std::vector<float> delayLeft;
    std::vector<float> delayRight;
    int delayWriteIndex = 0;
    float smoothedDelaySamples = 0.0f;
    int delayBufferLength = 1;
    bool delayWasEnabled = false;

    std::atomic<float> inputPeakDb { -100.0f };
    std::atomic<float> outputPeakDb { -100.0f };
    std::atomic<float> compGrDb { 0.0f };
    std::atomic<float> deEsserGrDb { 0.0f };
    std::atomic<float> appliedCorrectionSemitones { 0.0f };
    std::atomic<bool> gateOpen { false };
    std::atomic<bool> clipped { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalChain)
};

} // namespace rr
