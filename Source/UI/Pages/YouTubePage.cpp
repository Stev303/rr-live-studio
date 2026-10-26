#include "UI/Pages/YouTubePage.h"
#include "Utilities/MusicTheory.h"

namespace rr
{
YouTubePage::YouTubePage (AppState& state)
    : app (state),
      browser (app.getDataFolder().getChildFile ("WebView2")),
      transposeSlider (app.params, ParamId::transpose, juce::Slider::LinearHorizontal, true),
      pitchBypassToggle (app.params, ParamId::pitchBypass, "Pitch Bypass"),
      musicKeyCombo (app.params, ParamId::musicKey,
                     [] { juce::StringArray keys; for (auto* name : music::keyNames) keys.add (name); return keys; }(),
                     "Kunci Referensi"),
      videoVolumeSlider (app.params, ParamId::youtubeVideoVolume, juce::Slider::LinearHorizontal, true),
      boosterInputTrim (app.params, ParamId::musicInputTrimDb, juce::Slider::Rotary, true),
      boosterBass (app.params, ParamId::musicBassDb, juce::Slider::Rotary, true),
      boosterPresence (app.params, ParamId::musicPresenceDb, juce::Slider::Rotary, true),
      boosterTreble (app.params, ParamId::musicTrebleDb, juce::Slider::Rotary, true),
      boosterLoudness (app.params, ParamId::musicLoudness, juce::Slider::Rotary, true),
      boosterOutput (app.params, ParamId::boosterOutputGainDb, juce::Slider::Rotary, true),
      boosterBypassToggle (app.params, ParamId::boosterBypass, "Booster Bypass"),
      autoLevelToggle (app.params, ParamId::boosterAutoLevel, "Auto Level")
{
    // Browser toolbar
    for (auto* button : { &backButton, &forwardButton, &reloadButton, &homeButton,
                          &searchButton, &showHideButton })
    {
        styleActionButton (*button);
    }

    backButton.setTooltip ("Kembali");
    forwardButton.setTooltip ("Maju");
    reloadButton.setTooltip ("Muat ulang halaman");
    homeButton.setTooltip ("Halaman utama YouTube");
    showHideButton.setTooltip ("Sembunyikan atau tampilkan browser; audio tetap berjalan.");

    backButton.onClick = [this] { browser.goBack(); };
    forwardButton.onClick = [this] { browser.goForward(); };
    reloadButton.onClick = [this] { browser.reload(); };
    homeButton.onClick = [this] { browser.loadHome(); };

    configureLabel (browserStatusLabel, 10.0f, theme::textDim);
    browserStatusLabel.setText (browser.isBrowserAvailable()
                                    ? "WebView2 " + browser.getRuntimeVersion()
                                    : "WebView2 runtime tidak tersedia. Petunjuk terlihat di bawah.",
                                juce::dontSendNotification);

    configureLabel (browserHiddenLabel, 12.0f, theme::textDim, false, juce::Justification::centred);
    browserHiddenLabel.setJustificationType (juce::Justification::centred);
    browserHiddenLabel.setText ("Browser disembunyikan.\n\n"
                                "Audio YouTube yang sedang diputar tetap berjalan dan tetap dapat "
                                "ditangkap lewat WASAPI loopback selama halaman tidak dihentikan.",
                                juce::dontSendNotification);

    searchEditor.setFont (theme::font (12.0f));
    searchEditor.setTextToShowWhenEmpty ("Cari lagu atau video di YouTube...", theme::textDim);
    searchEditor.setReturnKeyStartsNewLine (false);
    searchEditor.onReturnKey = [this] { browser.searchFor (searchEditor.getText()); };
    searchEditor.onEscapeKey = [this] { searchEditor.clear(); };

    searchButton.onClick = [this] { browser.searchFor (searchEditor.getText()); };

    showHideButton.onClick = [this]
    {
        browserVisible = ! browserVisible;
        browser.setVisible (browserVisible);
        browserHiddenLabel.setVisible (! browserVisible);
        showHideButton.setButtonText (browserVisible ? "Sembunyikan" : "Tampilkan");
        resized();
    };

    browser.onBlockedNavigation = [this] (const juce::String& url)
    {
        browserStatusLabel.setText ("Navigasi diblokir: browser ini hanya memuat YouTube dan login Google.",
                                    juce::dontSendNotification);
        browserStatusLabel.setTooltip (url);
    };
    browser.onLoadError = [this] (const juce::String& error)
    {
        browserStatusLabel.setText ("Gagal memuat halaman: " + error, juce::dontSendNotification);
    };
    browser.onNavigationChanged = [this]
    {
        browserStatusLabel.setText ("Halaman YouTube dimuat. Tidak ada bypass iklan/DRM/login.",
                                    juce::dontSendNotification);
        applyVideoVolume();
    };

    addAndMakeVisible (browserPanel);
    browserPanel.addAndMakeVisible (backButton);
    browserPanel.addAndMakeVisible (forwardButton);
    browserPanel.addAndMakeVisible (reloadButton);
    browserPanel.addAndMakeVisible (homeButton);
    browserPanel.addAndMakeVisible (searchEditor);
    browserPanel.addAndMakeVisible (searchButton);
    browserPanel.addAndMakeVisible (showHideButton);
    browserPanel.addAndMakeVisible (browserStatusLabel);
    browserPanel.addAndMakeVisible (browser);
    browserPanel.addAndMakeVisible (browserHiddenLabel);
    browserHiddenLabel.setVisible (false);

    // Capture panel
    styleActionButton (refreshEndpointsButton, theme::magenta);
    refreshEndpointsButton.onClick = [this] { refreshEndpoints(); };
    styleActionButton (captureToggleButton, theme::magenta);
    captureToggleButton.onClick = [this] { toggleCapture(); };
    styleActionButton (settingsButton);
    settingsButton.onClick = [this] { app.navigateTo (8); };

    endpointBox.setTextWhenNothingSelected ("Pilih endpoint output Windows");
    endpointBox.onChange = [this]
    {
        const auto index = endpointBox.getSelectedItemIndex();

        if (juce::isPositiveAndBelow (index, endpoints.size()))
            app.engine.setLoopbackEndpointId (endpoints[index].id);

        updateCaptureUi();
    };

    configureLabel (captureWarningLabel, 9.5f, theme::amber);
    captureWarningLabel.setJustificationType (juce::Justification::topLeft);
    configureLabel (captureStatusLabel, 10.0f, theme::textDim);
    captureStatusLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (capturePanel);
    capturePanel.addAndMakeVisible (endpointBox);
    capturePanel.addAndMakeVisible (refreshEndpointsButton);
    capturePanel.addAndMakeVisible (captureToggleButton);
    capturePanel.addAndMakeVisible (settingsButton);
    capturePanel.addAndMakeVisible (captureWarningLabel);
    capturePanel.addAndMakeVisible (captureStatusLabel);
    capturePanel.addAndMakeVisible (loopbackMeter);

    // Pitch panel
    styleActionButton (minusFiveButton, theme::cyan);
    styleActionButton (plusFiveButton, theme::cyan);
    styleActionButton (resetPitchButton);
    minusFiveButton.onClick = [this]
    {
        const auto value = juce::jlimit (-12.0f, 12.0f, app.params.get (ParamId::transpose) - 5.0f);
        app.params.set (ParamId::transpose, value);
    };
    plusFiveButton.onClick = [this]
    {
        const auto value = juce::jlimit (-12.0f, 12.0f, app.params.get (ParamId::transpose) + 5.0f);
        app.params.set (ParamId::transpose, value);
    };
    resetPitchButton.onClick = [this] { app.params.set (ParamId::transpose, 0.0f); };

    configureLabel (keyDisplayLabel, 10.5f, theme::cyan, true);
    configureLabel (pitchStatusLabel, 9.5f, theme::textDim);
    pitchStatusLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (pitchPanel);
    pitchPanel.addAndMakeVisible (musicKeyCombo);
    pitchPanel.addAndMakeVisible (transposeSlider);
    pitchPanel.addAndMakeVisible (minusFiveButton);
    pitchPanel.addAndMakeVisible (plusFiveButton);
    pitchPanel.addAndMakeVisible (resetPitchButton);
    pitchPanel.addAndMakeVisible (pitchBypassToggle);
    pitchPanel.addAndMakeVisible (keyDisplayLabel);
    pitchPanel.addAndMakeVisible (pitchStatusLabel);
    pitchPanel.addAndMakeVisible (videoVolumeSlider);

    // Booster panel
    styleActionButton (autoBoostButton, theme::green);
    styleActionButton (autoBoostResetButton);
    styleActionButton (presetOriginalButton);
    styleActionButton (presetKaraokeButton);
    styleActionButton (presetWarmButton);
    styleActionButton (presetPunchyButton);
    styleActionButton (presetLoudButton);

    autoBoostButton.onClick = [this] { app.engine.requestAutoBoost(); };
    autoBoostResetButton.onClick = [this] { app.engine.resetAutoBoost(); };
    presetOriginalButton.onClick = [this] { loadBoosterPreset ("Original"); };
    presetKaraokeButton.onClick = [this] { loadBoosterPreset ("Karaoke Clear"); };
    presetWarmButton.onClick = [this] { loadBoosterPreset ("Warm Music"); };
    presetPunchyButton.onClick = [this] { loadBoosterPreset ("Punchy"); };
    presetLoudButton.onClick = [this] { loadBoosterPreset ("Loud but Safe"); };

    configureLabel (boosterMeterLabel, 9.5f, theme::textDim);
    boosterMeterLabel.setFont (theme::monoFont (9.5f));
    configureLabel (boosterStatusLabel, 9.5f, theme::textDim);
    boosterStatusLabel.setJustificationType (juce::Justification::topLeft);

    addAndMakeVisible (boosterPanel);
    boosterPanel.addAndMakeVisible (boosterInputTrim);
    boosterPanel.addAndMakeVisible (boosterBass);
    boosterPanel.addAndMakeVisible (boosterPresence);
    boosterPanel.addAndMakeVisible (boosterTreble);
    boosterPanel.addAndMakeVisible (boosterLoudness);
    boosterPanel.addAndMakeVisible (boosterOutput);
    boosterPanel.addAndMakeVisible (boosterBypassToggle);
    boosterPanel.addAndMakeVisible (autoLevelToggle);
    boosterPanel.addAndMakeVisible (autoBoostButton);
    boosterPanel.addAndMakeVisible (autoBoostResetButton);
    boosterPanel.addAndMakeVisible (presetOriginalButton);
    boosterPanel.addAndMakeVisible (presetKaraokeButton);
    boosterPanel.addAndMakeVisible (presetWarmButton);
    boosterPanel.addAndMakeVisible (presetPunchyButton);
    boosterPanel.addAndMakeVisible (presetLoudButton);
    boosterPanel.addAndMakeVisible (musicMeter);
    boosterPanel.addAndMakeVisible (boosterMeterLabel);
    boosterPanel.addAndMakeVisible (boosterStatusLabel);

    addAndMakeVisible (sideViewport);
    sideViewport.setViewedComponent (&sideContent, false);
    sideViewport.setScrollBarsShown (true, false);
    sideViewport.setScrollBarThickness (8);
    sideContent.addAndMakeVisible (capturePanel);
    sideContent.addAndMakeVisible (pitchPanel);
    sideContent.addAndMakeVisible (boosterPanel);

    configureLabel (captureWarningLabel, 9.5f, theme::amber);
    captureWarningLabel.setText ("Loopback menangkap seluruh suara pada endpoint terpilih, bukan hanya YouTube. "
                                 "Jika output master memakai ASIO/interface lain, audio YouTube tetap keluar "
                                 "lewat endpoint Windows default.",
                                 juce::dontSendNotification);

    refreshEndpoints();
    updateCaptureUi();
    startTimerHz (10);
}

YouTubePage::~YouTubePage()
{
    stopTimer();
}

void YouTubePage::paint (juce::Graphics&)
{
}

void YouTubePage::resized()
{
    auto area = getLocalBounds().reduced (14);
    auto sideWidth = juce::jlimit (330, 430, area.getWidth() * 36 / 100);

    sideViewport.setBounds (area.removeFromRight (sideWidth));
    area.removeFromRight (12);
    browserPanel.setBounds (area);

    auto browserArea = browserPanel.getContentBounds().reduced (8);
    auto toolbar = browserArea.removeFromTop (30);

    backButton.setBounds (toolbar.removeFromLeft (28));
    forwardButton.setBounds (toolbar.removeFromLeft (28));
    reloadButton.setBounds (toolbar.removeFromLeft (50));
    homeButton.setBounds (toolbar.removeFromLeft (46));
    showHideButton.setBounds (toolbar.removeFromRight (96));
    toolbar.removeFromRight (4);
    searchButton.setBounds (toolbar.removeFromRight (48));
    toolbar.removeFromRight (4);
    searchEditor.setBounds (toolbar.removeFromLeft (juce::jmax (120, toolbar.getWidth())));

    browserArea.removeFromTop (4);
    if (browserStatusLabel.isVisible())
        browserStatusLabel.setBounds (browserArea.removeFromTop (16));

    browserArea.removeFromTop (4);

    if (browserVisible)
    {
        browser.setBounds (browserArea);
    }
    else
    {
        browserHiddenLabel.setBounds (browserArea.reduced (20));
    }

    sideContent.setSize (juce::jmax (320, sideViewport.getMaximumVisibleWidth()), 902);
    layoutSideContent();
}

void YouTubePage::layoutSideContent()
{
    auto area = sideContent.getLocalBounds();
    capturePanel.setBounds (area.removeFromTop (244));
    area.removeFromTop (10);
    pitchPanel.setBounds (area.removeFromTop (218));
    area.removeFromTop (10);
    boosterPanel.setBounds (area.removeFromTop (430));

    {
        auto content = capturePanel.getContentBounds().reduced (10);
        auto endpointRow = content.removeFromTop (26);
        refreshEndpointsButton.setBounds (endpointRow.removeFromRight (70));
        endpointRow.removeFromRight (6);
        endpointBox.setBounds (endpointRow);

        content.removeFromTop (6);
        auto buttonRow = content.removeFromTop (28);
        captureToggleButton.setBounds (buttonRow.removeFromLeft (buttonRow.getWidth() * 58 / 100));
        buttonRow.removeFromLeft (6);
        settingsButton.setBounds (buttonRow);
        content.removeFromTop (6);
        captureWarningLabel.setBounds (content.removeFromTop (52));
        content.removeFromTop (4);
        loopbackMeter.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        captureStatusLabel.setBounds (content);
    }

    {
        auto content = pitchPanel.getContentBounds().reduced (10);
        auto topRow = content.removeFromTop (28);
        musicKeyCombo.setBounds (topRow.removeFromLeft (juce::jmax (180, topRow.getWidth() / 2)));
        content.removeFromTop (4);
        transposeSlider.setBounds (content.removeFromTop (30));
        content.removeFromTop (4);

        auto quickRow = content.removeFromTop (26);
        const auto third = quickRow.getWidth() / 3;
        minusFiveButton.setBounds (quickRow.removeFromLeft (third).reduced (2, 0));
        plusFiveButton.setBounds (quickRow.removeFromLeft (third).reduced (2, 0));
        resetPitchButton.setBounds (quickRow.reduced (2, 0));
        content.removeFromTop (4);

        auto bypassRow = content.removeFromTop (24);
        pitchBypassToggle.setBounds (bypassRow.removeFromLeft (160));
        videoVolumeSlider.setBounds (bypassRow);
        content.removeFromTop (4);
        keyDisplayLabel.setBounds (content.removeFromTop (18));
        pitchStatusLabel.setBounds (content);
    }

    {
        auto content = boosterPanel.getContentBounds().reduced (10);
        auto knobGrid = content.removeFromTop (150);
        const auto knobWidth = knobGrid.getWidth() / 3;
        auto row1 = knobGrid.removeFromTop (75);
        auto row2 = knobGrid.removeFromTop (75);

        boosterInputTrim.setBounds (row1.removeFromLeft (knobWidth));
        boosterBass.setBounds (row1.removeFromLeft (knobWidth));
        boosterPresence.setBounds (row1);
        boosterTreble.setBounds (row2.removeFromLeft (knobWidth));
        boosterLoudness.setBounds (row2.removeFromLeft (knobWidth));
        boosterOutput.setBounds (row2);

        content.removeFromTop (4);
        auto toggleRow = content.removeFromTop (24);
        boosterBypassToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2).reduced (2, 0));
        autoLevelToggle.setBounds (toggleRow.reduced (2, 0));
        content.removeFromTop (4);

