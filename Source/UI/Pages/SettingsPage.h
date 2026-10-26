#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

namespace rr
{
class SettingsPage : public juce::Component,
                     private juce::Timer
{
public:
    explicit SettingsPage (AppState& state);
    ~SettingsPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshEndpoints();
    void toggleLoopback();
    void updateLoopbackUi();
    void updateStatus();
    void updateDiagnostics (bool force);
    void exportLog();
    void clearLog();
    void resetStats();
    void applyLatencyMode();

    AppState& app;
    juce::Viewport viewport;
    juce::Component content;

    Panel devicePanel { "Audio Device - ASIO / WASAPI", theme::cyan };
    Panel statusPanel { "Status Audio", theme::green };
    Panel testPanel { "Test Output", theme::amber };
    Panel loopbackPanel { "YouTube Music Bus - WASAPI Loopback", theme::magenta };
    Panel diagnosticsPanel { "Diagnostics", theme::red };

    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;

    juce::Label deviceLabel;
    juce::Label sampleRateLabel;
    juce::Label bufferLabel;
    juce::Label latencyLabel;
    juce::Label cpuLabel;
    juce::Label dropoutLabel;
    juce::Label errorLabel;
    juce::Label warningLabel;
    juce::Label asioLabel;
    juce::ComboBox modeBox;
    juce::Label modeLabel;
    juce::TextButton saveButton { "Simpan Pengaturan" };

    ParamToggle testToneToggle;
    ParamSlider testToneLevel;
    LevelMeter testMeter { LevelMeter::Orientation::horizontal };
    juce::Label testLabel;

    juce::ComboBox endpointBox;
    juce::TextButton refreshEndpointsButton { "Refresh" };
    juce::TextButton captureButton { "Mulai Capture" };
    LevelMeter loopbackMeter { LevelMeter::Orientation::horizontal };
    juce::Label loopbackStatusLabel;
    juce::Label loopbackCaveatLabel;
    juce::Array<LoopbackCapture::EndpointInfo> endpoints;

    juce::TextEditor logView;
    juce::ToggleButton autoRefreshToggle;
    juce::TextButton refreshLogButton { "Refresh" };
    juce::TextButton exportLogButton { "Export..." };
    juce::TextButton clearLogButton { "Hapus Log" };
    juce::TextButton resetStatsButton { "Reset Statistik" };
    juce::Label statsLabel;
    juce::String lastLogText;
    std::unique_ptr<juce::FileChooser> exportChooser;

    int refreshCounter = 0;
    juce::String lastDeviceText;
    juce::String lastLoopbackText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPage)
};

} // namespace rr
