#include "DSP/VocalChain.h"
#include "Utilities/MusicTheory.h"
#include <cmath>

namespace rr
{
namespace
{
    inline float timeCoefficient (float milliseconds, double sampleRate) noexcept
    {
        const auto seconds = juce::jmax (0.00005, (double) milliseconds * 0.001);
        return (float) std::exp (-1.0 / (seconds * sampleRate));
    }

    inline float onePoleBlockCoefficient (float milliseconds, double sampleRate, int blockSize) noexcept
    {
        const auto seconds = juce::jmax (0.001f, milliseconds) * 0.001f;
        return 1.0f - std::exp (-(float) blockSize / ((float) sampleRate * seconds));
    }
}

VocalChain::VocalChain (ParamStore& parameters)
    : params (parameters)
    , harmonizer (parameters)
    , smartEq (parameters)
{
}

void VocalChain::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlockSize = juce::jmax (32, maximumBlockSize);

    inputGainSmoother.reset (sampleRate, 0.02);
    faderSmoother.reset (sampleRate, 0.02);

    compressor.prepare (sampleRate);
    pitchDetector.prepare (sampleRate);
    corrector.prepare (sampleRate, 10);
    corrector.setFormantPreserve (true);

    correctionL.assign ((size_t) maxBlockSize, 0.0f);
    correctionR.assign ((size_t) maxBlockSize, 0.0f);
    pitchMono.assign ((size_t) maxBlockSize, 0.0f);

    harmonizer.prepare (sampleRate, maxBlockSize);
    smartEq.prepare (sampleRate);

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    delayBufferLength = (int) std::ceil (sampleRate * 1.05) + 4;
    delayLeft.assign ((size_t) delayBufferLength, 0.0f);
    delayRight.assign ((size_t) delayBufferLength, 0.0f);

    updateFilters();
    reset();
}

void VocalChain::reset() noexcept
{
    inputGainSmoother.setCurrentAndTargetValue (dbToGain (params.get (ParamId::micGainDb)));
    faderSmoother.setCurrentAndTargetValue (dbToGain (params.get (ParamId::vocalFaderDb)));

    gateEnvelope = 0.0f;
    gateGain = 1.0f;
    gateHoldCounter = 0;
    gateOpenState = false;

    for (auto& filter : hpfFilters)
        filter.reset();

    for (auto& band : eqFilters)
        for (auto& filter : band)
            filter.reset();

    for (auto& filter : deEsserLowPass)
        filter.reset();

    deEsserEnvelope = 0.0f;
    deEsserGain = 1.0f;
    compressor.reset();
    pitchDetector.reset();
    corrector.reset();
    correctionRatio = 1.0f;
    correctionWasEnabled = false;
    harmonizer.reset();
    smartEq.reset();
    saturationCurrent = 0.0f;
    reverb.reset();
    reverbWasEnabled = false;

    if (! delayLeft.empty())
    {
        std::fill (delayLeft.begin(), delayLeft.end(), 0.0f);
        std::fill (delayRight.begin(), delayRight.end(), 0.0f);
    }

    delayWriteIndex = 0;
    smoothedDelaySamples = 0.0f;

    inputPeakDb.store (-100.0f, std::memory_order_relaxed);
    outputPeakDb.store (-100.0f, std::memory_order_relaxed);
    compGrDb.store (0.0f, std::memory_order_relaxed);
    deEsserGrDb.store (0.0f, std::memory_order_relaxed);
    appliedCorrectionSemitones.store (0.0f, std::memory_order_relaxed);
    gateOpen.store (false, std::memory_order_relaxed);
    clipped.store (false, std::memory_order_relaxed);
}