        auto autoRow = content.removeFromTop (26);
        autoBoostButton.setBounds (autoRow.removeFromLeft (autoRow.getWidth() / 2).reduced (2, 0));
        autoBoostResetButton.setBounds (autoRow.reduced (2, 0));
        content.removeFromTop (4);

        musicMeter.setBounds (content.removeFromTop (18));
        boosterMeterLabel.setBounds (content.removeFromTop (16));
        content.removeFromTop (4);

        auto presetRow1 = content.removeFromTop (26);
        const auto presetThird = presetRow1.getWidth() / 3;
        presetOriginalButton.setBounds (presetRow1.removeFromLeft (presetThird).reduced (2, 0));
        presetKaraokeButton.setBounds (presetRow1.removeFromLeft (presetThird).reduced (2, 0));
        presetWarmButton.setBounds (presetRow1.reduced (2, 0));
        content.removeFromTop (4);
        auto presetRow2 = content.removeFromTop (26);
        presetPunchyButton.setBounds (presetRow2.removeFromLeft (presetRow2.getWidth() / 2).reduced (2, 0));
        presetLoudButton.setBounds (presetRow2.reduced (2, 0));
        content.removeFromTop (4);
        boosterStatusLabel.setBounds (content);
    }
}

void YouTubePage::timerCallback()
{
    const auto meters = app.engine.getMeters();

    loopbackMeter.setLevelDb (meters.loopbackPeakDb);
    musicMeter.setLevelDb (meters.musicBusPeakDb);
    musicMeter.setClip (meters.clip);

    keyDisplayLabel.setText (
        "Referensi: " + juce::String (music::keyNames[juce::jlimit (0, 11, app.params.getInt (ParamId::musicKey))])
            + "   ->   Sekarang: "
            + juce::String (music::keyNames[((app.params.getInt (ParamId::musicKey)
                                             + juce::roundToInt (app.params.get (ParamId::transpose))) % 12 + 12) % 12])
            + "   (" + (app.params.get (ParamId::transpose) >= 0.0f ? "+" : "")
            + juce::String (juce::roundToInt (app.params.get (ParamId::transpose))) + " st)",
        juce::dontSendNotification);

    if (app.params.getBool (ParamId::pitchBypass))
    {
        pitchStatusLabel.setText ("Bypass: phase vocoder dilewati. Kunci referensi hanya informasi manual; "
                                  "tidak ada deteksi kunci otomatis dari lagu.", juce::dontSendNotification);
    }
    else
    {
        pitchStatusLabel.setText ("Aktif: phase vocoder stereo menjaga tempo dan stereo. Latensi pitch "
                                  "dikompensasi pada alur musik; lihat total latency di Dashboard.", juce::dontSendNotification);
    }

    auto boosterMeter = "Peak " + juce::String (meters.musicBoosterPeakDb, 1) + " dB  |  GR comp "
                      + juce::String (meters.musicCompressorGrDb, 1) + " dB  |  GR limiter "
                      + juce::String (meters.musicLimiterGrDb, 1) + " dB";
    boosterMeterLabel.setText (boosterMeter, juce::dontSendNotification);

    juce::String autoState;

    switch (meters.autoBoostState)
    {
        case 1:  autoState = "Auto Boost sedang mengukur..."; break;
        case 2:  autoState = "Auto Boost diterapkan: " + juce::String (meters.autoBoostDb, 1) + " dB"; break;
        default: autoState = "Auto Boost siap (aman, maksimum dibatasi)."; break;
    }

    boosterStatusLabel.setText (autoState + "  Level auto: +"
                                   + juce::String (app.engine.getMusicBooster().getAutoLevelGainDb(), 1) + " dB",
                               juce::dontSendNotification);

    if (app.engine.getMusicBooster().getClipFlag())
        boosterStatusLabel.setText (boosterStatusLabel.getText() + "  |  CLIP musik terdeteksi",
                                    juce::dontSendNotification);

    updateCaptureUi();
    applyVideoVolume();
}

