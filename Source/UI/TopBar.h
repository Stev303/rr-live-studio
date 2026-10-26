#pragma once

#include "App/AppState.h"
#include "UI/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rr
{
class TopBar : public juce::Component,
               private juce::Timer
{
public:
    TopBar (AppState& state, std::function<void()> onGoLive, std::function<void (int)> onNavigate);
    ~TopBar() override;

    void setPageTitle (const juce::String& title);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void updateStatus();

    AppState& app;
    std::function<void()> goLiveCallback;
    std::function<void (int)> navigateCallback;

    juce::Label titleLabel;
    juce::Label modeCaption;
    juce::ComboBox modeBox;
    juce::Label deviceLabel;
    juce::Label sampleRateLabel;
    juce::Label cpuLabel;
    juce::Label connectionLabel;
    juce::TextButton goLiveButton { "GO LIVE" };
    bool updatingMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
};

} // namespace rr