void VocalChain::updateFilters() noexcept
{
    const auto hpfHz = juce::jlimit (20.0f, 400.0f, params.get (ParamId::hpfHz));
    const auto hpfCoefficients = params.getBool (ParamId::hpfEnabled)
                                     ? dsp::BiquadCoefficients::highPass (sampleRate, (double) hpfHz, 0.7071)
                                     : dsp::BiquadCoefficients::unity();

    for (auto& filter : hpfFilters)
        filter.setCoefficients (hpfCoefficients);

    lastHpfHz = hpfHz;

    const auto smoothing = 0.22f;

    for (int band = 0; band < 5; ++band)
    {
        const auto base = (size_t) band * 3;
        const auto frequencyId = SmartEQ::eqParamIds[base];
        const auto gainId = SmartEQ::eqParamIds[base + 1];
        const auto qId = SmartEQ::eqParamIds[base + 2];

        eqFrequencies[(size_t) band] += (params.get (frequencyId) - eqFrequencies[(size_t) band]) * smoothing;
        eqGains[(size_t) band] += (params.get (gainId) - eqGains[(size_t) band]) * smoothing;
        eqQs[(size_t) band] += (params.get (qId) - eqQs[(size_t) band]) * smoothing;

        const auto coefficients = params.getBool (ParamId::eqEnabled)
                                      ? dsp::BiquadCoefficients::peaking (sampleRate,
                                                                          (double) eqFrequencies[(size_t) band],
                                                                          (double) eqQs[(size_t) band],
                                                                          (double) eqGains[(size_t) band])
                                      : dsp::BiquadCoefficients::unity();

        for (auto& filter : eqFilters[(size_t) band])
            filter.setCoefficients (coefficients);
    }

    const auto deEsserCoefficients = params.getBool (ParamId::deEsserEnabled)
                                         ? dsp::BiquadCoefficients::lowPass (sampleRate,
                                                                             (double) params.get (ParamId::deEsserFreqHz),
                                                                             0.7071)
                                         : dsp::BiquadCoefficients::unity();

    for (auto& filter : deEsserLowPass)
        filter.setCoefficients (deEsserCoefficients);
}

void VocalChain::processGate (float* left, float* right, int numSamples) noexcept
{
    const auto enabled = params.getBool (ParamId::gateEnabled);
    const auto thresholdDb = params.get (ParamId::gateThresholdDb);
    const auto thresholdGain = dbToGain (thresholdDb);
    const auto floorGain = dbToGain (-22.0f);
    const auto holdSamples = (int) (sampleRate * 0.06);
    const auto envelopeAttack = timeCoefficient (4.0f, sampleRate);
    const auto envelopeRelease = timeCoefficient (140.0f, sampleRate);
    const auto openCoefficient = 1.0f - timeCoefficient (2.0f, sampleRate);
    const auto closeCoefficient = 1.0f - timeCoefficient (90.0f, sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto peak = juce::jmax (std::abs (left[i]), std::abs (right[i]));
        const auto coefficient = peak > gateEnvelope ? envelopeAttack : envelopeRelease;
        gateEnvelope = coefficient * gateEnvelope + (1.0f - coefficient) * peak;

        bool shouldOpen = true;

        if (enabled)
        {
            if (gateEnvelope >= thresholdGain)
            {
                gateHoldCounter = holdSamples;
                shouldOpen = true;
            }
            else if (gateHoldCounter > 0)
            {
                --gateHoldCounter;
                shouldOpen = true;
            }
            else
            {
                shouldOpen = false;
            }
        }

        gateOpenState = shouldOpen;

        const auto target = enabled && ! shouldOpen ? floorGain : 1.0f;
        const auto gainCoefficient = target > gateGain ? openCoefficient : closeCoefficient;
        gateGain += (target - gateGain) * gainCoefficient;

        left[i] *= gateGain;
        right[i] *= gateGain;
    }

    gateOpen.store (enabled ? gateOpenState : true, std::memory_order_relaxed);
}

