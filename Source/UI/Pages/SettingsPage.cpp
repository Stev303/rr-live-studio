#include "UI/Pages/SettingsPage.h"

namespace rr
{
namespace
{
    juce::String formatLatencyLine (AppState& app)
    {
        return "Latency: " + juce::String (app.engine.getDeviceLatencyMs(), 1)
             + " ms (device) + " + juce::String (app.engine.getEngineLatencyMs(), 1)
             + " ms (engine) = " + juce::String (app.engine.getTotalLatencyMs(), 1) + " ms";
    }
}

SettingsPage::SettingsPage (AppState& state)
    : app (state),
      testToneToggle (app.params, ParamId::testToneEnabled, "Test Tone 1 kHz"),
      testToneLevel (app.params, ParamId::testToneDb, juce::Slider::LinearHorizontal, true)
{
    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        app.engine.getDeviceManager(),
        1, 2,   // mono/stereo microphone input
        1, 2,   // stereo output
        false, false,
        true,
        false);

    devicePanel.addAndMakeVisible (*deviceSelector);

    configureLabel (deviceLabel, 10.5f, theme::text, true);
    configureLabel (sampleRateLabel, 10.0f, theme::textDim);
    configureLabel (bufferLabel, 10.0f, theme::textDim);
    configureLabel (latencyLabel, 10.0f, theme::textDim);
    configureLabel (cpuLabel, 10.0f, theme::textDim);
    configureLabel (dropoutLabel, 10.0f, theme::textDim);
    configureLabel (errorLabel, 10.0f, theme::red);
    configureLabel (warningLabel, 10.0f, theme::amber);
    configureLabel (asioLabel, 10.0f, theme::textDim.withAlpha (0.9f));
    asioLabel.setJustificationType (juce::Justification::topLeft);
    asioLabel.setMinimumHorizontalScale (0.75f);

    errorLabel.setJustificationType (juce::Justification::topLeft);
    warningLabel.setJustificationType (juce::Justification::topLeft);
    errorLabel.setMinimumHorizontalScale (0.75f);
    warningLabel.setMinimumHorizontalScale (0.75f);

    configureLabel (modeLabel, 10.5f, theme::textDim);
    modeLabel.setText ("Mode Buffer", juce::dontSendNotification);
    modeBox.addItemList (AppState::getModeNames(), 1);
    modeBox.setText (app.getMode(), juce::dontSendNotification);
    modeBox.setTooltip ("Mengubah buffer audio. Device dapat restart sesaat.");
    modeBox.onChange = [this] { applyLatencyMode(); };

    styleActionButton (saveButton, theme::green);
    saveButton.setTooltip ("Simpan parameter, device audio, dan pengaturan loopback.");
    saveButton.onClick = [this]
    {
        app.save();
        app.diagnostics.log ("Pengaturan disimpan dari halaman Settings.");
        showInfoMessage ("Pengaturan disimpan",
                         "Parameter, konfigurasi audio device, mode buffer, dan endpoint loopback disimpan.");
    };

    statusPanel.addAndMakeVisible (deviceLabel);
    statusPanel.addAndMakeVisible (sampleRateLabel);
    statusPanel.addAndMakeVisible (bufferLabel);
    statusPanel.addAndMakeVisible (latencyLabel);
    statusPanel.addAndMakeVisible (cpuLabel);
    statusPanel.addAndMakeVisible (dropoutLabel);
    statusPanel.addAndMakeVisible (errorLabel);
    statusPanel.addAndMakeVisible (warningLabel);
    statusPanel.addAndMakeVisible (asioLabel);
    statusPanel.addAndMakeVisible (modeLabel);
    statusPanel.addAndMakeVisible (modeBox);
    statusPanel.addAndMakeVisible (saveButton);

    testPanel.addAndMakeVisible (testToneToggle);
    testPanel.addAndMakeVisible (testToneLevel);
    testPanel.addAndMakeVisible (testMeter);
    configureLabel (testLabel, 10.0f, theme::textDim);
    testLabel.setJustificationType (juce::Justification::topLeft);
    testLabel.setMinimumHorizontalScale (0.75f);
    testLabel.setText ("Test tone melewati master VST dan limiter menuju output terpilih. "
                       "Gunakan level rendah saat menguji perangkat atau virtual cable. "
                       "Matikan test tone sebelum Go Live.",
                       juce::dontSendNotification);
    testPanel.addAndMakeVisible (testLabel);
    testMeter.setInterceptsMouseClicks (false, false);