void YouTubePage::refreshEndpoints()
{
    const auto previous = app.engine.getLoopbackEndpointId();
    endpoints = app.engine.getLoopbackEndpoints();

    endpointBox.clear (juce::dontSendNotification);

    for (int i = 0; i < endpoints.size(); ++i)
    {
        auto name = endpoints[i].name;

        if (endpoints[i].isDefault)
            name << "  (default)";

        endpointBox.addItem (name, i + 1);
    }

    if (endpoints.isEmpty())
    {
        captureStatusLabel.setText ("Tidak ada endpoint output Windows yang aktif.", juce::dontSendNotification);
        captureToggleButton.setEnabled (false);
        return;
    }

    captureToggleButton.setEnabled (true);
    int selected = 0;

    for (int i = 0; i < endpoints.size(); ++i)
    {
        if (endpoints[i].id == previous)
        {
            selected = i;
            break;
        }
    }

    endpointBox.setSelectedItemIndex (selected, juce::dontSendNotification);

    if (previous.isEmpty())
        app.engine.setLoopbackEndpointId (endpoints[selected].id);
}

void YouTubePage::toggleCapture()
{
    if (app.engine.isLoopbackActive())
    {
        app.engine.stopLoopback();
        updateCaptureUi();
        return;
    }

    const auto index = endpointBox.getSelectedItemIndex();

    if (juce::isPositiveAndBelow (index, endpoints.size()))
        app.engine.setLoopbackEndpointId (endpoints[index].id);

    juce::String error;

    if (! app.engine.startLoopback (error))
        showWarningMessage ("Music capture", error);

    updateCaptureUi();
}

