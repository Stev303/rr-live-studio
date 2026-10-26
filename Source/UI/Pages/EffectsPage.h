#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

namespace rr
{
class EffectsPage : public juce::Component,
                    private juce::Timer
{
public:
    explicit EffectsPage (AppState& state);
    ~EffectsPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutContent();
    void loadMusicPreset (const juce::String& name);

    AppState& app;
    juce::Viewport viewport;
    juce::Component content;

    Panel pitchPanel { "YouTube Pitch / Key", theme::cyan };
    Panel balancePanel { "Smart Balance", theme::green };
    Panel masterPanel { "Master & Output", theme::amber };
    Panel boosterPanel { "Music Booster - kontrol lengkap", theme::magenta };

    ParamSlider transposeSlider;
    ParamCombo musicKeyCombo;
    ParamToggle pitchBypassToggle;
    juce::TextButton minusFiveButton { "-5 st" };
    juce::TextButton plusFiveButton { "+5 st" };
    juce::TextButton resetPitchButton { "Reset" };
    ParamSlider videoVolumeSlider;
    juce::Label keyDisplayLabel;
    juce::Label pitchStatusLabel;

    ParamToggle balanceToggle;
    ParamSlider balanceTargetSlider;
    ParamSlider balanceDuckSlider;
    ParamSlider balanceAttackSlider;
    ParamSlider balanceReleaseSlider;
    ParamSlider balanceStrengthSlider;
    ParamSlider balanceMaxSlider;
    LevelMeter balanceMeter { LevelMeter::Orientation::horizontal };
    juce::Label balanceStatusLabel;

    ParamSlider masterGainSlider;
    ParamToggle masterLimiterToggle;
    ParamSlider masterCeilingSlider;
    ParamToggle testToneToggle;
    ParamSlider testToneLevelSlider;
    LevelMeter masterMeter { LevelMeter::Orientation::horizontal };
    juce::Label masterStatusLabel;
    juce::TextButton resetMasterClipButton { "Reset Clip" };

    ParamSlider boosterInputTrim;
    ParamSlider boosterBass;
    ParamSlider boosterPresence;
    ParamSlider boosterTreble;
    ParamSlider boosterLoudness;
    ParamSlider boosterOutput;
    ParamSlider boosterCompThreshold;
    ParamSlider boosterCompMakeup;
    ParamSlider boosterCompRatio;
    ParamSlider boosterCompAttack;
    ParamSlider boosterCompRelease;
    ParamSlider boosterCeiling;
    ParamToggle boosterBypassToggle;
    ParamToggle autoLevelToggle;
    juce::TextButton autoBoostButton { "Auto Boost" };
    juce::TextButton autoBoostResetButton { "Reset Auto" };
    std::array<std::unique_ptr<juce::TextButton>, 5> presetButtons;
    LevelMeter boosterMeter { LevelMeter::Orientation::horizontal };
    juce::Label boosterMeterLabel;
    juce::Label boosterStatusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectsPage)
};

} // namespace rr