void VocalChain::processDeEsser (float* left, float* right, int numSamples) noexcept
{
    if (! params.getBool (ParamId::deEsserEnabled))
    {
        deEsserEnvelope = 0.0f;
        deEsserGain = 1.0f;
        deEsserGrDb.store (0.0f, std::memory_order_relaxed);
        return;
    }

    const auto thresholdDb = params.get (ParamId::deEsserThresholdDb);
    const auto attack = timeCoefficient (0.8f, sampleRate);
    const auto release = timeCoefficient (55.0f, sampleRate);
    auto maximumReduction = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto lowL = deEsserLowPass[0].processSample (left[i]);
        const auto lowR = deEsserLowPass[1].processSample (right[i]);
        const auto highL = left[i] - lowL;
        const auto highR = right[i] - lowR;

        const auto highPeak = juce::jmax (std::abs (highL), std::abs (highR));
        const auto coefficient = highPeak > deEsserEnvelope ? attack : release;
        deEsserEnvelope = coefficient * deEsserEnvelope + (1.0f - coefficient) * highPeak;

        const auto envelopeDb = juce::Decibels::gainToDecibels (deEsserEnvelope, -100.0f);
        const auto reductionDb = juce::jlimit (0.0f, 14.0f, (envelopeDb - thresholdDb) * 0.8f);
        maximumReduction = juce::jmax (maximumReduction, reductionDb);

        const auto targetGain = dbToGain (-reductionDb);
        deEsserGain = targetGain < deEsserGain ? targetGain
                                               : deEsserGain + (targetGain - deEsserGain) * 0.25f;

        left[i] = lowL + highL * deEsserGain;
        right[i] = lowR + highR * deEsserGain;
    }

    deEsserGrDb.store (maximumReduction, std::memory_order_relaxed);
}

void VocalChain::processSaturation (float* left, float* right, int numSamples) noexcept
{
    if (! params.getBool (ParamId::satEnabled))
        return;

    const auto driveAmount = juce::jlimit (0.0f, 1.0f, params.get (ParamId::satDrive) * 0.01f);
    const auto mix = juce::jlimit (0.0f, 1.0f, params.get (ParamId::satMix) * 0.01f);
    const auto drive = 1.0f + driveAmount * 9.0f;
    const auto target = drive;
    saturationCurrent += (target - saturationCurrent) * 0.2f;
    const auto makeup = 1.0f / std::tanh (juce::jmax (1.0f, saturationCurrent));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto wetL = std::tanh (left[i] * saturationCurrent) * makeup;
        const auto wetR = std::tanh (right[i] * saturationCurrent) * makeup;
        left[i] += (wetL - left[i]) * mix;
        right[i] += (wetR - right[i]) * mix;
    }
}

void VocalChain::updateReverbParameters() noexcept
{
    const auto mix = juce::jlimit (0.0f, 1.0f, params.get (ParamId::reverbMix) * 0.01f);
    const auto size = juce::jlimit (0.0f, 1.0f, params.get (ParamId::reverbSize) * 0.01f);
    const auto damping = juce::jlimit (0.0f, 1.0f, params.get (ParamId::reverbDamp) * 0.01f);

    juce::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = juce::jlimit (0.0f, 1.0f, 0.15f + size * 0.85f);
    reverbParameters.damping = damping;
    reverbParameters.wetLevel = mix * 0.85f;
    reverbParameters.dryLevel = 1.0f - mix * 0.5f;
    reverbParameters.width = 1.0f;
    reverbParameters.freezeMode = 0.0f;
    reverb.setParameters (reverbParameters);
}

