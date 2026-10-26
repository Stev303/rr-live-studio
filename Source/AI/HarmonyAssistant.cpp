#include "AI/HarmonyAssistant.h"
#include "Utilities/MusicTheory.h"

#include <cmath>

namespace rr
{
namespace
{
    const std::array<float, 12> majorProfile {
        6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
    };

    const std::array<float, 12> minorProfile {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
    };

    const std::array<float, 12> harmonicMinorProfile {
        6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 5.20f
    };

    const std::array<float, 12>* profileForScale (int scale)
    {
        switch (scale)
        {
            case 1:  return &minorProfile;
            case 2:  return &harmonicMinorProfile;
            default: return &majorProfile;
        }
    }

    float correlation (const std::array<float, 12>& histogram, const std::array<float, 12>& profile)
    {
        float meanHistogram = 0.0f;
        float meanProfile = 0.0f;

        for (int i = 0; i < 12; ++i)
        {
            meanHistogram += histogram[(size_t) i];
            meanProfile += profile[(size_t) i];
        }

        meanHistogram /= 12.0f;
        meanProfile /= 12.0f;

        float numerator = 0.0f;
        float histogramVariance = 0.0f;
        float profileVariance = 0.0f;

        for (int i = 0; i < 12; ++i)
        {
            const auto h = histogram[(size_t) i] - meanHistogram;
            const auto p = profile[(size_t) i] - meanProfile;
            numerator += h * p;
            histogramVariance += h * h;
            profileVariance += p * p;
        }

        const auto denominator = std::sqrt (histogramVariance * profileVariance);
        return denominator > 1.0e-9f ? numerator / denominator : 0.0f;
    }
}

void MusicKeyDetector::reset() noexcept
{
    histogram.fill (0.0f);
    observationCount = 0;
}

void MusicKeyDetector::addPitchObservation (float midiNote, float confidence)
{
    if (midiNote < 20.0f || confidence < 0.25f)
        return;

    auto pitchClass = (int) std::lround (midiNote) % 12;

    if (pitchClass < 0)
        pitchClass += 12;

    histogram[(size_t) pitchClass] += juce::jmax (0.1f, confidence);
    ++observationCount;
}

float MusicKeyDetector::getHistogramBin (int pitchClass) const noexcept
{
    if (pitchClass < 0 || pitchClass >= 12)
        return 0.0f;

    return histogram[(size_t) pitchClass];
}

MusicKeyDetector::Result MusicKeyDetector::detect() const
{
    Result result;

    if (observationCount < 12)
        return result;

    float sum = 0.0f;

    for (const auto value : histogram)
        sum += value;

    if (sum <= 1.0e-6f)
        return result;

    std::array<float, 12> normalised {};

    for (int i = 0; i < 12; ++i)
        normalised[(size_t) i] = histogram[(size_t) i] / sum;

    float best = -2.0f;
    float second = -2.0f;

    for (int scale = 0; scale < 3; ++scale)
    {
        const auto& profile = *profileForScale (scale);

        for (int key = 0; key < 12; ++key)
        {
            std::array<float, 12> rotated {};

            for (int i = 0; i < 12; ++i)
                rotated[(size_t) i] = profile[(size_t) ((i - key + 12) % 12)];

            const auto score = correlation (normalised, rotated);

            if (score > best)
            {
                second = best;
                best = score;
                result.key = key;
                result.scale = scale;
                result.correlation = score;
            }
            else if (score > second)
            {
                second = score;
            }
        }
    }

    const auto coverage = juce::jlimit (0.0f, 1.0f, (float) observationCount / 80.0f);
    const auto gap = juce::jlimit (0.0f, 1.0f, (best - juce::jmax (0.0f, second)) * 2.5f);
    result.confidence = juce::jlimit (0.0f, 1.0f, coverage * (0.45f + 0.55f * gap));
    result.valid = observationCount >= 24 && best > 0.25f;

    return result;
}

//==============================================================================
juce::String HarmonyAssistant::methodology()
{
    return "Asisten lokal berbasis aturan: histogram pitch vokal dicocokkan dengan profil "
           "tangga nada Major, Natural Minor, dan Harmonic Minor. Ini bukan model AI terlatih "
           "dan tidak mengirim data ke cloud.";
}

void HarmonyAssistant::beginAnalysis()
{
    detector.reset();
}

void HarmonyAssistant::addPitchObservation (float midiNote, float confidence)
{
    detector.addPitchObservation (midiNote, confidence);
}

HarmonySuggestion HarmonyAssistant::buildSuggestion() const
{
    HarmonySuggestion suggestion;
    const auto detected = detector.detect();

    suggestion.key = detected.key;
    suggestion.scale = detected.scale;
    suggestion.confidence = detected.confidence;

    if (! detected.valid)
    {
        suggestion.summary = "Belum cukup data pitch yang stabil. Nyanyikan melodi utama selama "
                             "beberapa detik dengan nada yang jelas, lalu analisa lagi.";
        suggestion.notes.add ("Observasi saat ini: " + juce::String (detector.getObservationCount())
                              + " (minimal 24 nada terdeteksi).");
        suggestion.notes.add ("Fallback lokal: tidak ada perubahan parameter yang disarankan.");
        return suggestion;
    }

    suggestion.valid = true;
    suggestion.voice1Interval = 0; // third within the detected scale
    suggestion.voice2Interval = 1; // fifth
    suggestion.voice2Enabled = detected.confidence > 0.45f;
    suggestion.enableCorrection = detected.confidence > 0.3f;
    suggestion.harmonyLevel = 40.0f + 15.0f * detected.confidence;
    suggestion.dryWet = 25.0f + 15.0f * detected.confidence;
    suggestion.humanize = 20.0f + 20.0f * detected.confidence;
    suggestion.stereoWidth = 50.0f + 25.0f * detected.confidence;
    suggestion.correctionAmount = 60.0f + 25.0f * detected.confidence;

    suggestion.summary = "Saran: " + music::keyAndScaleName (detected.key, detected.scale)
                       + " dengan harmoni " + juce::String (music::intervalNames[0])
                       + (suggestion.voice2Enabled ? " + " + juce::String (music::intervalNames[1]) : juce::String())
                       + ". Keyakinan heuristik " + juce::String (juce::roundToInt (detected.confidence * 100.0f)) + "%.";

    suggestion.notes.add ("Kunci dideteksi dari " + juce::String (detector.getObservationCount())
                          + " observasi pitch vokal (analisa lokal, tanpa cloud).");
    suggestion.notes.add ("Nada harmoni mengikuti tangga nada, bukan delay atau chorus.");
    suggestion.notes.add ("Koreksi pitch disarankan hanya jika keyakinan cukup; dengarkan hasil sebelum Go Live.");

    if (! suggestion.voice2Enabled)
        suggestion.notes.add ("Suara kedua dimatikan karena keyakinan key masih sedang.");

    return suggestion;
}

void HarmonyAssistant::applySuggestion (ParamStore& params, const HarmonySuggestion& suggestion) const
{
    if (! suggestion.valid)
        return;

    const auto key = juce::jlimit (0, 11, suggestion.key);
    const auto scale = juce::jlimit (0, 3, suggestion.scale);

    params.setSilently (ParamId::pcKey, (float) key);
    params.setSilently (ParamId::pcScale, (float) scale);
    params.set (ParamId::pitchCorrectionEnabled, suggestion.enableCorrection ? 1.0f : 0.0f);
    params.set (ParamId::pcAmount, suggestion.correctionAmount);
    params.set (ParamId::pcRetuneMs, 45.0f);

    params.set (ParamId::hKey, (float) key);
    params.set (ParamId::hScale, (float) scale);
    params.set (ParamId::harmonyEnabled, 1.0f);
    params.set (ParamId::harmonyBypass, 0.0f);
    params.set (ParamId::hVoice1On, 1.0f);
    params.set (ParamId::hVoice2On, suggestion.voice2Enabled ? 1.0f : 0.0f);
    params.set (ParamId::hVoice1Interval, (float) suggestion.voice1Interval);
    params.set (ParamId::hVoice2Interval, (float) suggestion.voice2Interval);
    params.set (ParamId::harmonyLevel, suggestion.harmonyLevel);
    params.set (ParamId::harmonyDryWet, suggestion.dryWet);
    params.set (ParamId::humanize, suggestion.humanize);
    params.set (ParamId::formantPreserve, 1.0f);
    params.set (ParamId::retuneSpeedMs, suggestion.retuneMs);
    params.set (ParamId::trackingStrength, 70.0f);
    params.set (ParamId::stereoWidth, suggestion.stereoWidth);
}

} // namespace rr
