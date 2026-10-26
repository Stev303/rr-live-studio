#include "DSP/Limiter.h"
#include "DSP/PitchShifter.h"

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
class DspTests final : public juce::UnitTest
{
public:
    DspTests() : juce::UnitTest ("DSP Integrity", "RrLiveStudio") {}

    void runTest() override
    {
        runLimiterTests();
        runPitchShifterTests();
    }

private:
    void runLimiterTests()
    {
        beginTest ("Limiter menjaga ceiling dan output tetap finite");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        constexpr float ceilingDb = -3.0f;

        rr::dsp::Limiter limiter;
        limiter.prepare (sampleRate, blockSize, true);
        limiter.setCeilingDb (ceilingDb);
        limiter.setReleaseMs (120.0f);
        limiter.setLookaheadMs (5.0f);

        const auto ceiling = juce::Decibels::decibelsToGain (ceilingDb);
        expectGreaterThan (limiter.getLatencySamples(), 0, "Lookahead harus menambah latency");

        std::vector<float> left ((size_t) blockSize, 0.0f);
        std::vector<float> right ((size_t) blockSize, 0.0f);

        double phase = 0.0;
        const auto phaseStep = juce::MathConstants<double>::twoPi * 997.0 / sampleRate;
        float maximum = 0.0f;
        bool finite = true;
        const auto blocks = (int) (sampleRate * 0.75 / (double) blockSize);

        for (int block = 0; block < blocks; ++block)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const auto value = (float) std::sin (phase);
                phase += phaseStep;
                left[(size_t) i] = value;
                right[(size_t) i] = value;
            }

            limiter.process (left.data(), right.data(), blockSize);