    endpointBox.setTooltip ("Pilih endpoint playback Windows yang akan ditangkap. "
                            "Endpoints adalah output perangkat, bukan aplikasi.");
    endpointBox.onChange = [this]
    {
        const int id = endpointBox.getSelectedId();

        if (juce::isPositiveAndBelow (id - 1, endpoints.size()))
            app.engine.setLoopbackEndpointId (endpoints[id - 1].id);

        updateLoopbackUi();
    };

    styleActionButton (refreshEndpointsButton);
    refreshEndpointsButton.onClick = [this] { refreshEndpoints(); };

    styleActionButton (captureButton, theme::magenta);
    captureButton.onClick = [this] { toggleLoopback(); };

    configureLabel (loopbackStatusLabel, 10.5f, theme::text);
    configureLabel (loopbackCaveatLabel, 10.0f, theme::textDim.withAlpha (0.9f));
    loopbackCaveatLabel.setJustificationType (juce::Justification::topLeft);
    loopbackCaveatLabel.setMinimumHorizontalScale (0.75f);
    loopbackCaveatLabel.setText ("WASAPI loopback menangkap seluruh audio sistem pada endpoint terpilih, "
                                 "bukan hanya YouTube. Endpoint tidak boleh sama dengan output master agar "
                                 "tidak terjadi feedback. Sample rate endpoint dikonversi ke sample rate engine; "
                                 "selisih clock ditangani resampler. Capture berjalan terpisah dari device ASIO/WASAPI utama.",
                                 juce::dontSendNotification);

    loopbackPanel.addAndMakeVisible (endpointBox);
    loopbackPanel.addAndMakeVisible (refreshEndpointsButton);
    loopbackPanel.addAndMakeVisible (captureButton);
    loopbackPanel.addAndMakeVisible (loopbackMeter);
    loopbackPanel.addAndMakeVisible (loopbackStatusLabel);
    loopbackPanel.addAndMakeVisible (loopbackCaveatLabel);

    logView.setMultiLine (true, false);
    logView.setReadOnly (true);
    logView.setCaretVisible (false);
    logView.setFont (theme::monoFont (10.0f));
    logView.setColour (juce::TextEditor::backgroundColourId, theme::background.darker (0.2f));
    logView.setColour (juce::TextEditor::textColourId, theme::text.withAlpha (0.9f));
    logView.setColour (juce::TextEditor::outlineColourId, theme::border);
    logView.setColour (juce::TextEditor::focusedOutlineColourId, theme::border);

    autoRefreshToggle.setButtonText ("Auto refresh");
    autoRefreshToggle.setToggleState (true, juce::dontSendNotification);
    autoRefreshToggle.setTooltip ("Perbarui tampilan log setiap satu detik.");

    styleActionButton (refreshLogButton);
    refreshLogButton.onClick = [this] { updateDiagnostics (true); };
    styleActionButton (exportLogButton, theme::cyan);
    exportLogButton.onClick = [this] { exportLog(); };
    styleActionButton (clearLogButton, theme::red);
    clearLogButton.onClick = [this] { clearLog(); };
    styleActionButton (resetStatsButton, theme::amber);
    resetStatsButton.setTooltip ("Reset penghitung dropout dan waktu callback audio.");
    resetStatsButton.onClick = [this] { resetStats(); };

    configureLabel (statsLabel, 10.0f, theme::textDim);
    statsLabel.setFont (theme::monoFont (10.0f));
    statsLabel.setMinimumHorizontalScale (0.7f);

    diagnosticsPanel.addAndMakeVisible (statsLabel);
    diagnosticsPanel.addAndMakeVisible (autoRefreshToggle);
    diagnosticsPanel.addAndMakeVisible (refreshLogButton);
    diagnosticsPanel.addAndMakeVisible (exportLogButton);
    diagnosticsPanel.addAndMakeVisible (clearLogButton);
    diagnosticsPanel.addAndMakeVisible (resetStatsButton);
    diagnosticsPanel.addAndMakeVisible (logView);

    content.addAndMakeVisible (devicePanel);
    content.addAndMakeVisible (statusPanel);
    content.addAndMakeVisible (testPanel);
    content.addAndMakeVisible (loopbackPanel);
    content.addAndMakeVisible (diagnosticsPanel);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollBarThickness (10);

