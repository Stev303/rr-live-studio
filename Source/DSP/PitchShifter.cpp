#include "DSP/PitchShifter.h"

namespace rr
{
namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr float kPi = 3.14159265358979323846f;

    inline float wrapPhase (float value) noexcept
    {
        value = std::fmod (value + kPi, kTwoPi);

        if (value < 0.0f)
            value += kTwoPi;

        return value - kPi;
    }

    inline int nextPowerOfTwo (int value)
    {
        int result = 1;

        while (result < value)
            result <<= 1;

        return result;
    }
}

PhaseVocoderPitchShifter::PhaseVocoderPitchShifter() = default;
PhaseVocoderPitchShifter::~PhaseVocoderPitchShifter() = default;

void PhaseVocoderPitchShifter::prepare (double newSampleRate, int newFftOrder)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    fftOrder = juce::jlimit (9, 13, newFftOrder);
    fftSize = 1 << fftOrder;
    hopSize = fftSize / 4;
    ringSize = nextPowerOfTwo (fftSize * 8);
    initialDelay = fftSize + hopSize;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);
    windowSquared.resize ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
    {
        const auto w = 0.5f - 0.5f * std::cos (kTwoPi * (float) i / (float) fftSize);
        window[(size_t) i] = w;
        windowSquared[(size_t) i] = w * w;
    }

    const auto half = fftSize / 2 + 1;
    envelopeLog.assign ((size_t) half, 0.0f);
    envelopePrefix.assign ((size_t) half + 1, 0.0f);
    fftWorkingBuffer.assign ((size_t) fftSize * 2, 0.0f);
    synthesisFrame.assign ((size_t) fftSize, 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        inputRing[(size_t) ch].assign ((size_t) fftSize, 0.0f);
        frameBuffer[(size_t) ch].assign ((size_t) fftSize, 0.0f);
        magnitude[(size_t) ch].assign ((size_t) half, 0.0f);
        phase[(size_t) ch].assign ((size_t) half, 0.0f);
        lastPhase[(size_t) ch].assign ((size_t) half, 0.0f);
        synthesisPhase[(size_t) ch].assign ((size_t) half, 0.0f);
        envelope[(size_t) ch].assign ((size_t) half, 1.0f);
        fftBuffer[(size_t) ch].assign ((size_t) fftSize * 2, 0.0f);
        overlapRing[(size_t) ch].assign ((size_t) ringSize, 0.0f);
    }

    normalisationRing.assign ((size_t) ringSize, 0.0f);
    peakPositions.reserve ((size_t) half);
    prepared = true;
    reset();
    ratioSmoothed = ratioTarget;
    readRatio = ratioSmoothed;
}

void PhaseVocoderPitchShifter::reset() noexcept
{
    if (! prepared)
        return;

    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (inputRing[(size_t) ch].begin(), inputRing[(size_t) ch].end(), 0.0f);
        std::fill (frameBuffer[(size_t) ch].begin(), frameBuffer[(size_t) ch].end(), 0.0f);
        std::fill (magnitude[(size_t) ch].begin(), magnitude[(size_t) ch].end(), 0.0f);
        std::fill (phase[(size_t) ch].begin(), phase[(size_t) ch].end(), 0.0f);
        std::fill (lastPhase[(size_t) ch].begin(), lastPhase[(size_t) ch].end(), 0.0f);
        std::fill (synthesisPhase[(size_t) ch].begin(), synthesisPhase[(size_t) ch].end(), 0.0f);
        std::fill (fftBuffer[(size_t) ch].begin(), fftBuffer[(size_t) ch].end(), 0.0f);
        std::fill (overlapRing[(size_t) ch].begin(), overlapRing[(size_t) ch].end(), 0.0f);
    }

    std::fill (normalisationRing.begin(), normalisationRing.end(), 0.0f);
    inputWriteIndex = 0;
    samplesSinceHop = 0;
    writePosition = 0.0;
    readPosition = 0.0;
    startupSamplesRemaining = initialDelay;
    ratioSmoothed = juce::jlimit (0.25f, 4.0f, ratioTarget);
    readRatio = ratioSmoothed;
}

void PhaseVocoderPitchShifter::setTransposeSemitones (float semitones) noexcept
{
    setRatio (semitonesToRatio (juce::jlimit (-24.0f, 24.0f, semitones)));
}

void PhaseVocoderPitchShifter::setRatio (float linearRatio) noexcept
{
    ratioTarget = juce::jlimit (0.25f, 4.0f, linearRatio);
}

void PhaseVocoderPitchShifter::processMono (const float* input, float* output, int numSamples) noexcept
{
    const float* inputs[1] = { input };
    float* outputs[1] = { output };
    processInternal (inputs, outputs, 1, numSamples);
}