            if (block >= 10)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    finite = finite && std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i]);
                    maximum = juce::jmax (maximum, juce::jmax (std::abs (left[(size_t) i]),
                                                               std::abs (right[(size_t) i])));
                }
            }
        }

        expect (finite, "Output limiter tidak boleh mengandung NaN/Inf");
        expectLessOrEqual (maximum, ceiling * 1.02f, "Peak output harus dekat dengan ceiling");
        expectGreaterThan (maximum, ceiling * 0.5f, "Limiter tidak boleh membisukan sinyal");
        expectGreaterThan (limiter.getGainReductionDb(), 0.0f, "Gain reduction harus dilaporkan");

        beginTest ("Limiter menolak NaN dan Inf pada input");

        rr::dsp::Limiter nanLimiter;
        nanLimiter.prepare (sampleRate, blockSize, false);
        nanLimiter.setCeilingDb (ceilingDb);

        std::fill (left.begin(), left.end(), 0.25f);
        std::fill (right.begin(), right.end(), -0.25f);
        left[0] = std::numeric_limits<float>::quiet_NaN();
        right[1] = std::numeric_limits<float>::infinity();

        nanLimiter.process (left.data(), right.data(), blockSize);

        bool nanFinite = true;

        for (int i = 0; i < blockSize; ++i)
            nanFinite = nanFinite && std::isfinite (left[(size_t) i]) && std::isfinite (right[(size_t) i]);

        expect (nanFinite, "NaN/Inf input harus dinetralkan");
    }

    double measureFrequency (float transposeSemitones)
    {
        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;
        constexpr double inputHz = 220.0;
        constexpr double seconds = 1.6;

        rr::PhaseVocoderPitchShifter shifter;
        shifter.prepare (sampleRate, 11);
        shifter.setTransposeSemitones (transposeSemitones);
        shifter.reset();

        const auto totalSamples = (int) (sampleRate * seconds);
        const auto warmUpSamples = (int) (sampleRate * 0.45);

        std::vector<float> input ((size_t) blockSize);
        std::vector<float> output ((size_t) blockSize);
        std::vector<float> measured;

        int crossings = 0;
        double phase = 0.0;
        double sumSquares = 0.0;
        const auto phaseStep = juce::MathConstants<double>::twoPi * inputHz / sampleRate;
        int position = 0;
        float previous = 0.0f;

        while (position < totalSamples)
        {
            const auto count = juce::jmin (blockSize, totalSamples - position);

            for (int i = 0; i < count; ++i)
            {
                input[(size_t) i] = (float) (0.5 * std::sin (phase));
                phase += phaseStep;
            }

            shifter.processMono (input.data(), output.data(), count);

            for (int i = 0; i < count; ++i)
            {
                const auto sample = output[(size_t) i];

                if (position + i >= warmUpSamples)
                {
                    if (previous <= 0.0f && sample > 0.0f)
                        ++crossings;

                    measured.push_back (sample);
                    sumSquares += (double) sample * (double) sample;
                }

                previous = sample;
            }

            position += count;
        }

        expectGreaterThan (sumSquares, 1.0e-6, "Pitch shifter harus menghasilkan sinyal");

        const auto measuredSamples = (int) measured.size();
        const auto zeroCrossingHz = measuredSamples > 0
                                      ? (double) crossings * sampleRate / (double) measuredSamples
                                      : 0.0;

        constexpr int fftOrder = 15;
        constexpr int fftSize = 1 << fftOrder;
        const auto analysisSize = juce::jmin (fftSize, measuredSamples);

        if (analysisSize < 4096)
            return zeroCrossingHz;

        juce::dsp::FFT fft (fftOrder);
        std::vector<float> fftBuffer ((size_t) fftSize * 2, 0.0f);
        const auto firstSample = measuredSamples - analysisSize;

        for (int i = 0; i < analysisSize; ++i)
        {
            const auto window = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                        * (float) i / (float) analysisSize);
            fftBuffer[(size_t) i] = measured[(size_t) (firstSample + i)] * window;
        }

        fft.performFrequencyOnlyForwardTransform (fftBuffer.data());

        const auto binHz = sampleRate / (double) analysisSize;
        const auto lowBin = juce::jmax (1, (int) std::floor (100.0 / binHz));
        const auto highBin = juce::jmin (analysisSize / 2 - 2, (int) std::ceil (1000.0 / binHz));

        int peakBin = lowBin;

        for (int bin = lowBin + 1; bin <= highBin; ++bin)
            if (fftBuffer[(size_t) bin] > fftBuffer[(size_t) peakBin])
                peakBin = bin;

        auto peakOffset = 0.0;

        if (peakBin > 0 && peakBin < analysisSize / 2)
        {
            const auto left = std::log (fftBuffer[(size_t) (peakBin - 1)] + 1.0e-9f);
            const auto centre = std::log (fftBuffer[(size_t) peakBin] + 1.0e-9f);
            const auto right = std::log (fftBuffer[(size_t) (peakBin + 1)] + 1.0e-9f);
            const auto denominator = left - 2.0 * centre + right;

            if (std::abs (denominator) > 1.0e-9)
                peakOffset = 0.5 * (double) (left - right) / (double) denominator;
        }

        const auto peakHz = ((double) peakBin + peakOffset) * binHz;
        logMessage ("  " + juce::String (transposeSemitones, 1) + " st: peak "
                    + juce::String (peakHz, 2) + " Hz, zero-crossing "
                    + juce::String (zeroCrossingHz, 2) + " Hz");

        return peakHz;
    }

    void runPitchShifterTests()
    {
        beginTest ("Rasio semitone akurat");
        expectWithinAbsoluteError (rr::PhaseVocoderPitchShifter::semitonesToRatio (12.0f), 2.0f, 1.0e-5f);
        expectWithinAbsoluteError (rr::PhaseVocoderPitchShifter::semitonesToRatio (-12.0f), 0.5f, 1.0e-5f);

        beginTest ("Phase vocoder mempertahankan frekuensi pada 0 st");
        const auto identityHz = measureFrequency (0.0f);
        expectWithinAbsoluteError (identityHz, 220.0, 220.0 * 0.03,
                                   "Frekuensi output 0 st: " + juce::String (identityHz, 2) + " Hz");

        beginTest ("Phase vocoder menaikkan satu oktaf pada +12 st");
        const auto octaveHz = measureFrequency (12.0f);
        expectWithinAbsoluteError (octaveHz, 440.0, 440.0 * 0.05,
                                   "Frekuensi output +12 st: " + juce::String (octaveHz, 2) + " Hz");
    }
};

DspTests dspTests;
} // namespace