    refreshEndpoints();
    updateStatus();
    updateLoopbackUi();
    updateDiagnostics (true);

#if JUCE_ASIO
    asioLabel.setText ("ASIO: SDK terdeteksi saat build. Pilih driver ASIO pada panel Audio Device. "
                       "Loopback YouTube tetap memakai WASAPI dan dapat memiliki clock berbeda; "
                       "resampler menyesuaikan sample rate loopback ke engine.",
                       juce::dontSendNotification);
#else
    asioLabel.setText ("ASIO: tidak tersedia pada build ini (JUCE_ASIO=0). Driver yang tampil adalah "
                       "Windows Audio (WASAPI). Untuk ASIO, set RRLIVE_ASIO_SDK_PATH ke folder ASIO SDK "
                       "lalu build ulang. WASAPI shared tetap menjadi fallback.",
                       juce::dontSendNotification);
#endif

    startTimerHz (2);
}

SettingsPage::~SettingsPage()
{
    stopTimer();
}

void SettingsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void SettingsPage::resized()
{
    viewport.setBounds (getLocalBounds());

    const int width = juce::jmax (1010, viewport.getMaximumVisibleWidth());
    constexpr int margin = 8;
    constexpr int topHeight = 628;
    constexpr int loopbackHeight = 250;
    constexpr int diagnosticsHeight = 330;
    constexpr int gap = 8;
    const int contentHeight = margin + topHeight + gap + loopbackHeight + gap
                            + diagnosticsHeight + margin;

    content.setSize (width, juce::jmax (contentHeight, viewport.getHeight()));

    auto area = juce::Rectangle<int> (0, 0, width, contentHeight).reduced (margin);

    auto topRow = area.removeFromTop (topHeight);
    const int rightWidth = juce::jmax (330, (int) (topRow.getWidth() * 0.38f));
    auto rightColumn = topRow.removeFromRight (rightWidth);
    topRow.removeFromRight (gap);

    devicePanel.setBounds (topRow);
    statusPanel.setBounds (rightColumn.removeFromTop (420));
    rightColumn.removeFromTop (gap);
    testPanel.setBounds (rightColumn);

    deviceSelector->setBounds (devicePanel.getContentBounds());

    area.removeFromTop (gap);
    loopbackPanel.setBounds (area.removeFromTop (loopbackHeight));
    area.removeFromTop (gap);
    diagnosticsPanel.setBounds (area.removeFromTop (diagnosticsHeight));

    auto statusArea = statusPanel.getContentBounds();
    deviceLabel.setBounds (statusArea.removeFromTop (16));
    sampleRateLabel.setBounds (statusArea.removeFromTop (16));
    bufferLabel.setBounds (statusArea.removeFromTop (16));
    latencyLabel.setBounds (statusArea.removeFromTop (17));
    cpuLabel.setBounds (statusArea.removeFromTop (16));
    dropoutLabel.setBounds (statusArea.removeFromTop (18));
    statusArea.removeFromTop (4);
    errorLabel.setBounds (statusArea.removeFromTop (32));
    warningLabel.setBounds (statusArea.removeFromTop (54));
    statusArea.removeFromTop (4);

    auto modeRow = statusArea.removeFromTop (26);
    modeLabel.setBounds (modeRow.removeFromLeft (90));
    modeBox.setBounds (modeRow.removeFromLeft (juce::jmin (200, modeRow.getWidth())));

    statusArea.removeFromTop (6);
    saveButton.setBounds (statusArea.removeFromTop (26).removeFromLeft (190));
    statusArea.removeFromTop (6);
    asioLabel.setBounds (statusArea);

    auto testArea = testPanel.getContentBounds();
    testToneToggle.setBounds (testArea.removeFromTop (24));
    testArea.removeFromTop (2);
    testToneLevel.setBounds (testArea.removeFromTop (38));
    testArea.removeFromTop (4);
    testMeter.setBounds (testArea.removeFromTop (18));
    testArea.removeFromTop (2);
    testLabel.setBounds (testArea);

    auto loopArea = loopbackPanel.getContentBounds();
    auto endpointRow = loopArea.removeFromTop (28);
    refreshEndpointsButton.setBounds (endpointRow.removeFromRight (86));
    endpointRow.removeFromRight (6);
    captureButton.setBounds (endpointRow.removeFromRight (154));
    endpointRow.removeFromRight (8);
    endpointBox.setBounds (endpointRow);

    loopArea.removeFromTop (6);
    auto statusRow = loopArea.removeFromTop (20);
    loopbackMeter.setBounds (statusRow.removeFromRight (juce::jmin (260, statusRow.getWidth() / 2)));
    statusRow.removeFromRight (8);
    loopbackStatusLabel.setBounds (statusRow);

    loopArea.removeFromTop (4);
    loopbackCaveatLabel.setBounds (loopArea);

    auto diagArea = diagnosticsPanel.getContentBounds();
    statsLabel.setBounds (diagArea.removeFromTop (18));
    diagArea.removeFromTop (4);

    auto buttonRow = diagArea.removeFromTop (26);
    autoRefreshToggle.setBounds (buttonRow.removeFromLeft (130));
    buttonRow.removeFromLeft (8);
    refreshLogButton.setBounds (buttonRow.removeFromLeft (84));
    buttonRow.removeFromLeft (6);
    exportLogButton.setBounds (buttonRow.removeFromLeft (94));
    buttonRow.removeFromLeft (6);
    clearLogButton.setBounds (buttonRow.removeFromLeft (94));
    buttonRow.removeFromLeft (6);
    resetStatsButton.setBounds (buttonRow.removeFromLeft (124));

    diagArea.removeFromTop (6);
    logView.setBounds (diagArea);
}

