#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

namespace rr
{
/** Stereo phase-vocoder pitch shifter.

    Pitch shifting is performed by time-stretching the input with a phase vocoder
    (variable synthesis hop) and reading the overlap-added result at the inverse
    ratio.  This preserves program duration while moving all partials by the same
    ratio.  Identity phase locking reduces the characteristic phase-vocoder
    "smearing", and an optional spectral-envelope correction keeps formants in
    place.

    All buffers are allocated in prepare().  process() performs no allocation,
    no locking and no file/network I/O, so it is suitable for the audio thread.
*/
class PhaseVocoderPitchShifter
{
public:
    PhaseVocoderPitchShifter();
    ~PhaseVocoderPitchShifter();

    void prepare (double newSampleRate, int fftOrder = 11);
    void reset() noexcept;
    bool isPrepared() const noexcept { return prepared; }

    void setTransposeSemitones (float semitones) noexcept;
    void setRatio (float linearRatio) noexcept;
    void setFormantPreserve (bool shouldPreserve) noexcept { formantPreserve = shouldPreserve; }
    bool getFormantPreserve() const noexcept { return formantPreserve; }

    void processMono (const float* input, float* output, int numSamples) noexcept;
    void processStereo (const float* inputL, const float* inputR,
                        float* outputL, float* outputR, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return initialDelay; }
    int getFftSize() const noexcept { return fftSize; }
    int getHopSize() const noexcept { return hopSize; }
    double getSampleRate() const noexcept { return sampleRate; }
    float getCurrentRatio() const noexcept { return ratioSmoothed; }

    static float semitonesToRatio (float semitones) noexcept
    {
        return std::pow (2.0f, semitones / 12.0f);
    }

private:
    void processInternal (const float* const* inputs, float* const* outputs, int numChannels, int numSamples) noexcept;
    void processFrame() noexcept;
    void computeEnvelope (const std::vector<float>& magnitudes, std::vector<float>& envelopeOut) noexcept;
    void addToOverlap (int channel, int frameOffset, const std::vector<float>& frame) noexcept;
    float readInterpolated (const std::vector<float>& buffer, double position) const noexcept;
    int wrapIndex (long long index) const noexcept;

    std::unique_ptr<juce::dsp::FFT> fft;
    double sampleRate = 48000.0;
    int fftOrder = 11;
    int fftSize = 2048;
    int hopSize = 512;
    int ringSize = 32768;
    int initialDelay = 0;
    bool prepared = false;
    bool formantPreserve = false;

    std::vector<float> window;
    std::vector<float> windowSquared;
    std::vector<float> envelopeLog;
    std::vector<float> envelopePrefix;

    std::array<std::vector<float>, 2> inputRing;
    std::array<std::vector<float>, 2> frameBuffer;
    std::array<std::vector<float>, 2> magnitude;
    std::array<std::vector<float>, 2> phase;
    std::array<std::vector<float>, 2> lastPhase;
    std::array<std::vector<float>, 2> synthesisPhase;
    std::array<std::vector<float>, 2> envelope;
    std::array<std::vector<float>, 2> fftBuffer;
    std::array<std::vector<float>, 2> overlapRing;

    std::vector<float> normalisationRing;
    std::vector<float> fftWorkingBuffer;
    std::vector<float> synthesisFrame;

    int inputWriteIndex = 0;
    int samplesSinceHop = 0;
    double writePosition = 0.0;
    double readPosition = 0.0;
    int startupSamplesRemaining = 0;
    float ratioTarget = 1.0f;
    float ratioSmoothed = 1.0f;
    float readRatio = 1.0f;

    std::vector<int> peakPositions;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseVocoderPitchShifter)
};

} // namespace rr