void YouTubePage::updateCaptureUi()
{
    const auto active = app.engine.isLoopbackActive();
    const auto capturing = app.engine.isLoopbackCapturing();

    captureToggleButton.setButtonText (active ? "Hentikan Capture" : "Mulai Capture");
    captureToggleButton.setColour (juce::TextButton::buttonColourId,
                                   active ? theme::magenta.withAlpha (0.22f) : theme::panelAlt);
    captureToggleButton.setEnabled (app.engine.isRunning() && ! endpoints.isEmpty());

    juce::String status;

    if (! app.engine.isRunning())
        status = "Engine audio belum berjalan. Buka Audio Settings.";
    else if (capturing)
    {
        auto& capture = app.engine.getLoopbackCapture();
        const auto targetRate = capture.getTargetSampleRate();
        const auto bufferMs = targetRate > 0.0
                                ? (double) capture.getAvailableSamples() * 1000.0 / targetRate
                                : 0.0;

        status = "Capture aktif: " + app.engine.getLoopbackEndpointName() + "\n"
               + juce::String (capture.getCaptureSampleRate() / 1000.0, 1)
               + " kHz -> " + juce::String (targetRate / 1000.0, 1) + " kHz"
               + "  |  buffer " + juce::String (juce::roundToInt (bufferMs)) + " ms"
               + "  |  terbuang " + juce::String (capture.getOverflowCount());
    }
    else if (active)
        status = "Menunggu capture dimulai...";
    else
    {
        status = "Capture nonaktif. ";
        const auto error = app.engine.getLoopbackError();

        if (error.isNotEmpty())
            status << "Error terakhir: " << error;
        else
            status << "Pilih endpoint lalu tekan Mulai Capture.";
    }

    captureStatusLabel.setText (status, juce::dontSendNotification);
    captureStatusLabel.setColour (juce::Label::textColourId,
                                  (! active && app.engine.getLoopbackError().isNotEmpty()) ? theme::red
                                                                                          : theme::textDim);
}

void YouTubePage::updateBrowserToolbar()
{
    showHideButton.setButtonText (browserVisible ? "Sembunyikan" : "Tampilkan");
}

void YouTubePage::applyVideoVolume()
{
    const auto volume = juce::jlimit (0, 100, app.params.getInt (ParamId::youtubeVideoVolume));

    if (volume == lastVideoVolume)
        return;

    lastVideoVolume = volume;
    browser.setVideoVolumePercent (volume);
}

void YouTubePage::loadBoosterPreset (const juce::String& name)
{
    juce::String error;
    juce::StringArray warnings;

    if (! app.presets.load (PresetManager::Category::Music, name, error, &warnings))
    {
        showWarningMessage ("Preset music", error);
        return;
    }

    if (! warnings.isEmpty())
        showWarningMessage ("Preset music", warnings.joinIntoString ("\n"));
}

} // namespace rr
