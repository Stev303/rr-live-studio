#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>

namespace rr
{
/** Modal-style Go Live checklist overlay.

    The overlay verifies the local signal path and shows honest guidance for
    routing PC audio to a phone.  It does not control BIGO LIVE, and it never
    claims that a plain USB connection is enough.
*/
class GoLiveDialog : public juce::Component,
                     private juce::Timer
{
public:
    explicit GoLiveDialog (AppState& state, std::function<void()> onClose = {});
    ~GoLiveDialog() override;

    void open();
    void close();

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    class CheckRow;

    void timerCallback() override;
    void updateChecklist();
    void updateOutputPanel();
    void resetClip();

    AppState& app;
    std::function<void()> closeCallback;
    juce::Rectangle<int> dialogArea;

    Panel checklistPanel { "Checklist Go Live", theme::green };
    Panel outputPanel { "Output Akhir", theme::magenta };
    Panel guidePanel { "Panduan Output ke HP / BIGO LIVE", theme::amber };

    juce::TextButton closeButton { "X" };
    juce::Label readyLabel;
    juce::Label checklistHint;
    std::array<std::unique_ptr<CheckRow>, 6> checkRows;
    juce::ToggleButton manualOutputToggle;
    juce::ToggleButton manualHeadphoneToggle;

    juce::Label outputDeviceLabel;
    juce::Label outputFormatLabel;
    juce::Label outputLatencyLabel;
    juce::Label masterChainLabel;
    juce::Label peakLabel;
    LevelMeter finalMeter { LevelMeter::Orientation::horizontal };
    ParamToggle testToneToggle;
    ParamSlider testToneLevel;
    juce::TextButton resetClipButton { "Reset Clip" };
    juce::Label testNoteLabel;

    juce::Label guideLabel;

    juce::TextButton settingsButton { "Buka Settings" };
    juce::TextButton youtubeButton { "Buka YouTube" };
    juce::TextButton closeFooterButton { "Tutup" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoLiveDialog)
};

} // namespace rr
