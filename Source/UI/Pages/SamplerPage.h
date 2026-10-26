#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <memory>

namespace rr
{
class SamplerPage : public juce::Component,
                    private juce::Timer
{
public:
    explicit SamplerPage (AppState& state);
    ~SamplerPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class PadComponent;

    void timerCallback() override;
    void loadSampleForPad (int padIndex);
    void renamePad (int padIndex);
    void clearPadWithConfirm (int padIndex);
    void refreshPadStates();
    void updateStatus();

    AppState& app;
    juce::Viewport viewport;
    juce::Component content;

    Panel headerPanel { "Sampler - 10 Pads", theme::amber };

    ParamSlider samplerFader;
    ParamToggle samplerMute;
    ParamToggle samplerSolo;
    LevelMeter samplerMeter { LevelMeter::Orientation::horizontal };
    juce::TextButton stopAllButton { "Stop Semua" };
    juce::TextButton saveSessionButton { "Simpan Sesi" };
    juce::Label meterLabel;
    juce::Label statusLabel;
    juce::Label infoLabel;

    std::array<std::unique_ptr<PadComponent>, SamplerEngine::numPads> pads;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String lastStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerPage)
};

} // namespace rr