void VocalChain::processDelay (float* left, float* right, int numSamples) noexcept
{
    if (! params.getBool (ParamId::delayEnabled))
    {
        if (delayWasEnabled)
        {
            std::fill (delayLeft.begin(), delayLeft.end(), 0.0f);
            std::fill (delayRight.begin(), delayRight.end(), 0.0f);
            delayWriteIndex = 0;
            delayWasEnabled = false;
        }

        return;
    }

    delayWasEnabled = true;

    const auto delayTimeMs = juce::jlimit (40.0f, 800.0f, params.get (ParamId::delayTimeMs));
    const auto targetDelaySamples = (float) (delayTimeMs * 0.001 * sampleRate);
    const auto smoothing = onePoleBlockCoefficient (50.0f, sampleRate, numSamples);
    smoothedDelaySamples += (targetDelaySamples - smoothedDelaySamples) * smoothing;

    if (smoothedDelaySamples < 1.0f)
        smoothedDelaySamples = targetDelaySamples;

    const auto feedback = juce::jlimit (0.0f, 0.8f, params.get (ParamId::delayFeedback) * 0.01f);
    const auto mix = juce::jlimit (0.0f, 1.0f, params.get (ParamId::delayMix) * 0.01f);

    for (int i = 0; i < numSamples; ++i)
    {
        auto readPosition = (double) delayWriteIndex - (double) smoothedDelaySamples;

        while (readPosition < 0.0)
            readPosition += (double) delayBufferLength;

        const auto index0 = (int) readPosition % delayBufferLength;
        const auto index1 = (index0 + 1) % delayBufferLength;
        const auto fraction = (float) (readPosition - std::floor (readPosition));

        const auto delayedL = delayLeft[(size_t) index0] * (1.0f - fraction) + delayLeft[(size_t) index1] * fraction;
        const auto delayedR = delayRight[(size_t) index0] * (1.0f - fraction) + delayRight[(size_t) index1] * fraction;

        delayLeft[(size_t) delayWriteIndex] = left[i] + delayedL * feedback;
        delayRight[(size_t) delayWriteIndex] = right[i] + delayedR * feedback;

        left[i] = left[i] * (1.0f - mix) + delayedL * mix;
        right[i] = right[i] * (1.0f - mix) + delayedR * mix;

        if (++delayWriteIndex >= delayBufferLength)
            delayWriteIndex = 0;
    }
}

float VocalChain::targetCorrectionRatio (float detectedMidi, float confidence) const noexcept
{
    if (confidence < 0.2f || detectedMidi < 1.0f)
        return 1.0f;

    const auto key = params.getInt (ParamId::pcKey);
    const auto scale = params.getInt (ParamId::pcScale);
    const auto amount = juce::jlimit (0.0f, 1.0f, params.get (ParamId::pcAmount) * 0.01f);
    const auto targetNote = music::nearestScaleNote ((int) std::lround (detectedMidi), key, scale);
    const auto detectedFrequency = 440.0f * std::pow (2.0f, (detectedMidi - 69.0f) / 12.0f);
    const auto targetFrequency = 440.0f * std::pow (2.0f, ((float) targetNote - 69.0f) / 12.0f);

    if (detectedFrequency < 1.0f)
        return 1.0f;

    const auto fullRatio = juce::jlimit (0.5f, 2.0f, targetFrequency / detectedFrequency);
    return std::pow (fullRatio, amount);
}

void VocalChain::processPitchCorrection (float* left, float* right, int numSamples) noexcept
{
    if (! params.getBool (ParamId::pitchCorrectionEnabled))
    {
        if (correctionWasEnabled)
        {
            corrector.reset();
            correctionRatio = 1.0f;
            correctionWasEnabled = false;
            appliedCorrectionSemitones.store (0.0f, std::memory_order_relaxed);
        }

        return;
    }

    if (! correctionWasEnabled)
    {
        corrector.reset();
        correctionRatio = 1.0f;
        correctionWasEnabled = true;
    }

    const auto target = targetCorrectionRatio (pitchDetector.getMidiNote(), pitchDetector.getConfidence());
    const auto retuneMs = params.get (ParamId::pcRetuneMs);
    const auto coefficient = retuneMs <= 0.0f ? 1.0f
                                               : onePoleBlockCoefficient (retuneMs, sampleRate, numSamples);
    correctionRatio += (target - correctionRatio) * coefficient;
    correctionRatio = juce::jlimit (0.5f, 2.0f, correctionRatio);

    corrector.setRatio (correctionRatio);
    corrector.processStereo (left, right, correctionL.data(), correctionR.data(), numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = correctionL[(size_t) i];
        right[i] = correctionR[(size_t) i];
    }

    appliedCorrectionSemitones.store (12.0f * std::log2 (juce::jmax (1.0e-6f, correctionRatio)),
                                      std::memory_order_relaxed);
}

int VocalChain::getLatencySamples() const noexcept
{
    return params.getBool (ParamId::pitchCorrectionEnabled) ? corrector.getLatencySamples() : 0;
}

