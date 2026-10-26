#include "DSP/SmartEQ.h"
#include <cmath>

namespace rr
{
namespace
{
    constexpr float kMinFrequency = 20.0f;
    constexpr float kMaxFrequency = 20000.0f;
    constexpr float kSilenceFloorDb = -92.0f;

    float clampGain (float value) noexcept { return juce::jlimit (-6.0f, 3.0f, value); }
}

SmartEQ::SmartEQ (ParamStore& parameters)
    : params (parameters)
{
    for (auto& value : spectrumDb)
        value.store (-100.0f, std::memory_order_relaxed);
}

void SmartEQ::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (6.283185307179586f * (float) i / (float) fftSize);

    ring.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);

    reset();
}

void SmartEQ::reset() noexcept
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    ringWriteIndex = 0;
    samplesSinceAnalysis = 0;
    analysisFrames.store (0, std::memory_order_relaxed);

    for (auto& value : spectrumDb)
        value.store (-100.0f, std::memory_order_relaxed);
}

void SmartEQ::process (const float* left, const float* right, int numSamples) noexcept
{
    if (! fft || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto l = left != nullptr ? left[i] : 0.0f;
        const auto r = right != nullptr ? right[i] : 0.0f;
        auto mono = 0.5f * (l + r);

        if (! std::isfinite (mono))
            mono = 0.0f;

        ring[(size_t) ringWriteIndex] = mono;
        ringWriteIndex = (ringWriteIndex + 1) % fftSize;

        if (++samplesSinceAnalysis >= analysisInterval)
        {
            samplesSinceAnalysis = 0;
            analyseFrame();
        }
    }
}

void SmartEQ::analyseFrame() noexcept
{
    if (! fft)
        return;

    std::fill (fftData.begin(), fftData.end(), 0.0f);

    for (int i = 0; i < fftSize; ++i)
    {
        const auto index = (ringWriteIndex + i) % fftSize;
        fftData[(size_t) i] = ring[(size_t) index] * window[(size_t) i];
    }

    fft->performRealOnlyForwardTransform (fftData.data(), false);

    const auto half = fftSize / 2 + 1;
    const auto binWidth = (float) sampleRate / (float) fftSize;
    const auto frameNumber = analysisFrames.load (std::memory_order_relaxed);

    for (int band = 0; band < numSpectrumBands; ++band)
    {
        const auto lowFrequency = kMinFrequency * std::pow (kMaxFrequency / kMinFrequency,
                                                            (float) band / (float) numSpectrumBands);
        const auto highFrequency = kMinFrequency * std::pow (kMaxFrequency / kMinFrequency,
                                                             (float) (band + 1) / (float) numSpectrumBands);

        auto firstBin = juce::jmax (1, (int) std::floor (lowFrequency / binWidth));
        auto lastBin = juce::jmin (half - 1, (int) std::ceil (highFrequency / binWidth));
        lastBin = juce::jmax (firstBin, lastBin);

        double power = 0.0;
        int count = 0;

        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            const auto real = fftData[(size_t) (bin * 2)];
            const auto imag = fftData[(size_t) (bin * 2 + 1)];
            power += (double) real * (double) real + (double) imag * (double) imag;
            ++count;
        }

        const auto meanPower = count > 0 ? power / (double) count : 0.0;
        const auto db = (float) (10.0 * std::log10 (meanPower + 1.0e-12));
        const auto previous = spectrumDb[(size_t) band].load (std::memory_order_relaxed);
        const auto smoothed = frameNumber == 0 ? db : previous * 0.72f + db * 0.28f;
        spectrumDb[(size_t) band].store (juce::jlimit (-120.0f, 20.0f, smoothed), std::memory_order_relaxed);
    }

    analysisFrames.fetch_add (1, std::memory_order_relaxed);
}

int SmartEQ::bandIndexForFrequency (float frequencyHz) const noexcept
{
    const auto normalised = std::log (juce::jlimit (kMinFrequency, kMaxFrequency, frequencyHz) / kMinFrequency)
                          / std::log (kMaxFrequency / kMinFrequency);
    return juce::jlimit (0, numSpectrumBands - 1, (int) std::floor (normalised * (float) numSpectrumBands));
}

float SmartEQ::averageLevelBetween (float lowHz, float highHz) const noexcept
{
    const auto first = bandIndexForFrequency (lowHz);
    const auto last = bandIndexForFrequency (highHz);
    float sum = 0.0f;
    int count = 0;

    for (int band = first; band <= last; ++band)
    {
        sum += spectrumDb[(size_t) band].load (std::memory_order_relaxed);
        ++count;
    }

    return count > 0 ? sum / (float) count : -100.0f;
}

void SmartEQ::getSpectrum (std::array<float, numSpectrumBands>& destination) const noexcept
{
    for (int i = 0; i < numSpectrumBands; ++i)
        destination[(size_t) i] = spectrumDb[(size_t) i].load (std::memory_order_relaxed);
}

