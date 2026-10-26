#pragma once

#include "Audio/ParamStore.h"
#include "DSP/PitchShifter.h"
#include "Utilities/MusicTheory.h"
#include <atomic>
#include <vector>

namespace rr
{
/** Two-voice scale-aware harmonizer for the microphone vocal bus.

    The pitch of the dry vocal is detected once per block by the vocal chain and
    handed to this class.  Each harmony voice runs a full stereo phase-vocoder
    pitch shifter, so the voices follow the melody instead of being chorus or
    delay effects.  Ratios are smoothed according to the Retune parameter and
    the dry/wet crossfade is ramped to avoid clicks.

    prepare() allocates; process() is allocation-free and can run on the audio
    thread.
*/
class Harmonizer
{
public:
    explicit Harmonizer (ParamStore& parameters);
    ~Harmonizer() = default;

    void prepare (double newSampleRate, int maximumBlockSize);
    void reset() noexcept;

    void process (const float* inputL, const float* inputR,
                  float* outputL, float* outputR,
                  int numSamples,
                  float detectedMidiNote,
                  float pitchConfidence,
                  bool pitchIsVoiced) noexcept;

    int getLatencySamples() const noexcept { return voice1.getLatencySamples(); }
    bool isActive() const noexcept { return active; }

    float getVoice1ShiftSemitones() const noexcept { return shift1Semitones.load (std::memory_order_relaxed); }
    float getVoice2ShiftSemitones() const noexcept { return shift2Semitones.load (std::memory_order_relaxed); }
    float getWetAmount() const noexcept { return wetAmount.load (std::memory_order_relaxed); }

private:
    float computeVoiceRatio (int intervalParam, float customSemitones,
                             float detectedMidi, int key, int scale) const noexcept;
    void updateShifterParameters (PhaseVocoderPitchShifter& shifter,
                                  float targetRatio, float humanizeCents) noexcept;
    void applyStereoWidth (float* left, float* right, int numSamples, float width) noexcept;

    ParamStore& params;
    double sampleRate = 48000.0;
    int maxBlockSize = 512;

    PhaseVocoderPitchShifter voice1;
    PhaseVocoderPitchShifter voice2;

    std::vector<float> wet1L, wet1R, wet2L, wet2R;

    float smoothedRatio1 = 1.0f;
    float smoothedRatio2 = 1.0f;
    float lastShift1 = 0.0f;
    float lastShift2 = 0.0f;
    float humanizePhase = 0.0f;
    float gain1 = 0.0f;
    float gain2 = 0.0f;
    float wetMixture = 0.0f;
    bool active = false;
    bool wasEnabled = false;

    std::atomic<float> shift1Semitones { 0.0f };
    std::atomic<float> shift2Semitones { 0.0f };
    std::atomic<float> wetAmount { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Harmonizer)
};

} // namespace rr