void VocalChain::process (const float* inputL, const float* inputR,
                          float* outputL, float* outputR, int numSamples) noexcept
{
    if (numSamples <= 0 || outputL == nullptr || outputR == nullptr)
        return;

    if (inputL == nullptr || inputR == nullptr)
    {
        juce::FloatVectorOperations::clear (outputL, numSamples);
        juce::FloatVectorOperations::clear (outputR, numSamples);
        inputPeakDb.store (-100.0f, std::memory_order_relaxed);
        outputPeakDb.store (-100.0f, std::memory_order_relaxed);
        return;
    }

    if (outputL != inputL)
        juce::FloatVectorOperations::copy (outputL, inputL, numSamples);

    if (outputR != inputR)
        juce::FloatVectorOperations::copy (outputR, inputR, numSamples);

    inputGainSmoother.setTargetValue (dbToGain (params.get (ParamId::micGainDb)));

    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = inputGainSmoother.getNextValue();
        auto l = outputL[i] * gain;
        auto r = outputR[i] * gain;

        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;

        peak = juce::jmax (peak, std::abs (l), std::abs (r));
        outputL[i] = l;
        outputR[i] = r;
    }

    inputPeakDb.store (juce::Decibels::gainToDecibels (peak, -100.0f), std::memory_order_relaxed);

    processGate (outputL, outputR, numSamples);

    // Analyse the vocal before EQ so the Smart EQ sees the source timbre.
    smartEq.process (outputL, outputR, numSamples);

    updateFilters();

    for (int i = 0; i < numSamples; ++i)
    {
        auto l = hpfFilters[0].processSample (outputL[i]);
        auto r = hpfFilters[1].processSample (outputR[i]);

        for (int band = 0; band < 5; ++band)
        {
            l = eqFilters[(size_t) band][0].processSample (l);
            r = eqFilters[(size_t) band][1].processSample (r);
        }

        outputL[i] = l;
        outputR[i] = r;
    }

    processDeEsser (outputL, outputR, numSamples);

    compressor.setParameters (params.get (ParamId::compThresholdDb),
                              params.get (ParamId::compRatio),
                              params.get (ParamId::compAttackMs),
                              params.get (ParamId::compReleaseMs),
                              params.get (ParamId::compMakeupDb));
    compressor.setEnabled (params.getBool (ParamId::compEnabled));
    compressor.process (outputL, outputR, numSamples);
    compGrDb.store (compressor.getGainReductionDb(), std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
        pitchMono[(size_t) i] = 0.5f * (outputL[i] + outputR[i]);

    pitchDetector.process (pitchMono.data(), numSamples);

    processPitchCorrection (outputL, outputR, numSamples);

    harmonizer.process (outputL, outputR, outputL, outputR, numSamples,
                        pitchDetector.getMidiNote(), pitchDetector.getConfidence(), pitchDetector.isVoiced());

    processSaturation (outputL, outputR, numSamples);

    if (params.getBool (ParamId::reverbEnabled))
    {
        updateReverbParameters();
        reverb.processStereo (outputL, outputR, numSamples);
        reverbWasEnabled = true;
    }
    else if (reverbWasEnabled)
    {
        reverb.reset();
        reverbWasEnabled = false;
    }

    processDelay (outputL, outputR, numSamples);

    faderSmoother.setTargetValue (dbToGain (params.get (ParamId::vocalFaderDb)));

    float outputPeak = 0.0f;
    bool didClip = false;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = faderSmoother.getNextValue();
        auto l = outputL[i] * gain;
        auto r = outputR[i] * gain;

        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;

        if (std::abs (l) > 1.0f || std::abs (r) > 1.0f)
            didClip = true;

        outputL[i] = juce::jlimit (-4.0f, 4.0f, l);
        outputR[i] = juce::jlimit (-4.0f, 4.0f, r);
        outputPeak = juce::jmax (outputPeak, std::abs (outputL[i]), std::abs (outputR[i]));
    }

    outputPeakDb.store (juce::Decibels::gainToDecibels (outputPeak, -100.0f), std::memory_order_relaxed);

    if (didClip)
        clipped.store (true, std::memory_order_relaxed);
}

} // namespace rr