void PhaseVocoderPitchShifter::processStereo (const float* inputL, const float* inputR,
                                              float* outputL, float* outputR, int numSamples) noexcept
{
    const float* inputs[2] = { inputL, inputR };
    float* outputs[2] = { outputL, outputR };
    processInternal (inputs, outputs, 2, numSamples);
}

int PhaseVocoderPitchShifter::wrapIndex (long long index) const noexcept
{
    auto wrapped = index % ringSize;

    if (wrapped < 0)
        wrapped += ringSize;

    return (int) wrapped;
}

float PhaseVocoderPitchShifter::readInterpolated (const std::vector<float>& buffer, double position) const noexcept
{
    const auto base = (long long) std::floor (position);
    const auto fraction = (float) (position - (double) base);

    const auto xm1 = buffer[(size_t) wrapIndex (base - 1)];
    const auto x0  = buffer[(size_t) wrapIndex (base)];
    const auto x1  = buffer[(size_t) wrapIndex (base + 1)];
    const auto x2  = buffer[(size_t) wrapIndex (base + 2)];

    const auto c0 = x0;
    const auto c1 = 0.5f * (x1 - xm1);
    const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);

    return ((c3 * fraction + c2) * fraction + c1) * fraction + c0;
}

void PhaseVocoderPitchShifter::processInternal (const float* const* inputs, float* const* outputs,
                                                int numChannels, int numSamples) noexcept
{
    if (! prepared || inputs == nullptr || outputs == nullptr)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);

        return;
    }

    numChannels = juce::jlimit (1, 2, numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto value = inputs[ch] != nullptr ? inputs[ch][i] : 0.0f;
            inputRing[(size_t) ch][(size_t) inputWriteIndex] = std::isfinite (value) ? value : 0.0f;
        }

        inputWriteIndex = (inputWriteIndex + 1) % fftSize;

        if (++samplesSinceHop >= hopSize)
        {
            samplesSinceHop = 0;
            processFrame();
        }

        const bool warmingUp = startupSamplesRemaining > 0;

        if (warmingUp)
            --startupSamplesRemaining;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float sample = 0.0f;

            if (! warmingUp)
            {
                const auto normalisation = readInterpolated (normalisationRing, readPosition);

                if (normalisation > 1.0e-4f)
                    sample = readInterpolated (overlapRing[(size_t) ch], readPosition) / normalisation;
            }

            if (outputs[ch] != nullptr)
                outputs[ch][i] = std::isfinite (sample) ? juce::jlimit (-8.0f, 8.0f, sample) : 0.0f;
        }

        if (! warmingUp)
            readPosition += (double) readRatio;
    }
}

void PhaseVocoderPitchShifter::computeEnvelope (const std::vector<float>& magnitudes, std::vector<float>& envelopeOut) noexcept
{
    const auto half = fftSize / 2 + 1;

    for (int k = 0; k < half; ++k)
        envelopeLog[(size_t) k] = std::log (magnitudes[(size_t) k] + 1.0e-9f);

    envelopePrefix[0] = 0.0f;

    for (int k = 0; k < half; ++k)
        envelopePrefix[(size_t) k + 1] = envelopePrefix[(size_t) k] + envelopeLog[(size_t) k];

    const auto width = juce::jmax (3, fftSize / 256);

    for (int k = 0; k < half; ++k)
    {
        const auto low = juce::jmax (0, k - width);
        const auto high = juce::jmin (half - 1, k + width);
        const auto sum = envelopePrefix[(size_t) high + 1] - envelopePrefix[(size_t) low];
        const auto count = (float) (high - low + 1);
        envelopeOut[(size_t) k] = std::exp (sum / count);
    }
}

void PhaseVocoderPitchShifter::addToOverlap (int channel, int frameOffset, const std::vector<float>& frameData) noexcept
{
    auto& ring = overlapRing[(size_t) channel];
    const auto position = writePosition + (double) frameOffset;
    const auto base = (long long) std::floor (position);
    const auto fraction = (float) (position - (double) base);
    const auto index0 = wrapIndex (base);
    const auto index1 = wrapIndex (base + 1);
    const auto sample = frameData[(size_t) frameOffset];

    ring[(size_t) index0] += sample * (1.0f - fraction);
    ring[(size_t) index1] += sample * fraction;

    if (channel == 0)
    {
        const auto weight = windowSquared[(size_t) frameOffset];
        normalisationRing[(size_t) index0] += weight * (1.0f - fraction);
        normalisationRing[(size_t) index1] += weight * fraction;
    }
}

