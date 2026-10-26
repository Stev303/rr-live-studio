#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

namespace rr
{
class VocalChainPage : public juce::Component,
                       private juce::Timer
{
public:
    explicit VocalChainPage (AppState& state);
    ~VocalChainPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutContent();
    void loadVocalPreset (const juce::String& name);
    void analyseEq();
    void applyEq();
    void undoEq();
    void resetEq();
    void startHarmonyAnalysis();
    void finishHarmonyAnalysis();
    void cancelHarmonyAnalysis();
    void applyHarmonySuggestion();
    void updateHarmonyUi();

    AppState& app;
    juce::Viewport viewport;
    juce::Component content;

    Panel presetPanel { "Preset Vokal", theme::cyan };
    Panel inputPanel { "Input, Gate, HPF", theme::cyan };
    Panel correctionPanel { "Pitch Correction", theme::green };
    Panel dynamicsPanel { "De-Esser & Compressor", theme::amber };
    Panel eqPanel { "Smart EQ (analisis spektrum lokal)", theme::cyan };
    Panel harmonyPanel { "Auto Harmonizer (rule-based lokal)", theme::magenta };
    Panel characterPanel { "Saturasi, Reverb, Delay, Fader", theme::amber };

    std::array<std::unique_ptr<juce::TextButton>, 6> presetButtons;
    juce::Label presetStatusLabel;

    ParamSlider micGainSlider;
    ParamSlider gateThresholdSlider;
    ParamSlider hpfSlider;
    ParamToggle gateToggle;
    ParamToggle hpfToggle;
    ParamToggle micMuteToggle;
    LevelMeter inputMeter { LevelMeter::Orientation::horizontal };
    LevelMeter outputMeter { LevelMeter::Orientation::horizontal };
    juce::Label inputMeterLabel;
    juce::Label gateStatusLabel;
    juce::Label chainOrderLabel;

    ParamToggle deEsserToggle;
    ParamToggle compToggle;
    ParamSlider deEsserThresholdSlider;
    ParamSlider deEsserFreqSlider;
    ParamSlider compThresholdSlider;
    ParamSlider compRatioSlider;
    ParamSlider compAttackSlider;
    ParamSlider compReleaseSlider;
    ParamSlider compMakeupSlider;
    juce::Label dynamicsMeterLabel;

    ParamToggle correctionToggle;
    ParamCombo pcKeyCombo;
    ParamCombo pcScaleCombo;
    ParamSlider pcRetuneSlider;
    ParamSlider pcAmountSlider;
    juce::Label detectionLabel;
    juce::Label correctionStatusLabel;

    SmartEqDisplay eqDisplay;
    juce::TextButton eqAnalyseButton { "Analisa Spektrum" };
    juce::TextButton eqApplyButton { "Terapkan" };
    juce::TextButton eqUndoButton { "Undo" };
    juce::TextButton eqResetButton { "Reset EQ" };
    juce::Label eqStatusLabel;
    std::array<std::unique_ptr<ParamSlider>, 5> eqFreqSliders;
    std::array<std::unique_ptr<ParamSlider>, 5> eqGainSliders;
    std::array<std::unique_ptr<ParamSlider>, 5> eqQSliders;
    SmartEqAnalysis lastEqAnalysis;
    bool hasEqAnalysis = false;

    ParamToggle harmonyToggle;
    ParamToggle harmonyBypassToggle;
    ParamCombo hKeyCombo;
    ParamCombo hScaleCombo;
    ParamToggle voice1Toggle;
    ParamToggle voice2Toggle;
    ParamCombo voice1IntervalCombo;
    ParamCombo voice2IntervalCombo;
    ParamSlider voice1CustomSlider;
    ParamSlider voice2CustomSlider;
    ParamSlider harmonyLevelSlider;
    ParamSlider harmonyDryWetSlider;
    ParamSlider humanizeSlider;
    ParamSlider harmonyRetuneSlider;
    ParamSlider trackingSlider;
    ParamSlider stereoWidthSlider;
    ParamToggle formantToggle;
    juce::TextButton harmonyAnalyseButton { "Analisa Nada" };
    juce::TextButton harmonyApplyButton { "Terapkan Saran" };
    juce::TextButton harmonyCancelButton { "Batal" };
    juce::Label harmonySuggestionLabel;
    juce::Label harmonyStatusLabel;
    HarmonySuggestion currentSuggestion;
    bool collectingHarmony = false;
    int harmonyCollectionTicks = 0;

    ParamToggle satToggle;
    ParamSlider satDriveSlider;
    ParamSlider satMixSlider;
    ParamToggle reverbToggle;
    ParamSlider reverbMixSlider;
    ParamSlider reverbSizeSlider;
    ParamSlider reverbDampSlider;
    ParamToggle delayToggle;
    ParamSlider delayTimeSlider;
    ParamSlider delayFeedbackSlider;
    ParamSlider delayMixSlider;
    ParamSlider vocalFaderSlider;
    juce::Label masterVocalMeterLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalChainPage)
};

} // namespace rr
