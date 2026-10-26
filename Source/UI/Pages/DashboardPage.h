#pragma once

#include "App/AppState.h"
#include "UI/RoutingDiagram.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace rr
{
class DashboardPage : public juce::Component,
                      private juce::Timer
{
public:
    DashboardPage (AppState& state, std::function<void()> onGoLive);
    ~DashboardPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateCaptureButton();
    void toggleCapture();
    void updateStatus();

    AppState& app;
    std::function<void()> goLiveCallback;

    Panel livePanel { "Live Meter", theme::cyan };
    Panel quickPanel { "Kontrol Cepat", theme::magenta };
    Panel statusPanel { "Status Sistem", theme::green };
    Panel routingPanel { "Alur Sinyal (aktual)", theme::cyan };

    LevelMeter micMeter { LevelMeter::Orientation::vertical };
    LevelMeter musicMeter { LevelMeter::Orientation::vertical };
    LevelMeter masterMeter { LevelMeter::Orientation::vertical };

    juce::Label micCaption;
    juce::Label musicCaption;
    juce::Label masterCaption;
    juce::Label micValue;
    juce::Label musicValue;
    juce::Label masterValue;

    ParamSlider micGain;
    ParamSlider musicFader;
    ParamSlider masterGain;
    ParamSlider transpose;
    ParamToggle micMute;
    ParamToggle testTone;

    juce::TextButton captureButton { "Mulai Music Capture" };
    juce::TextButton resetClipButton { "Reset Clip" };
    juce::TextButton goLiveButton { "GO LIVE" };
    juce::TextButton settingsButton { "Audio Settings" };
    juce::TextButton youtubeButton { "Buka YouTube" };
    juce::TextButton vocalButton { "Vocal Chain" };

    juce::Label stateLabel;
    juce::Label deviceLabel;
    juce::Label latencyLabel;
    juce::Label cpuLabel;
    juce::Label loopbackLabel;
    juce::Label dropoutsLabel;
    juce::Label warningLabel;
    juce::Label disclaimerLabel;

    RoutingDiagram routing;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DashboardPage)
};

} // namespace rr