void SettingsPage::timerCallback()
{
    updateStatus();
    updateLoopbackUi();

    ++refreshCounter;

    if (refreshCounter % 2 == 0 && autoRefreshToggle.getToggleState())
        updateDiagnostics (false);
}

void SettingsPage::updateStatus()
{
    const auto meters = app.engine.getMeters();

    juce::String deviceText = "Device: " + app.engine.getDeviceDescription();

    if (deviceText != lastDeviceText)
    {
        lastDeviceText = deviceText;
        deviceLabel.setText (deviceText, juce::dontSendNotification);
        deviceLabel.setTooltip (deviceText);
    }

    sampleRateLabel.setText (app.engine.isRunning()
                                 ? "Sample rate: " + juce::String (app.engine.getCurrentSampleRate(), 0) + " Hz"
                                 : "Audio device tidak aktif",
                             juce::dontSendNotification);
    bufferLabel.setText (app.engine.isRunning()
                             ? "Buffer: " + juce::String (app.engine.getCurrentBlockSize()) + " samples"
                             : "Buffer: -",
                         juce::dontSendNotification);
    latencyLabel.setText (formatLatencyLine (app), juce::dontSendNotification);
    cpuLabel.setText ("CPU audio: " + juce::String (meters.cpuPercent, 1) + " %",
                      juce::dontSendNotification);
    dropoutLabel.setText ("Dropouts: " + juce::String (meters.dropouts)
                              + "  -  loopback overflow: " + juce::String (meters.loopbackOverflows),
                          juce::dontSendNotification);

    const auto deviceError = app.engine.getDeviceError();
    errorLabel.setText (deviceError.isNotEmpty() ? "Error device: " + deviceError
                                                 : "Error device: -",
                        juce::dontSendNotification);

    const auto startupWarning = app.getStartupWarning();
    warningLabel.setText (startupWarning.isNotEmpty() ? startupWarning
                                                      : "Tidak ada peringatan startup.",
                          juce::dontSendNotification);

    if (modeBox.getText() != app.getMode())
        modeBox.setText (app.getMode(), juce::dontSendNotification);

    testMeter.setLevelDb (meters.masterPeakDb, meters.masterPeakDb);
    testMeter.setClip (meters.clip);
}

void SettingsPage::refreshEndpoints()
{
    const auto selectedId = endpointBox.getSelectedId();
    const auto previousId = juce::isPositiveAndBelow (selectedId - 1, endpoints.size())
                                ? endpoints[selectedId - 1].id
                                : app.engine.getLoopbackEndpointId();

    endpoints = app.engine.getLoopbackEndpoints();
    endpointBox.clear (juce::dontSendNotification);

    int newSelectedId = 0;

    for (int i = 0; i < endpoints.size(); ++i)
    {
        const auto& endpoint = endpoints.getReference (i);
        endpointBox.addItem (endpoint.name + (endpoint.isDefault ? " (default output)" : ""), i + 1);

        if (endpoint.id == previousId)
            newSelectedId = i + 1;
    }

    if (endpoints.isEmpty())
    {
        endpointBox.addItem ("Tidak ada endpoint output Windows", 1);
        endpointBox.setSelectedId (1, juce::dontSendNotification);
        endpointBox.setEnabled (false);
        captureButton.setEnabled (false);
    }
    else
    {
        endpointBox.setEnabled (true);
        captureButton.setEnabled (true);

        if (newSelectedId > 0)
            endpointBox.setSelectedId (newSelectedId, juce::dontSendNotification);
    }

    updateLoopbackUi();
}

void SettingsPage::toggleLoopback()
{
    if (app.engine.isLoopbackActive())
    {
        app.engine.stopLoopback();
        app.diagnostics.log ("Loopback dihentikan dari halaman Settings.");
        updateLoopbackUi();
        return;
    }

    juce::String error;

    if (! app.engine.startLoopback (error))
    {
        showWarningMessage ("Capture tidak dimulai", error);
        updateLoopbackUi();
        return;
    }

    app.diagnostics.log ("Loopback dimulai dari halaman Settings: "
                         + app.engine.getLoopbackEndpointName());
    updateLoopbackUi();
}

