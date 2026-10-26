#include "UI/Pages/DashboardPage.h"

namespace rr
{
namespace
{
    juce::String formatMeterDb (float db)
    {
        if (db <= -99.0f)
            return juce::String::fromUTF8 ("\xe2\x80\x94");

        return juce::String (db, 1) + " dB";
    }

    // Panel content bounds are in panel-local space; the dashboard controls are
    // children of the page, so they must be translated into page coordinates.
    juce::Rectangle<int> panelContentInPage (const Panel& panel)
    {
        return panel.getContentBounds().translated (panel.getX(), panel.getY());
    }
}

DashboardPage::DashboardPage (AppState& state, std::function<void()> onGoLive)
    : app (state),
      goLiveCallback (std::move (onGoLive)),
      micGain (app.params, ParamId::micGainDb, juce::Slider::LinearHorizontal, true),
      musicFader (app.params, ParamId::musicMixDb, juce::Slider::LinearHorizontal, true),
      masterGain (app.params, ParamId::masterGainDb, juce::Slider::LinearHorizontal, true),
      transpose (app.params, ParamId::transpose, juce::Slider::LinearHorizontal, true),
      micMute (app.params, ParamId::micMute, "Mute Mic"),
      testTone (app.params, ParamId::testToneEnabled, "Test Tone 1 kHz"),
      routing (app)
{
    configureLabel (micCaption, 11.0f, theme::cyan, true, juce::Justification::centred);
    micCaption.setText ("MIC", juce::dontSendNotification);
    configureLabel (musicCaption, 11.0f, theme::magenta, true, juce::Justification::centred);
    musicCaption.setText ("MUSIC", juce::dontSendNotification);
    configureLabel (masterCaption, 11.0f, theme::green, true, juce::Justification::centred);
    masterCaption.setText ("MASTER", juce::dontSendNotification);

    for (auto* label : { &micValue, &musicValue, &masterValue })
    {
        configureLabel (*label, 11.0f, theme::text, false, juce::Justification::centred);
        label->setFont (theme::monoFont (11.0f));
        label->setText ("\xe2\x80\x94", juce::dontSendNotification);
    }

    styleActionButton (captureButton, theme::magenta);
    captureButton.setTooltip ("WASAPI loopback menangkap seluruh audio sistem pada endpoint terpilih.");
    captureButton.onClick = [this] { toggleCapture(); };

    styleActionButton (resetClipButton, theme::amber);
    resetClipButton.onClick = [this]
    {
        app.engine.resetClipIndicators();
        app.engine.getVocalChain().clearClipFlag();
        app.engine.getMusicBooster().clearClipFlag();
    };

    styleActionButton (goLiveButton, theme::magenta);
    goLiveButton.setColour (juce::TextButton::buttonColourId, theme::magenta.withAlpha (0.2f));
    goLiveButton.onClick = [this]
    {
        if (goLiveCallback != nullptr)
            goLiveCallback();
    };

    styleActionButton (settingsButton);
    settingsButton.onClick = [this] { app.navigateTo (8); };
    styleActionButton (youtubeButton, theme::magenta);
    youtubeButton.onClick = [this] { app.navigateTo (1); };
    styleActionButton (vocalButton, theme::green);
    vocalButton.onClick = [this] { app.navigateTo (2); };

    configureLabel (stateLabel, 12.0f, theme::cyan, true);
    configureLabel (deviceLabel, 10.5f, theme::textDim);
    configureLabel (latencyLabel, 10.5f, theme::textDim);
    configureLabel (cpuLabel, 10.5f, theme::textDim);
    configureLabel (loopbackLabel, 10.5f, theme::textDim);
    configureLabel (dropoutsLabel, 10.5f, theme::textDim);
    configureLabel (warningLabel, 10.5f, theme::amber);
    warningLabel.setJustificationType (juce::Justification::topLeft);
    configureLabel (disclaimerLabel, 9.5f, theme::textDim.withAlpha (0.8f));
    disclaimerLabel.setJustificationType (juce::Justification::topLeft);
    disclaimerLabel.setText ("DSP lokal, tanpa cloud. Music capture memakai WASAPI loopback "
                             "sehingga menangkap seluruh suara pada endpoint, bukan hanya YouTube.",
                             juce::dontSendNotification);

    addAndMakeVisible (livePanel);
    addAndMakeVisible (quickPanel);
    addAndMakeVisible (statusPanel);
    addAndMakeVisible (routingPanel);
    addAndMakeVisible (routing);

    addAndMakeVisible (micMeter);
    addAndMakeVisible (musicMeter);
    addAndMakeVisible (masterMeter);
    addAndMakeVisible (micCaption);
    addAndMakeVisible (musicCaption);
    addAndMakeVisible (masterCaption);
    addAndMakeVisible (micValue);
    addAndMakeVisible (musicValue);
    addAndMakeVisible (masterValue);

    addAndMakeVisible (micGain);
    addAndMakeVisible (musicFader);
    addAndMakeVisible (masterGain);
    addAndMakeVisible (transpose);
    addAndMakeVisible (micMute);
    addAndMakeVisible (testTone);
    addAndMakeVisible (captureButton);
    addAndMakeVisible (resetClipButton);
    addAndMakeVisible (goLiveButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (youtubeButton);
    addAndMakeVisible (vocalButton);

    addAndMakeVisible (stateLabel);
    addAndMakeVisible (deviceLabel);
    addAndMakeVisible (latencyLabel);
    addAndMakeVisible (cpuLabel);
    addAndMakeVisible (loopbackLabel);
    addAndMakeVisible (dropoutsLabel);
    addAndMakeVisible (warningLabel);
    addAndMakeVisible (disclaimerLabel);

    updateCaptureButton();
    updateStatus();
    startTimerHz (12);
}

DashboardPage::~DashboardPage()
{
    stopTimer();
}

void DashboardPage::paint (juce::Graphics&)
{
}

void DashboardPage::resized()
{
    auto area = getLocalBounds().reduced (14);
    auto topArea = area.removeFromTop (juce::jlimit (270, 320, area.getHeight() * 45 / 100));
    area.removeFromTop (12);

    livePanel.setBounds (topArea.removeFromLeft (juce::jlimit (260, 300, topArea.getWidth() / 4)));
    topArea.removeFromLeft (12);
    statusPanel.setBounds (topArea.removeFromRight (juce::jlimit (250, 290, topArea.getWidth() / 4)));
    topArea.removeFromRight (12);
    quickPanel.setBounds (topArea);

    routingPanel.setBounds (area);
    routing.setBounds (panelContentInPage (routingPanel).reduced (8));

    auto live = panelContentInPage (livePanel).reduced (10);
    const auto meterWidth = live.getWidth() / 3;

    auto drawColumn = [this] (juce::Rectangle<int> column, juce::Label& caption,
                              juce::Label& value, LevelMeter& meter)
    {
        caption.setBounds (column.removeFromTop (18));
        value.setBounds (column.removeFromBottom (18));
        meter.setBounds (column.reduced (16, 2));
    };

    drawColumn (live.removeFromLeft (meterWidth), micCaption, micValue, micMeter);
    drawColumn (live.removeFromLeft (meterWidth), musicCaption, musicValue, musicMeter);
    drawColumn (live, masterCaption, masterValue, masterMeter);

    auto quick = panelContentInPage (quickPanel).reduced (12);
    const auto rowHeight = juce::jmax (28, (quick.getHeight() - 108) / 4);

    micGain.setBounds (quick.removeFromTop (rowHeight));
    musicFader.setBounds (quick.removeFromTop (rowHeight));
    masterGain.setBounds (quick.removeFromTop (rowHeight));
    transpose.setBounds (quick.removeFromTop (rowHeight));
    quick.removeFromTop (4);

    auto toggleRow = quick.removeFromTop (28);
    micMute.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2).reduced (2, 0));
    testTone.setBounds (toggleRow.reduced (2, 0));
    quick.removeFromTop (4);