void PhaseVocoderPitchShifter::processFrame() noexcept
{
    if (! fft)
        return;

    ratioSmoothed += (ratioTarget - ratioSmoothed) * 0.25f;
    ratioSmoothed = juce::jlimit (0.25f, 4.0f, ratioSmoothed);
    readRatio = ratioSmoothed;

    const auto half = fftSize / 2 + 1;
    const auto ratio = ratioSmoothed;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto& fftData = fftBuffer[(size_t) ch];
        auto& frame = frameBuffer[(size_t) ch];
        std::fill (fftData.begin(), fftData.end(), 0.0f);

        for (int j = 0; j < fftSize; ++j)
        {
            const auto index = (inputWriteIndex + j) % fftSize;
            frame[(size_t) j] = inputRing[(size_t) ch][(size_t) index] * window[(size_t) j];
            fftData[(size_t) j] = frame[(size_t) j];
        }

        fft->performRealOnlyForwardTransform (fftData.data(), false);

        auto& mags = magnitude[(size_t) ch];
        auto& phases = phase[(size_t) ch];
        auto& lastPhases = lastPhase[(size_t) ch];
        auto& synthPhases = synthesisPhase[(size_t) ch];
        auto& env = envelope[(size_t) ch];

        for (int k = 0; k < half; ++k)
        {
            const auto real = fftData[(size_t) (k * 2)];
            const auto imag = fftData[(size_t) (k * 2 + 1)];
            mags[(size_t) k] = std::sqrt (real * real + imag * imag);
            phases[(size_t) k] = std::atan2 (imag, real);
        }

        if (formantPreserve)
            computeEnvelope (mags, env);

        for (int k = 1; k < half; ++k)
        {
            const auto expected = kTwoPi * (float) k * (float) hopSize / (float) fftSize;
            const auto deviation = wrapPhase (phases[(size_t) k] - lastPhases[(size_t) k] - expected);
            // Frames are written hopSize * ratio apart, so the phase must advance at
            // the same time-stretch factor or the partials drift out of tune.
            synthPhases[(size_t) k] += (expected + deviation) * ratio;
            lastPhases[(size_t) k] = phases[(size_t) k];
        }

        synthPhases[0] = 0.0f;
        lastPhases[0] = phases[0];

        peakPositions.clear();
        auto maxMagnitude = 0.0f;

        for (int k = 1; k < half - 1; ++k)
            maxMagnitude = juce::jmax (maxMagnitude, mags[(size_t) k]);

        const auto peakThreshold = maxMagnitude * 0.001f;

        for (int k = 1; k < half - 1; ++k)
        {
            const auto m = mags[(size_t) k];

            if (m > peakThreshold && m > mags[(size_t) (k - 1)] && m >= mags[(size_t) (k + 1)])
                peakPositions.push_back (k);
        }

        int peakCursor = 0;

        for (int k = 0; k < half; ++k)
        {
            auto finalPhase = synthPhases[(size_t) k];

            if (! peakPositions.empty())
            {
                while (peakCursor + 1 < (int) peakPositions.size()
                       && std::abs (peakPositions[(size_t) (peakCursor + 1)] - k)
                            <= std::abs (peakPositions[(size_t) peakCursor] - k))
                    ++peakCursor;

                const auto peak = peakPositions[(size_t) peakCursor];

                if (std::abs (peak - k) <= 16)
                    finalPhase = synthPhases[(size_t) peak] + (phases[(size_t) k] - phases[(size_t) peak]);
            }

            auto magnitudeValue = mags[(size_t) k];

            if (formantPreserve && std::abs (ratio - 1.0f) > 1.0e-4f)
            {
                const auto shiftedBin = juce::jlimit (0.0f, (float) (half - 1), (float) k * ratio);
                const auto lower = (int) shiftedBin;
                const auto upper = juce::jmin (half - 1, lower + 1);
                const auto fraction = shiftedBin - (float) lower;
                const auto shiftedEnvelope = env[(size_t) lower] * (1.0f - fraction) + env[(size_t) upper] * fraction;
                const auto scale = juce::jlimit (0.2f, 5.0f, shiftedEnvelope / juce::jmax (1.0e-9f, env[(size_t) k]));
                magnitudeValue *= scale;
            }

            const auto realOut = magnitudeValue * std::cos (finalPhase);
            auto imagOut = magnitudeValue * std::sin (finalPhase);

            if (k == 0 || k == half - 1)
                imagOut = 0.0f;

            fftData[(size_t) (k * 2)] = realOut;
            fftData[(size_t) (k * 2 + 1)] = imagOut;

            if (k > 0 && k < half - 1)
            {
                const auto mirror = fftSize - k;
                fftData[(size_t) (mirror * 2)] = realOut;
                fftData[(size_t) (mirror * 2 + 1)] = -imagOut;
            }
        }

        fft->performRealOnlyInverseTransform (fftData.data());

        for (int j = 0; j < fftSize; ++j)
            synthesisFrame[(size_t) j] = fftData[(size_t) j] * window[(size_t) j];

        for (int j = 0; j < fftSize; ++j)
            addToOverlap (ch, j, synthesisFrame);
    }

    writePosition += (double) hopSize * (double) ratioSmoothed;
}

} // namespace rr