void SettingsPage::updateLoopbackUi()
{
    captureButton.setButtonText (app.engine.isLoopbackActive() ? "Hentikan Capture"
                                                               : "Mulai Capture");

    const auto meters = app.engine.getMeters();
    loopbackMeter.setLevelDb (meters.loopbackPeakDb, meters.loopbackPeakDb);
    loopbackMeter.setClip (false);

    juce::String status;

    if (app.engine.isLoopbackCapturing())
    {
        status = "Capture aktif: " + app.engine.getLoopbackEndpointName()
               + "  -  " + juce::String (app.engine.getLoopbackCapture().getCaptureSampleRate(), 0)
               + " Hz  -  overflow " + juce::String (meters.loopbackOverflows);
    }
    else if (app.engine.isLoopbackActive())
    {
        const auto error = app.engine.getLoopbackError();
        status = error.isNotEmpty() ? "Capture gagal/tertunda: " + error
                                    : "Capture diminta, menunggu audio device...";
    }
    else if (endpoints.isEmpty())
    {
        status = "Tidak ada endpoint output Windows aktif.";
    }
    else
    {
        status = "Capture berhenti. Pilih endpoint lalu klik Mulai Capture.";
    }

    if (status != lastLoopbackText)
    {
        lastLoopbackText = status;
        loopbackStatusLabel.setText (status, juce::dontSendNotification);
    }
}

void SettingsPage::updateDiagnostics (bool force)
{
    const auto text = app.diagnostics.getRecentText (80);

    if (force || text != lastLogText)
    {
        lastLogText = text;
        logView.setText (text, false);
    }

    const auto stats = app.diagnostics.getAudioStats();
    statsLabel.setText ("Callback " + juce::String (stats.callbackCount)
                            + "  -  dropout " + juce::String (stats.dropoutCount)
                            + "  -  avg " + juce::String (stats.averageCallbackMs, 2) + " ms"
                            + "  -  max " + juce::String (stats.maxCallbackMs, 2) + " ms"
                            + "  -  clip " + (stats.clipped ? "ya" : "tidak")
                            + "  -  file: " + app.diagnostics.getLogFile().getFullPathName(),
                        juce::dontSendNotification);
}

void SettingsPage::exportLog()
{
    const auto defaultFile = app.getDataFolder()
                                 .getChildFile ("diagnostics-"
                                                + juce::Time::getCurrentTime()
                                                      .formatted ("%Y%m%d-%H%M%S")
                                                + ".log");

    exportChooser = std::make_unique<juce::FileChooser> ("Export diagnostics log",
                                                         defaultFile,
                                                         "*.log");

    exportChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                    | juce::FileBrowserComponent::canSelectFiles
                                    | juce::FileBrowserComponent::warnAboutOverwriting,
                                [this] (const juce::FileChooser& chooser)
                                {
                                    const auto file = chooser.getResult();

                                    if (file.getFullPathName().isEmpty())
                                        return;

                                    if (app.diagnostics.exportTo (file))
                                        showInfoMessage ("Log diekspor", file.getFullPathName());
                                    else
                                        showWarningMessage ("Export gagal",
                                                            "Log kosong atau file tidak dapat ditulis.");
                                });
}

void SettingsPage::clearLog()
{
    showConfirm ("Hapus Log",
                 "Semua baris log di memori akan dihapus. File log yang sudah diekspor tidak terpengaruh.",
                 "Hapus",
                 [this]
                 {
                     app.diagnostics.clear();
                     updateDiagnostics (true);
                 });
}

void SettingsPage::resetStats()
{
    app.diagnostics.resetAudioStats();
    app.engine.resetClipIndicators();
    updateDiagnostics (true);
}

void SettingsPage::applyLatencyMode()
{
    const auto mode = modeBox.getText();

    if (mode.isEmpty() || mode == app.getMode())
        return;

    juce::String error;

    juce::MouseCursor::showWaitCursor();
    app.setMode (mode, error);
    juce::MouseCursor::hideWaitCursor();

    if (error.isNotEmpty())
    {
        showWarningMessage ("Mode buffer tidak berubah", error);
        modeBox.setText (app.getMode(), juce::dontSendNotification);
        return;
    }

    app.save();
    app.diagnostics.log ("Mode buffer diubah ke: " + mode);
    updateStatus();
}

} // namespace rr
