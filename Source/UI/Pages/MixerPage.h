#pragma once

#include "App/AppState.h"
#include "UI/RoutingDiagram.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rr
{
class MixerPage : public juce::Component,
                  private juce::Timer
{
public:
    explicit MixerPage (AppState& state);
    ~MixerPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void resetClipAndPeak();

    AppState& app;
    juce::Viewport viewport;
    juce::Component content;

    Panel micPanel { "Mic - Vokal", theme::green };
    Panel musicPanel { "YouTube Music", theme::magenta };
    Panel samplerPanel { "Sampler", theme::amber };
    Panel vocalPanel { "Vocal FX", theme::cyan };
    Panel masterPanel { "Master", theme::red };
    Panel routingPanel { "Routing - sama dengan engine", theme::green };

    ParamSlider micFader;
    ParamSlider micInputGain;
    ParamToggle micMute;
    ParamToggle micSolo;
    LevelMeter micMeter { LevelMeter::Orientation::vertical };
    juce::Label micStatus;

    ParamSlider musicFader;
    ParamToggle musicMute;
    ParamToggle musicSolo;
    LevelMeter musicMeter { LevelMeter::Orientation::vertical };
    juce::Label musicStatus;
    juce::Label musicCaptureStatus;

    ParamSlider samplerFader;
    ParamToggle samplerMute;
    ParamToggle samplerSolo;
    LevelMeter samplerMeter { LevelMeter::Orientation::vertical };
    juce::Label samplerStatus;
    juce::TextButton samplerPageButton { "Buka Sampler" };

    ParamSlider vocalFader;
    LevelMeter vocalMeter { LevelMeter::Orientation::vertical };
    ParamToggle reverbToggle;
    ParamSlider reverbMixSlider;
    ParamToggle delayToggle;
    ParamSlider delayMixSlider;
    juce::Label vocalStatus;

    ParamSlider masterFader;
    LevelMeter masterMeter { LevelMeter::Orientation::vertical };
    ParamToggle masterLimiterToggle;
    ParamSlider masterCeilingSlider;
    juce::TextButton resetClipButton { "Reset Clip" };
    juce::Label masterStatus;

    RoutingDiagram routingDiagram;
    juce::Label noteLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerPage)
};

} // namespace rr