    auto actionRow = quick.removeFromTop (30);
    captureButton.setBounds (actionRow.removeFromLeft (actionRow.getWidth() / 2).reduced (2, 0));
    resetClipButton.setBounds (actionRow.reduced (2, 0));
    quick.removeFromTop (6);

    goLiveButton.setBounds (quick.removeFromBottom (juce::jmin (44, quick.getHeight())));

    auto status = panelContentInPage (statusPanel).reduced (10);
    stateLabel.setBounds (status.removeFromTop (22));
    deviceLabel.setBounds (status.removeFromTop (18));
    latencyLabel.setBounds (status.removeFromTop (18));
    cpuLabel.setBounds (status.removeFromTop (18));
    loopbackLabel.setBounds (status.removeFromTop (18));
    dropoutsLabel.setBounds (status.removeFromTop (18));
    status.removeFromTop (4);

    auto buttonRow = status.removeFromTop (28);
    settingsButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() / 2).reduced (2, 0));
    youtubeButton.setBounds (buttonRow.reduced (2, 0));
    status.removeFromTop (4);
    vocalButton.setBounds (status.removeFromTop (28).reduced (2, 0));
    status.removeFromTop (4);

    disclaimerLabel.setBounds (status.removeFromBottom (juce::jmin (48, status.getHeight())));
    warningLabel.setBounds (status);
}

