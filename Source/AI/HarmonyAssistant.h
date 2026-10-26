#pragma once

#include "Audio/ParamStore.h"
#include <array>

namespace rr
{
/** Heuristic key detector built from observed lead-vocal pitch.

    This is deliberately not a neural network.  It correlates a pitch-class
    histogram with major, natural-minor and harmonic-minor profiles.  The UI
    labels it as a rule-based local assistant.
*/
class MusicKeyDetector
{
public:
    struct Result
    {
        int key = 0;
        int scale = 0;
        float confidence = 0.0f;
        float correlation = 0.0f;
        bool valid = false;
    };

    void reset() noexcept;
    void addPitchObservation (float midiNote, float confidence);

    int getObservationCount() const noexcept { return observationCount; }
    float getHistogramBin (int pitchClass) const noexcept;
    Result detect() const;

private:
    std::array<float, 12> histogram {};
    int observationCount = 0;
};

struct HarmonySuggestion
{
    bool valid = false;
    int key = 0;
    int scale = 0;
    int voice1Interval = 0;
    int voice2Interval = 1;
    bool voice2Enabled = true;
    bool enableCorrection = true;
    float harmonyLevel = 45.0f;
    float dryWet = 30.0f;
    float humanize = 25.0f;
    float retuneMs = 60.0f;
    float stereoWidth = 60.0f;
    float correctionAmount = 75.0f;
    float confidence = 0.0f;
    juce::String summary;
    juce::StringArray notes;
};

class HarmonyAssistant
{
public:
    static juce::String methodology();

    void beginAnalysis();
    void addPitchObservation (float midiNote, float confidence);
    int getObservationCount() const noexcept { return detector.getObservationCount(); }

    HarmonySuggestion buildSuggestion() const;
    void applySuggestion (ParamStore& params, const HarmonySuggestion& suggestion) const;

private:
    MusicKeyDetector detector;
};

} // namespace rr