SmartEqAnalysis SmartEQ::analyse() const
{
    SmartEqAnalysis result;

    result.bands[0] = { 90.0f, 0.0f, 0.9f, {} };
    result.bands[1] = { 300.0f, 0.0f, 1.1f, {} };
    result.bands[2] = { 1000.0f, 0.0f, 1.4f, {} };
    result.bands[3] = { 3500.0f, 0.0f, 1.2f, {} };
    result.bands[4] = { 7500.0f, 0.0f, 1.6f, {} };

    if (! hasEnoughData())
    {
        result.notes.add ("Belum cukup data. Nyalakan mikrofon dan biarkan audio berjalan minimal 1 detik.");
        return result;
    }

    const auto low = averageLevelBetween (40.0f, 150.0f);
    const auto lowMid = averageLevelBetween (150.0f, 400.0f);
    const auto mid = averageLevelBetween (400.0f, 1500.0f);
    const auto presence = averageLevelBetween (1500.0f, 4000.0f);
    const auto high = averageLevelBetween (4000.0f, 8000.0f);
    const auto nasalRegion = averageLevelBetween (700.0f, 1400.0f);
    const auto nasalReference = averageLevelBetween (1800.0f, 3500.0f);
    const auto sibilance = averageLevelBetween (5500.0f, 8500.0f);
    const auto sibilanceReference = averageLevelBetween (1500.0f, 3500.0f);

    const auto loudestBand = juce::jmax (juce::jmax (low, lowMid, mid, presence),
                                         juce::jmax (high, nasalRegion, sibilance));

    if (loudestBand < kSilenceFloorDb)
    {
        result.notes.add ("Sinyal vokal terlalu pelan atau hening. Naikkan input gain lalu analisis ulang.");
        return result;
    }

    const auto boomyExcess = low - lowMid;
    const auto muddyExcess = lowMid - mid;
    const auto nasalExcess = nasalRegion - nasalReference;
    const auto harshExcess = presence - high;
    const auto sibilanceExcess = sibilance - sibilanceReference;

    auto cutGain = [] (float excess)
    {
        return excess > 2.0f ? clampGain (-juce::jlimit (0.5f, 6.0f, (excess - 2.0f) * 0.65f)) : 0.0f;
    };

    result.bands[0].gainDb = cutGain (boomyExcess);
    result.bands[1].gainDb = cutGain (muddyExcess);
    result.bands[2].gainDb = cutGain (nasalExcess);
    result.bands[3].gainDb = cutGain (harshExcess);
    result.bands[4].gainDb = cutGain (sibilanceExcess);

    if (presence < mid - 5.0f)
    {
        result.bands[3].gainDb = clampGain (result.bands[3].gainDb + 1.5f);
        result.notes.add ("Vokal terasa kurang terbuka; presence 3.5 kHz dinaikkan sedikit.");
    }

    if (boomyExcess > 2.0f)
        result.notes.add ("Terindikasi boomy di sekitar 90 Hz: kurangi low-end.");
    if (muddyExcess > 2.0f)
        result.notes.add ("Terindikasi muddy di sekitar 300 Hz: bersihkan low-mid.");
    if (nasalExcess > 2.0f)
        result.notes.add ("Terindikasi nasal di sekitar 1 kHz: kurangi sedikit.");
    if (harshExcess > 2.0f)
        result.notes.add ("Terindikasi harsh di sekitar 3.5 kHz: kurangi presence.");
    if (sibilanceExcess > 2.0f)
        result.notes.add ("Sibilance tinggi terdeteksi: kurangi sekitar 7.5 kHz.");

    if (result.notes.isEmpty())
        result.notes.add ("Spektrum relatif seimbang. Saran pemangkasan halus dapat diterapkan atau diabaikan.");

    result.notes.add ("Analisis DSP rule-based pada sinyal vokal; bukan model AI terlatih. Semua band dapat diedit manual.");

    const auto frames = (float) getAnalysisFrameCount();
    const auto activity = juce::jlimit (0.0f, 1.0f, (loudestBand + 75.0f) / 35.0f);
    result.confidence = juce::jlimit (0.0f, 1.0f, (frames / 60.0f) * (0.5f + 0.5f * activity));
    result.hasData = true;
    return result;
}

void SmartEQ::apply (const SmartEqAnalysis& analysis)
{
    for (int i = 0; i < 15; ++i)
        undoValues[(size_t) i] = params.get (eqParamIds[(size_t) i]);

    undoValid = true;

    for (int band = 0; band < SmartEqAnalysis::numBands; ++band)
    {
        const auto& suggestion = analysis.bands[(size_t) band];
        const auto base = (size_t) band * 3;
        const auto& freqInfo = ParamStore::info (eqParamIds[base]);
        const auto& gainInfo = ParamStore::info (eqParamIds[base + 1]);
        const auto& qInfo = ParamStore::info (eqParamIds[base + 2]);

        params.set (eqParamIds[base],
                    juce::jlimit (freqInfo.minValue, freqInfo.maxValue, suggestion.frequencyHz));
        params.set (eqParamIds[base + 1],
                    juce::jlimit (gainInfo.minValue, gainInfo.maxValue, suggestion.gainDb));
        params.set (eqParamIds[base + 2],
                    juce::jlimit (qInfo.minValue, qInfo.maxValue, suggestion.q));
    }
}

void SmartEQ::undo()
{
    if (! undoValid)
        return;

    for (int i = 0; i < 15; ++i)
        params.set (eqParamIds[(size_t) i], undoValues[(size_t) i]);

    undoValid = false;
}

void SmartEQ::resetEq()
{
    for (auto id : eqParamIds)
        params.set (id, ParamStore::info (id).defaultValue);

    undoValid = false;
}

juce::String SmartEQ::methodology()
{
    return "Smart EQ memakai analisis spektrum FFT lokal dan aturan ambang batas untuk mendeteksi "
           "boomy, muddy, nasal, harsh, dan sibilance. Tidak ada model AI atau pengiriman audio ke cloud.";
}

} // namespace rr