void DashboardPage::timerCallback()
{
    const auto meters = app.engine.getMeters();

    micMeter.setLevelDb (meters.micPeakDb);
    musicMeter.setLevelDb (meters.musicBusPeakDb);
    masterMeter.setLevelDb (meters.masterPeakDb);
    micMeter.setClip (meters.clip);
    musicMeter.setClip (meters.clip);
    masterMeter.setClip (meters.clip);

    micValue.setText (formatMeterDb (meters.micPeakDb), juce::dontSendNotification);
    musicValue.setText (formatMeterDb (meters.musicBusPeakDb), juce::dontSendNotification);
    masterValue.setText (formatMeterDb (meters.masterPeakDb), juce::dontSendNotification);

    updateCaptureButton();
    updateStatus();
}

void DashboardPage::updateCaptureButton()
{
    const auto active = app.engine.isLoopbackActive();

    if (active)
    {
        captureButton.setButtonText ("Hentikan Music Capture");
        captureButton.setColour (juce::TextButton::buttonColourId, theme::magenta.withAlpha (0.22f));
    }
    else
    {
        captureButton.setButtonText ("Mulai Music Capture");
        captureButton.setColour (juce::TextButton::buttonColourId, theme::panelAlt);
    }
}

void DashboardPage::toggleCapture()
{
    if (app.engine.isLoopbackActive())
    {
        app.engine.stopLoopback();
        updateCaptureButton();
        return;
    }

    juce::String error;

    if (! app.engine.startLoopback (error))
        showWarningMessage ("Music capture", error);

    updateCaptureButton();
}

void DashboardPage::updateStatus()
{
    const auto meters = app.engine.getMeters();

    if (! app.engine.isRunning())
    {
        stateLabel.setText ("AUDIO OFF", juce::dontSendNotification);
        stateLabel.setColour (juce::Label::textColourId, theme::red);
    }
    else if (app.engine.isLoopbackCapturing())
    {
        stateLabel.setText ("MUSIC CAPTURE AKTIF", juce::dontSendNotification);
        stateLabel.setColour (juce::Label::textColourId, theme::green);
    }
    else
    {
        stateLabel.setText ("ENGINE AUDIO SIAP", juce::dontSendNotification);
        stateLabel.setColour (juce::Label::textColourId, theme::cyan);
    }

    auto device = app.engine.getDeviceDescription();

    if (device.length() > 44)
        device = device.substring (0, 42) + "...";

    deviceLabel.setText ("Device: " + (device.isEmpty() ? juce::String ("-") : device),
                         juce::dontSendNotification);
    deviceLabel.setTooltip (app.engine.getDeviceDescription());

    latencyLabel.setText ("Latensi: dev " + juce::String (app.engine.getDeviceLatencyMs(), 1)
                              + " ms / engine " + juce::String (app.engine.getEngineLatencyMs(), 1)
                              + " ms / total " + juce::String (app.engine.getTotalLatencyMs(), 1) + " ms",
                          juce::dontSendNotification);
    cpuLabel.setText ("CPU DSP: " + juce::String (meters.cpuPercent, 1) + "%  |  "
                          + juce::String (app.engine.getCurrentSampleRate() / 1000.0, 1) + " kHz / "
                          + juce::String (app.engine.getCurrentBlockSize()) + " smp",
                      juce::dontSendNotification);
    loopbackLabel.setText ("Loopback: " + app.engine.getLoopbackEndpointName(),
                           juce::dontSendNotification);

    auto dropouts = juce::String ("Dropout callback: ") + juce::String (meters.dropouts)
                  + "  |  Overflow capture: " + juce::String (meters.loopbackOverflows);

    if (meters.captureSampleRate > 0.0)
        dropouts << "  |  Capture " << juce::String (meters.captureSampleRate / 1000.0, 1) << " kHz";

    dropoutsLabel.setText (dropouts, juce::dontSendNotification);

    auto warning = app.engine.getLoopbackWarning();

    if (! app.engine.isRunning() && app.getDeviceInitError().isNotEmpty())
        warning = "Engine audio belum berjalan: " + app.getDeviceInitError();
    else if (warning.isEmpty() && app.getStartupWarning().isNotEmpty())
        warning = app.getStartupWarning();

    warningLabel.setTooltip (warning);
    warningLabel.setText (warning.replace ("\n", " "), juce::dontSendNotification);
    warningLabel.setColour (juce::Label::textColourId,
                            warning.isNotEmpty() ? theme::amber : theme::textDim);
}

} // namespace rr
