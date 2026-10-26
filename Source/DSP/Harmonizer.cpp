#include "DSP/Harmonizer.h"
#include <cmath>

namespace rr
{
namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr float kMinRatio = 0.25f;
    constexpr float kMaxRatio = 4.0f;

    inline float onePoleCoefficient (float milliseconds, double sampleRate, int blockSize) noexcept
    {
        const auto seconds = juce::jmax (0.001f, milliseconds) * 0.001f;
        return 1.0f - std::exp (-(float) blockSize / ((float) sampleRate * seconds));
    }
}

Harmonizer::Harmonizer (ParamStore& parameters)
    : params (parameters)
{
}

void Harmonizer::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlockSize = juce::jmax (16, maximumBlockSize);

    // fftOrder 10 keeps the harmony voices below ~30 ms of latency at 48 kHz.
    voice1.prepare (sampleRate, 10);
    voice2.prepare (sampleRate, 10);

    wet1L.assign ((size_t) maxBlockSize, 0.0f);
    wet1R.assign ((size_t) maxBlockSize, 0.0f);
    wet2L.assign ((size_t) maxBlockSize, 0.0f);
    wet2R.assign ((size_t) maxBlockSize, 0.0f);

    reset();
}

void Harmonizer::reset() noexcept
{
    voice1.reset();
    voice2.reset();
    smoothedRatio1 = 1.0f;
    smoothedRatio2 = 1.0f;
    lastShift1 = 0.0f;
    lastShift2 = 0.0f;
    humanizePhase = 0.0f;
    gain1 = 0.0f;
    gain2 = 0.0f;
    wetMixture = 0.0f;
    active = false;
    wasEnabled = false;
    shift1Semitones.store (0.0f, std::memory_order_relaxed);
    shift2Semitones.store (0.0f, std::memory_order_relaxed);
    wetAmount.store (0.0f, std::memory_order_relaxed);
}

float Harmonizer::computeVoiceRatio (int intervalParam, float customSemitones,
                                     float detectedMidi, int key, int scale) const noexcept
{
    const auto detectedRounded = (int) std::lround (detectedMidi);
    const auto snappedBase = music::nearestScaleNote (detectedRounded, key, scale);

    int targetNote = snappedBase;

    if (intervalParam == 3)
        targetNote = snappedBase + (int) std::lround (customSemitones);
    else
        targetNote = music::transposeNoteByScaleSteps (snappedBase, key, scale, music::intervalToScaleSteps (intervalParam));

    const auto detectedFrequency = juce::jmax (1.0f, 440.0f * std::pow (2.0f, (detectedMidi - 69.0f) / 12.0f));
    const auto targetFrequency = 440.0f * std::pow (2.0f, ((float) targetNote - 69.0f) / 12.0f);

    return juce::jlimit (kMinRatio, kMaxRatio, targetFrequency / detectedFrequency);
}

void Harmonizer::updateShifterParameters (PhaseVocoderPitchShifter& shifter,
                                          float targetRatio, float humanizeCents) noexcept
{
    const auto humanised = targetRatio * std::pow (2.0f, humanizeCents / 1200.0f);
    shifter.setRatio (juce::jlimit (kMinRatio, kMaxRatio, humanised));
    shifter.setFormantPreserve (params.getBool (ParamId::formantPreserve));
}

void Harmonizer::applyStereoWidth (float* left, float* right, int numSamples, float width) noexcept
{
    const auto sideGain = juce::jlimit (0.0f, 1.5f, width);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto mid = 0.5f * (left[i] + right[i]);
        const auto side = 0.5f * (left[i] - right[i]) * sideGain;
        left[i] = mid + side;
        right[i] = mid - side;
    }
}

