#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"
#include "YouTube/YouTubeBrowser.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rr
{
class YouTubePage : public juce::Component,
                    private juce::Timer
{
public:
    explicit YouTubePage (AppState& state);
    ~YouTubePage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshEndpoints();
    void toggleCapture();
    void updateCaptureUi();
    void updateBrowserToolbar();
    void applyVideoVolume();
    void loadBoosterPreset (const juce::String& name);
    void layoutSideContent();

    AppState& app;
    YouTubeBrowser browser;

    Panel browserPanel { "YouTube (WebView2 resmi)", theme::magenta };
    juce::Viewport sideViewport;
    juce::Component sideContent;
    Panel capturePanel { "Music Capture - WASAPI Loopback", theme::magenta };
    Panel pitchPanel { "YouTube Pitch / Key Changer", theme::cyan };
    Panel boosterPanel { "YouTube Audio Booster", theme::green };

    juce::TextButton backButton { "<" };
    juce::TextButton forwardButton { ">" };
    juce::TextButton reloadButton { "Muat" };
    juce::TextButton homeButton { "Home" };
    juce::TextEditor searchEditor;
    juce::TextButton searchButton { "Cari" };
    juce::TextButton showHideButton { "Sembunyikan" };
    juce::Label browserStatusLabel;
    juce::Label browserHiddenLabel;

    juce::ComboBox endpointBox;
    juce::TextButton refreshEndpointsButton { "Refresh" };
    juce::TextButton captureToggleButton { "Mulai Capture" };
    juce::TextButton settingsButton { "Audio Settings" };
    juce::Label captureWarningLabel;
    juce::Label captureStatusLabel;
    LevelMeter loopbackMeter { LevelMeter::Orientation::horizontal };
    juce::Array<LoopbackCapture::EndpointInfo> endpoints;

    ParamSlider transposeSlider;
    ParamToggle pitchBypassToggle;
    ParamCombo musicKeyCombo;
    juce::TextButton minusFiveButton { "-5 st" };
    juce::TextButton plusFiveButton { "+5 st" };
    juce::TextButton resetPitchButton { "Reset" };
    juce::Label keyDisplayLabel;
    juce::Label pitchStatusLabel;
    ParamSlider videoVolumeSlider;

    ParamSlider boosterInputTrim;
    ParamSlider boosterBass;
    ParamSlider boosterPresence;
    ParamSlider boosterTreble;
    ParamSlider boosterLoudness;
    ParamSlider boosterOutput;
    ParamToggle boosterBypassToggle;
    ParamToggle autoLevelToggle;
    juce::TextButton autoBoostButton { "Auto Boost" };
    juce::TextButton autoBoostResetButton { "Reset Auto" };
    juce::TextButton presetOriginalButton { "Original" };
    juce::TextButton presetKaraokeButton { "Karaoke Clear" };
    juce::TextButton presetWarmButton { "Warm Music" };
    juce::TextButton presetPunchyButton { "Punchy" };
    juce::TextButton presetLoudButton { "Loud but Safe" };
    LevelMeter musicMeter { LevelMeter::Orientation::horizontal };
    juce::Label boosterStatusLabel;
    juce::Label boosterMeterLabel;

    bool browserVisible = true;
    int lastVideoVolume = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouTubePage)
};

} // namespace rr
