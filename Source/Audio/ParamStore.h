#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <array>
#include <atomic>

namespace rr
{
enum class ParamGroup
{
    Master,
    Music,
    Booster,
    Balance,
    Pitch,
    Vocal,
    Correction,
    Harmony,
    Sampler,
    Test
};

#define RR_PARAM_LIST(X) \
    X(masterGainDb,          "master.gain",            "Master Gain",        "dB",  -24.0f,  6.0f,   0.0f,  1.0f, ParamGroup::Master,     false, 1) \
    X(masterLimiterEnabled,  "master.limiter",         "Master Limiter",     "",      0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Master,     true,  0) \
    X(masterLimiterCeilingDb,"master.limiterCeiling",  "Limiter Ceiling",    "dB",   -6.0f,  0.0f,  -1.0f,  1.0f, ParamGroup::Master,     false, 1) \
    X(testToneEnabled,       "test.tone",              "Test Tone 1 kHz",    "",      0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Test,       true,  0) \
    X(testToneDb,            "test.toneGain",          "Test Tone Level",    "dB",  -40.0f,  0.0f, -18.0f,  1.0f, ParamGroup::Test,       false, 1) \
    \
    X(musicGainDb,           "music.gain",             "Music Gain",         "dB",  -24.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(musicInputTrimDb,      "music.inputTrim",        "Input Trim",         "dB",  -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(musicBassDb,           "music.bass",             "Bass Enhance",       "dB",  -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(musicPresenceDb,       "music.presence",         "Presence",           "dB",  -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(musicTrebleDb,         "music.treble",           "Treble Enhance",     "dB",  -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(musicLoudness,         "music.loudness",         "Perceived Loudness", "%",      0.0f, 100.0f,  0.0f,  1.0f, ParamGroup::Booster,    false, 0) \
    X(boosterCompThresholdDb,"music.compThreshold",    "Comp Threshold",     "dB",  -48.0f,  0.0f, -18.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(boosterCompRatio,      "music.compRatio",        "Comp Ratio",         ":1",     1.0f,  8.0f,   2.5f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(boosterCompAttackMs,   "music.compAttack",       "Comp Attack",        "ms",     1.0f, 100.0f,  10.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(boosterCompReleaseMs,  "music.compRelease",      "Comp Release",       "ms",    20.0f, 500.0f, 120.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(boosterCompMakeupDb,   "music.compMakeup",       "Comp Makeup",        "dB",     0.0f, 18.0f,   3.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(boosterLimiterCeilingDb,"music.limCeiling",      "Music Limiter",      "dB",    -6.0f,  0.0f,  -1.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(boosterOutputGainDb,   "music.outputGain",       "Output Gain",        "dB",  -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Booster,    false, 1) \
    X(boosterAutoLevel,      "music.autoLevel",        "Auto Level",         "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Booster,    true,  0) \
    X(boosterBypass,         "music.boosterBypass",    "Booster Bypass",     "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Booster,    true,  0) \
    X(musicMixDb,            "mix.music",              "Music Fader",        "dB",  -36.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Music,      false, 1) \
    X(musicMute,             "mix.musicMute",          "Music Mute",         "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Music,      true,  0) \
    X(musicSolo,             "mix.musicSolo",          "Music Solo",         "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Music,      true,  0) \
    X(youtubeVideoVolume,    "youtube.videoVolume",    "Video Volume",       "%",      0.0f, 100.0f,  85.0f,  1.0f, ParamGroup::Music,      false, 0) \
    \
    X(transpose,             "pitch.transpose",        "Transpose",          "st",   -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Pitch,      false, 0) \
    X(musicKey,              "pitch.musicKey",         "Kunci Referensi",    "",       0.0f, 11.0f,   0.0f,  1.0f, ParamGroup::Pitch,      false, 0) \
    X(pitchBypass,           "pitch.bypass",           "Pitch Bypass",       "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Pitch,      true,  0) \
    \
    X(sbEnabled,             "balance.enabled",        "Smart Balance",      "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Balance,    true,  0) \
    X(sbTargetDb,            "balance.target",         "Vocal Over Music",   "dB",     0.0f, 15.0f,   4.0f,  1.0f, ParamGroup::Balance,    false, 1) \
    X(sbDuckDb,              "balance.duck",           "Duck Amount",        "dB",     0.0f, 18.0f,   6.0f,  1.0f, ParamGroup::Balance,    false, 1) \
    X(sbAttackMs,            "balance.attack",         "Duck Attack",        "ms",     5.0f, 200.0f, 25.0f,  1.0f, ParamGroup::Balance,    false, 0) \
    X(sbReleaseMs,           "balance.release",        "Duck Release",       "ms",    50.0f, 2000.0f,400.0f, 1.0f, ParamGroup::Balance,    false, 0) \
    X(sbStrength,            "balance.strength",       "Strength",           "%",      0.0f, 100.0f, 70.0f,  1.0f, ParamGroup::Balance,    false, 0) \
    X(sbMaxReductionDb,      "balance.maxReduction",   "Max Music Duck",     "dB",     0.0f, 24.0f,  10.0f,  1.0f, ParamGroup::Balance,    false, 1) \
    \
    X(micGainDb,             "vocal.inputGain",        "Input Gain",         "dB",  -12.0f, 24.0f,   6.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(micMixDb,              "mix.mic",                "Mic Fader",          "dB",  -36.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(micMute,               "mix.micMute",            "Mic Mute",           "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(micSolo,               "mix.micSolo",            "Mic Solo",           "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(gateEnabled,           "vocal.gate",             "Noise Gate",         "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(gateThresholdDb,       "vocal.gateThreshold",    "Gate Threshold",     "dB",   -70.0f,-20.0f, -50.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(hpfEnabled,            "vocal.hpf",              "High-Pass Filter",   "",       0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(hpfHz,                 "vocal.hpfHz",            "HPF Frequency",      "Hz",    20.0f, 400.0f,  90.0f,  0.5f, ParamGroup::Vocal,      false, 0) \
    X(deEsserEnabled,        "vocal.deEsser",          "De-Esser",           "",       0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(deEsserThresholdDb,    "vocal.deEsserThreshold", "De-Esser Threshold", "dB",   -60.0f,-10.0f, -32.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(deEsserFreqHz,         "vocal.deEsserFreq",      "Sibilance Freq",     "Hz",  4000.0f,12000.0f,6500.0f,0.6f, ParamGroup::Vocal,      false, 0) \
    X(compEnabled,           "vocal.comp",             "Compressor",         "",       0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(compThresholdDb,       "vocal.compThreshold",    "Comp Threshold",     "dB",   -48.0f,  0.0f, -22.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(compRatio,             "vocal.compRatio",        "Comp Ratio",         ":1",     1.0f, 10.0f,   3.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(compAttackMs,          "vocal.compAttack",       "Comp Attack",        "ms",     1.0f, 100.0f,  12.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(compReleaseMs,         "vocal.compRelease",      "Comp Release",       "ms",    20.0f, 500.0f, 140.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(compMakeupDb,          "vocal.compMakeup",       "Comp Makeup",        "dB",     0.0f, 18.0f,   3.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(eqEnabled,             "vocal.eq",               "Smart EQ",           "",       0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(eq1Freq,               "vocal.eq1Freq",          "EQ 1 Freq",          "Hz",    40.0f, 150.0f,  80.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(eq1Gain,               "vocal.eq1Gain",          "EQ 1 Gain",          "dB",   -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(eq1Q,                  "vocal.eq1Q",             "EQ 1 Q",             "",       0.3f,  4.0f,   0.9f,  1.0f, ParamGroup::Vocal,      false, 2) \
    X(eq2Freq,               "vocal.eq2Freq",          "EQ 2 Freq",          "Hz",   150.0f, 600.0f, 300.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(eq2Gain,               "vocal.eq2Gain",          "EQ 2 Gain",          "dB",   -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(eq2Q,                  "vocal.eq2Q",             "EQ 2 Q",             "",       0.3f,  4.0f,   1.0f,  1.0f, ParamGroup::Vocal,      false, 2) \
    X(eq3Freq,               "vocal.eq3Freq",          "EQ 3 Freq",          "Hz",   600.0f,2500.0f,1200.0f, 1.0f, ParamGroup::Vocal,      false, 0) \
    X(eq3Gain,               "vocal.eq3Gain",          "EQ 3 Gain",          "dB",   -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(eq3Q,                  "vocal.eq3Q",             "EQ 3 Q",             "",       0.3f,  4.0f,   1.0f,  1.0f, ParamGroup::Vocal,      false, 2) \
    X(eq4Freq,               "vocal.eq4Freq",          "EQ 4 Freq",          "Hz",  2500.0f,6000.0f,3500.0f, 1.0f, ParamGroup::Vocal,      false, 0) \
    X(eq4Gain,               "vocal.eq4Gain",          "EQ 4 Gain",          "dB",   -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(eq4Q,                  "vocal.eq4Q",             "EQ 4 Q",             "",       0.3f,  4.0f,   1.0f,  1.0f, ParamGroup::Vocal,      false, 2) \
    X(eq5Freq,               "vocal.eq5Freq",          "EQ 5 Freq",          "Hz",  6000.0f,12000.0f,8000.0f,1.0f, ParamGroup::Vocal,      false, 0) \
    X(eq5Gain,               "vocal.eq5Gain",          "EQ 5 Gain",          "dB",   -12.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    X(eq5Q,                  "vocal.eq5Q",             "EQ 5 Q",             "",       0.3f,  4.0f,   1.0f,  1.0f, ParamGroup::Vocal,      false, 2) \
    X(satEnabled,            "vocal.saturation",       "Saturation",         "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(satDrive,              "vocal.satDrive",         "Drive",              "%",      0.0f, 100.0f, 20.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(satMix,                "vocal.satMix",           "Sat Mix",            "%",      0.0f, 100.0f, 25.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(reverbEnabled,         "vocal.reverb",           "Reverb",             "",       0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(reverbMix,             "vocal.reverbMix",        "Reverb Mix",         "%",      0.0f, 100.0f, 12.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(reverbSize,            "vocal.reverbSize",       "Room Size",          "%",      0.0f, 100.0f, 45.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(reverbDamp,            "vocal.reverbDamp",       "Damping",            "%",      0.0f, 100.0f, 50.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(delayEnabled,          "vocal.delay",            "Delay",              "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Vocal,      true,  0) \
    X(delayTimeMs,           "vocal.delayTime",        "Delay Time",         "ms",    40.0f, 800.0f, 250.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(delayFeedback,         "vocal.delayFeedback",    "Delay Feedback",     "%",      0.0f, 80.0f,  25.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(delayMix,              "vocal.delayMix",         "Delay Mix",          "%",      0.0f, 100.0f, 10.0f,  1.0f, ParamGroup::Vocal,      false, 0) \
    X(vocalFaderDb,          "vocal.fader",            "Vocal Fader",        "dB",   -36.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Vocal,      false, 1) \
    \
    X(pitchCorrectionEnabled,"correction.enabled",     "Pitch Correction",   "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Correction, true,  0) \
    X(pcKey,                 "correction.key",         "Key",                "",       0.0f, 11.0f,   0.0f,  1.0f, ParamGroup::Correction, false, 0) \
    X(pcScale,               "correction.scale",       "Scale",              "",       0.0f,  3.0f,   0.0f,  1.0f, ParamGroup::Correction, false, 0) \
    X(pcRetuneMs,            "correction.retune",      "Retune Speed",       "ms",     0.0f, 200.0f,  40.0f,  1.0f, ParamGroup::Correction, false, 0) \
    X(pcAmount,              "correction.amount",      "Correction Amount",  "%",      0.0f, 100.0f,100.0f,  1.0f, ParamGroup::Correction, false, 0) \
    \
    X(harmonyEnabled,        "harmony.enabled",        "Auto Harmonizer",    "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Harmony,    true,  0) \
    X(harmonyBypass,         "harmony.bypass",         "Harmony Bypass",     "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Harmony,    true,  0) \
    X(hKey,                  "harmony.key",            "Harmony Key",        "",       0.0f, 11.0f,   0.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(hScale,                "harmony.scale",          "Harmony Scale",      "",       0.0f,  3.0f,   0.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(hVoice1On,             "harmony.voice1",         "Voice 1",            "",       0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Harmony,    true,  0) \
    X(hVoice2On,             "harmony.voice2",         "Voice 2",            "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Harmony,    true,  0) \
    X(hVoice1Interval,       "harmony.voice1Interval", "Voice 1 Interval",   "",       0.0f,  3.0f,   0.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(hVoice2Interval,       "harmony.voice2Interval", "Voice 2 Interval",   "",       0.0f,  3.0f,   1.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(hVoice1Custom,         "harmony.voice1Custom",   "Voice 1 Custom",     "st",   -12.0f, 12.0f,   3.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(hVoice2Custom,         "harmony.voice2Custom",   "Voice 2 Custom",     "st",   -12.0f, 12.0f,   7.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(harmonyLevel,          "harmony.level",          "Harmony Level",      "%",      0.0f, 100.0f, 50.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(harmonyDryWet,         "harmony.dryWet",         "Dry/Wet",            "%",      0.0f, 100.0f, 35.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(humanize,              "harmony.humanize",       "Humanize",           "%",      0.0f, 100.0f, 25.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(formantPreserve,       "harmony.formant",        "Formant Preserve",   "",       0.0f,  1.0f,   1.0f,  1.0f, ParamGroup::Harmony,    true,  0) \
    X(retuneSpeedMs,         "harmony.retune",         "Retune Speed",       "ms",     0.0f, 200.0f,  60.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(trackingStrength,      "harmony.tracking",       "Vocal Tracking",     "%",      0.0f, 100.0f, 60.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    X(stereoWidth,           "harmony.stereoWidth",    "Stereo Width",       "%",      0.0f, 100.0f, 60.0f,  1.0f, ParamGroup::Harmony,    false, 0) \
    \
    X(samplerMixDb,          "mix.sampler",            "Sampler Fader",      "dB",   -36.0f, 12.0f,   0.0f,  1.0f, ParamGroup::Sampler,    false, 1) \
    X(samplerMute,           "mix.samplerMute",        "Sampler Mute",       "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Sampler,    true,  0) \
    X(samplerSolo,           "mix.samplerSolo",        "Sampler Solo",       "",       0.0f,  1.0f,   0.0f,  1.0f, ParamGroup::Sampler,    true,  0)

enum class ParamId : int
{
#define RR_ENUM_ENTRY(name, key, label, unit, minv, maxv, defv, skewv, group, toggle, decimals) name,
    RR_PARAM_LIST(RR_ENUM_ENTRY)
#undef RR_ENUM_ENTRY
    count
};

struct ParamInfo
{
    ParamId id = ParamId::count;
    const char* key = "";
    const char* label = "";
    const char* unit = "";
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    float skew = 1.0f;
    ParamGroup group = ParamGroup::Master;
    bool isToggle = false;
    int decimals = 0;
};

class ParamStore : public juce::ChangeBroadcaster
{
public:
    static constexpr int numParams = static_cast<int> (ParamId::count);

    ParamStore();

    static const ParamInfo& info (ParamId id) noexcept;
    static ParamId idFromKey (const juce::String& key) noexcept;
    static juce::String groupName (ParamGroup group);

    float get (ParamId id) const noexcept          { return values[(size_t) id].load (std::memory_order_relaxed); }
    bool getBool (ParamId id) const noexcept       { return get (id) > 0.5f; }
    int getInt (ParamId id) const noexcept         { return (int) std::lround (get (id)); }
    void set (ParamId id, float newValue);
    void setBool (ParamId id, bool v)              { set (id, v ? 1.0f : 0.0f); }
    void setSilently (ParamId id, float newValue) noexcept;
    std::atomic<float>& atomic (ParamId id) noexcept { return values[(size_t) id]; }
    const std::atomic<float>& atomic (ParamId id) const noexcept { return values[(size_t) id]; }

    void resetAll();
    void resetGroup (ParamGroup group);

    juce::var toVar() const;
    bool fromVar (const juce::var& source, juce::StringArray* warnings = nullptr);

    juce::String getDisplayValue (ParamId id) const;

private:
    std::array<std::atomic<float>, (size_t) numParams> values;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamStore)
};

inline float dbToGain (float db) noexcept { return db <= -100.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float gain) noexcept { return gain <= 1.0e-7f ? -120.0f : 20.0f * std::log10 (gain); }

} // namespace rr