void Harmonizer::process (const float* inputL, const float* inputR,
                          float* outputL, float* outputR,
                          int numSamples,
                          float detectedMidiNote,
                          float pitchConfidence,
                          bool pitchIsVoiced) noexcept
{
    if (numSamples <= 0 || outputL == nullptr || outputR == nullptr)
        return;

    const auto enabled = params.getBool (ParamId::harmonyEnabled) && ! params.getBool (ParamId::harmonyBypass);
    const auto level = juce::jlimit (0.0f, 1.0f, params.get (ParamId::harmonyLevel) * 0.01f);
    const auto dryWet = juce::jlimit (0.0f, 1.0f, params.get (ParamId::harmonyDryWet) * 0.01f);
    const auto width = juce::jlimit (0.0f, 1.0f, params.get (ParamId::stereoWidth) * 0.01f);
    const auto humanize = juce::jlimit (0.0f, 1.0f, params.get (ParamId::humanize) * 0.01f);
    const auto retuneMs = params.get (ParamId::retuneSpeedMs);
    const auto tracking = juce::jlimit (0.0f, 1.0f, params.get (ParamId::trackingStrength) * 0.01f);
    const auto key = params.getInt (ParamId::hKey);
    const auto scale = params.getInt (ParamId::hScale);
    const auto voice1On = params.getBool (ParamId::hVoice1On);
    const auto voice2On = params.getBool (ParamId::hVoice2On);
    const auto interval1 = params.getInt (ParamId::hVoice1Interval);
    const auto interval2 = params.getInt (ParamId::hVoice2Interval);
    const auto custom1 = params.get (ParamId::hVoice1Custom);
    const auto custom2 = params.get (ParamId::hVoice2Custom);

    const auto targetWet = enabled ? dryWet : 0.0f;
    const auto wetCoeff = onePoleCoefficient (20.0f, sampleRate, numSamples);
    wetMixture += (targetWet - wetMixture) * wetCoeff;

    if (wetMixture < 1.0e-4f)
        wetMixture = 0.0f;

    const auto nowActive = wetMixture > 0.0f;

    if (! nowActive)
    {
        if (! inputL || ! inputR)
        {
            juce::FloatVectorOperations::clear (outputL, numSamples);
            juce::FloatVectorOperations::clear (outputR, numSamples);
            active = false;
            return;
        }

        if (outputL != inputL)
            juce::FloatVectorOperations::copy (outputL, inputL, numSamples);

        if (outputR != inputR)
            juce::FloatVectorOperations::copy (outputR, inputR, numSamples);

        if (wasEnabled && ! enabled)
        {
            voice1.reset();
            voice2.reset();
            gain1 = gain2 = 0.0f;
        }

        wasEnabled = enabled;
        active = false;
        wetAmount.store (0.0f, std::memory_order_relaxed);
        return;
    }

    if (! active)
    {
        voice1.reset();
        voice2.reset();
        smoothedRatio1 = 1.0f;
        smoothedRatio2 = 1.0f;
        gain1 = 0.0f;
        gain2 = 0.0f;
        wasEnabled = true;
    }

    const auto targetGain1 = (voice1On ? level : 0.0f) * (pitchIsVoiced ? 1.0f : 0.0f);
    const auto targetGain2 = (voice2On ? level : 0.0f) * (pitchIsVoiced ? 1.0f : 0.0f);
    const auto gainCoeff = onePoleCoefficient (25.0f, sampleRate, numSamples);
    gain1 += (targetGain1 - gain1) * gainCoeff;
    gain2 += (targetGain2 - gain2) * gainCoeff;

    if (pitchIsVoiced && pitchConfidence > 0.15f && detectedMidiNote > 1.0f)
    {
        auto targetRatio1 = computeVoiceRatio (interval1, custom1, detectedMidiNote, key, scale);
        auto targetRatio2 = computeVoiceRatio (interval2, custom2, detectedMidiNote, key, scale);

        targetRatio1 = 1.0f + (targetRatio1 - 1.0f) * tracking;
        targetRatio2 = 1.0f + (targetRatio2 - 1.0f) * tracking;

        const auto ratioCoeff = retuneMs <= 0.0f
                                    ? 1.0f
                                    : juce::jmin (1.0f, onePoleCoefficient (retuneMs, sampleRate, numSamples));

        smoothedRatio1 += (targetRatio1 - smoothedRatio1) * ratioCoeff;
        smoothedRatio2 += (targetRatio2 - smoothedRatio2) * ratioCoeff;
    }

    humanizePhase += (float) numSamples * kTwoPi / (float) sampleRate;

    if (humanizePhase > kTwoPi)
        humanizePhase -= kTwoPi;

    const auto depthCents = humanize * 14.0f;
    const auto humanize1 = depthCents * std::sin (humanizePhase * 0.13f);
    const auto humanize2 = depthCents * std::sin (humanizePhase * 0.17f + 2.1f);

    updateShifterParameters (voice1, smoothedRatio1, humanize1);
    updateShifterParameters (voice2, smoothedRatio2, humanize2);

    lastShift1 = 12.0f * std::log2 (juce::jmax (1.0e-6f, smoothedRatio1));
    lastShift2 = 12.0f * std::log2 (juce::jmax (1.0e-6f, smoothedRatio2));

    int offset = 0;

    while (offset < numSamples)
    {
        const auto chunk = juce::jmin (maxBlockSize, numSamples - offset);
        const auto* inL = inputL != nullptr ? inputL + offset : nullptr;
        const auto* inR = inputR != nullptr ? inputR + offset : nullptr;
        auto* outL = outputL + offset;
        auto* outR = outputR + offset;

        voice1.processStereo (inL, inR, wet1L.data(), wet1R.data(), chunk);
        voice2.processStereo (inL, inR, wet2L.data(), wet2R.data(), chunk);

        const auto dryGain = 1.0f - wetMixture;

        for (int i = 0; i < chunk; ++i)
        {
            wet1L[(size_t) i] *= gain1;
            wet1R[(size_t) i] *= gain1;
            wet2L[(size_t) i] *= gain2;
            wet2R[(size_t) i] *= gain2;
        }

        applyStereoWidth (wet1L.data(), wet1R.data(), chunk, width * 0.5f);
        applyStereoWidth (wet2L.data(), wet2R.data(), chunk, width * 0.5f);

        for (int i = 0; i < chunk; ++i)
        {
            const auto harmonyL = (wet1L[(size_t) i] + wet2L[(size_t) i]) * wetMixture;
            const auto harmonyR = (wet1R[(size_t) i] + wet2R[(size_t) i]) * wetMixture;

            const auto dryL = (inL != nullptr ? inL[i] : 0.0f) * dryGain;
            const auto dryR = (inR != nullptr ? inR[i] : 0.0f) * dryGain;

            outL[i] = juce::jlimit (-4.0f, 4.0f, dryL + harmonyL);
            outR[i] = juce::jlimit (-4.0f, 4.0f, dryR + harmonyR);
        }

        offset += chunk;
    }

    active = true;
    wasEnabled = true;
    shift1Semitones.store (lastShift1, std::memory_order_relaxed);
    shift2Semitones.store (lastShift2, std::memory_order_relaxed);
    wetAmount.store (wetMixture, std::memory_order_relaxed);
}

} // namespace rr
