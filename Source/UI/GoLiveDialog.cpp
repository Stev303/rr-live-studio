#include "UI/GoLiveDialog.h"

namespace rr
{
namespace
{
    juce::String formatCheckDb (float db)
    {
        return db <= -99.0f ? juce::String::fromUTF8 ("\xe2\x80\x94")
                            : juce::String (db, 1) + " dB";
    }
}

//==============================================================================
class GoLiveDialog::CheckRow : public juce::Component
{
public:
    enum class State { ok, warning, fail };

    explicit CheckRow (juce::String rowTitle)
        : title (std::move (rowTitle))
    {
        setInterceptsMouseClicks (false, false);
    }

    void setState (State newState, juce::String newDetail)
    {
        if (state == newState && detail == newDetail)
            return;

        state = newState;
        detail = std::move (newDetail);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds();

        auto iconArea = area.removeFromLeft (24).withSizeKeepingCentre (16, 16);
        const auto colour = state == State::ok ? theme::green
                          : state == State::warning ? theme::amber
                                                    : theme::red;

        g.setColour (colour.withAlpha (0.16f));
        g.fillEllipse (iconArea.toFloat());
        g.setColour (colour);
        g.drawEllipse (iconArea.toFloat().reduced (0.5f), 1.2f);

        const auto icon = iconArea.toFloat().reduced (4.0f);

        if (state == State::ok)
        {
            juce::Path tick;
            tick.startNewSubPath (icon.getX(), icon.getCentreY());
            tick.lineTo (icon.getX() + icon.getWidth() * 0.38f, icon.getBottom());
            tick.lineTo (icon.getRight(), icon.getY());
            g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
        else if (state == State::warning)
        {
            g.fillRect (icon.getCentreX() - 0.9f, icon.getY() + 1.0f, 1.8f, icon.getHeight() * 0.55f);
            g.fillEllipse (icon.getCentreX() - 1.2f, icon.getBottom() - 2.6f, 2.4f, 2.4f);
        }
        else
        {
            g.drawLine (icon.getX(), icon.getY(), icon.getRight(), icon.getBottom(), 1.8f);
            g.drawLine (icon.getRight(), icon.getY(), icon.getX(), icon.getBottom(), 1.8f);
        }

        auto textArea = area.reduced (4, 0);

        if (detail.isNotEmpty())
        {
            g.setColour (theme::text);
            g.setFont (theme::font (11.5f, true));
            g.drawText (title, textArea.removeFromTop (juce::jmax (14, getHeight() / 2)),
                        juce::Justification::centredLeft, true);

            g.setColour (theme::textDim);
            g.setFont (theme::font (9.2f));
            g.drawText (detail, textArea, juce::Justification::centredLeft, true);
        }
        else
        {
            g.setColour (theme::text);
            g.setFont (theme::font (11.5f, true));
            g.drawText (title, textArea, juce::Justification::centredLeft, true);
        }
    }

private:
    juce::String title;
    juce::String detail;
    State state = State::warning;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CheckRow)
};

//==============================================================================
GoLiveDialog::GoLiveDialog (AppState& state, std::function<void()> onClose)
    : app (state),
      closeCallback (std::move (onClose)),
      testToneToggle (app.params, ParamId::testToneEnabled, "Test Tone 1 kHz"),
      testToneLevel (app.params, ParamId::testToneDb, juce::Slider::LinearHorizontal, true)
{
    static const char* const rowTitles[] =
    {
        "Audio device aktif",
        "Master limiter aktif",
        "Test tone nonaktif",
        "Tidak ada clip pada master",
        "YouTube Music capture",
        "Sinyal master terdeteksi"
    };

    for (int i = 0; i < (int) checkRows.size(); ++i)
        checkRows[(size_t) i] = std::make_unique<CheckRow> (rowTitles[i]);

    configureLabel (readyLabel, 12.0f, theme::green, true);
    readyLabel.setJustificationType (juce::Justification::topLeft);
    readyLabel.setMinimumHorizontalScale (0.75f);

    configureLabel (checklistHint, 9.3f, theme::textDim);
    checklistHint.setJustificationType (juce::Justification::topLeft);

    manualOutputToggle.setButtonText ("Saya sudah memilih output PC ini sebagai sumber audio di HP/BIGO.");
    manualHeadphoneToggle.setButtonText ("Mic HP/speaker dimatikan; monitoring memakai headphone.");
    manualOutputToggle.onClick = [this] { updateChecklist(); };

    configureLabel (outputDeviceLabel, 10.5f, theme::text, true);
    outputDeviceLabel.setJustificationType (juce::Justification::topLeft);
    outputDeviceLabel.setMinimumHorizontalScale (0.7f);
    configureLabel (outputFormatLabel, 10.0f, theme::textDim);
    configureLabel (outputLatencyLabel, 10.0f, theme::textDim);
    configureLabel (masterChainLabel, 10.0f, theme::textDim);
    configureLabel (peakLabel, 11.0f, theme::text);
    peakLabel.setFont (theme::monoFont (11.0f));
    peakLabel.setMinimumHorizontalScale (0.7f);

    configureLabel (testNoteLabel, 9.3f, theme::textDim);
    testNoteLabel.setJustificationType (juce::Justification::topLeft);
    testNoteLabel.setMinimumHorizontalScale (0.75f);
    testNoteLabel.setText ("Test tone melewati master VST dan limiter. Gunakan level rendah, dan matikan "
                           "sebelum live. Test tone dimatikan otomatis saat checklist ini ditutup.",
                           juce::dontSendNotification);

    styleActionButton (resetClipButton, theme::red);
    resetClipButton.onClick = [this] { resetClip(); };

    closeButton.setTooltip ("Tutup checklist");
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::textColourOffId, theme::red);
    closeButton.onClick = [this] { close(); };

    styleActionButton (settingsButton);
    settingsButton.onClick = [this]
    {
        close();
        app.navigateTo (8);
    };

    styleActionButton (youtubeButton, theme::magenta);
    youtubeButton.onClick = [this]
    {
        close();
        app.navigateTo (1);
    };

    styleActionButton (closeFooterButton, theme::cyan);
    closeFooterButton.onClick = [this] { close(); };

    configureLabel (guideLabel, 9.7f, theme::textDim);
    guideLabel.setJustificationType (juce::Justification::topLeft);
    guideLabel.setMinimumHorizontalScale (0.75f);
    guideLabel.setText ("- Aplikasi ini TIDAK terintegrasi otomatis dengan BIGO LIVE dan tidak mengontrol HP. "
                        "Aplikasi hanya menyiapkan output audio; pemilihan sumber audio dilakukan di HP/aplikasi BIGO.\n"
                        "- Kabel USB biasa tidak otomatis mengirim audio ke HP. Gunakan audio interface / mixer USB "
                        "atau virtual audio cable yang kompatibel, dan sambungkan sesuai dokumentasi perangkat.\n"
                        "- Set output Rr Live Studio ke perangkat tersebut di Settings, lalu pilih sumber audio eksternal "
                        "yang sama di BIGO. Uji level dengan test tone dan meter output akhir.\n"
                        "- Gunakan headphone untuk monitoring, matikan speaker dan mic HP untuk mencegah feedback. "
                        "Delay monitoring tergantung perangkat, buffer, dan jalur virtual cable.\n"
                        "- WASAPI loopback menangkap seluruh audio sistem pada endpoint terpilih, bukan hanya YouTube.",
                        juce::dontSendNotification);

    for (auto& row : checkRows)
        checklistPanel.addAndMakeVisible (*row);

    checklistPanel.addAndMakeVisible (readyLabel);
    checklistPanel.addAndMakeVisible (checklistHint);
    checklistPanel.addAndMakeVisible (manualOutputToggle);
    checklistPanel.addAndMakeVisible (manualHeadphoneToggle);

    outputPanel.addAndMakeVisible (outputDeviceLabel);
    outputPanel.addAndMakeVisible (outputFormatLabel);
    outputPanel.addAndMakeVisible (outputLatencyLabel);
    outputPanel.addAndMakeVisible (masterChainLabel);
    outputPanel.addAndMakeVisible (peakLabel);
    outputPanel.addAndMakeVisible (finalMeter);
    outputPanel.addAndMakeVisible (testToneToggle);
    outputPanel.addAndMakeVisible (testToneLevel);
    outputPanel.addAndMakeVisible (resetClipButton);
    outputPanel.addAndMakeVisible (testNoteLabel);

    guidePanel.addAndMakeVisible (guideLabel);

    addAndMakeVisible (checklistPanel);
    addAndMakeVisible (outputPanel);
    addAndMakeVisible (guidePanel);
    addAndMakeVisible (closeButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (youtubeButton);
    addAndMakeVisible (closeFooterButton);

    setWantsKeyboardFocus (true);
    setVisible (false);

    updateChecklist();
    updateOutputPanel();
}

GoLiveDialog::~GoLiveDialog()
{
    stopTimer();
}

void GoLiveDialog::open()
{
    if (auto* parent = getParentComponent())
        setBounds (parent->getLocalBounds());

    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    startTimerHz (10);
    updateChecklist();
    updateOutputPanel();
}

void GoLiveDialog::close()
{
    if (! isVisible())
        return;

    if (app.params.getBool (ParamId::testToneEnabled))
        app.params.setBool (ParamId::testToneEnabled, false);

    stopTimer();
    setVisible (false);

    if (closeCallback != nullptr)
        closeCallback();
}

void GoLiveDialog::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.78f));

    theme::drawPanel (g, dialogArea.toFloat(), theme::cyan.withAlpha (0.45f), 14.0f);

    auto inner = dialogArea.reduced (18);
    auto header = inner.removeFromTop (62);

    g.setColour (theme::cyan.withAlpha (0.9f));
    g.fillRoundedRectangle (header.getX() + 2.0f, header.getY() + 6.0f, 4.0f, 22.0f, 2.0f);

    g.setColour (theme::text);
    g.setFont (theme::font (20.0f, true));
    g.drawText ("GO LIVE CHECKLIST", header.withTrimmedLeft (14).withTrimmedRight (40).removeFromTop (26),
                juce::Justification::centredLeft, false);

    g.setColour (theme::textDim);
    g.setFont (theme::font (10.5f));
    g.drawText ("Verifikasi jalur audio lokal dan persiapan output ke HP / BIGO LIVE",
                header.withTrimmedLeft (14).withTrimmedRight (40).withTrimmedTop (24),
                juce::Justification::centredLeft, false);

    g.setColour (theme::border);
    g.fillRect (dialogArea.getX() + 18, header.getBottom() + 4, dialogArea.getWidth() - 36, 1);
}

void GoLiveDialog::resized()
{
    dialogArea = getLocalBounds().reduced (16).withSizeKeepingCentre (
        juce::jmin (960, juce::jmax (620, getWidth() - 60)),
        juce::jmin (710, juce::jmax (560, getHeight() - 60)));

    auto inner = dialogArea.reduced (18);
    auto header = inner.removeFromTop (62);
    closeButton.setBounds (header.removeFromRight (36).withSizeKeepingCentre (28, 26));

    auto footer = inner.removeFromBottom (44);
    inner.removeFromBottom (6);

    auto guideArea = inner.removeFromBottom (166);
    inner.removeFromBottom (8);

    auto leftColumn = inner.removeFromLeft (juce::jmax (300, (int) (inner.getWidth() * 0.54f)));
    inner.removeFromLeft (8);
    checklistPanel.setBounds (leftColumn);
    outputPanel.setBounds (inner);
    guidePanel.setBounds (guideArea);

    closeFooterButton.setBounds (footer.removeFromRight (104));
    footer.removeFromRight (8);
    youtubeButton.setBounds (footer.removeFromRight (128));
    footer.removeFromRight (8);
    settingsButton.setBounds (footer.removeFromRight (136));

    auto checklistArea = checklistPanel.getContentBounds();
    readyLabel.setBounds (checklistArea.removeFromTop (26));
    checklistArea.removeFromTop (2);

    for (auto& row : checkRows)
    {
        row->setBounds (checklistArea.removeFromTop (34));
        checklistArea.removeFromTop (2);
    }

    checklistArea.removeFromTop (2);
    checklistHint.setBounds (checklistArea.removeFromTop (16));
    checklistArea.removeFromTop (4);
    manualOutputToggle.setBounds (checklistArea.removeFromTop (26));
    manualHeadphoneToggle.setBounds (checklistArea.removeFromTop (26));

    auto outputArea = outputPanel.getContentBounds();
    outputDeviceLabel.setBounds (outputArea.removeFromTop (32));
    outputFormatLabel.setBounds (outputArea.removeFromTop (16));
    outputLatencyLabel.setBounds (outputArea.removeFromTop (16));
    masterChainLabel.setBounds (outputArea.removeFromTop (16));
    outputArea.removeFromTop (4);
    peakLabel.setBounds (outputArea.removeFromTop (18));
    finalMeter.setBounds (outputArea.removeFromTop (22));
    outputArea.removeFromTop (8);
    testToneToggle.setBounds (outputArea.removeFromTop (26));
    outputArea.removeFromTop (2);
    testToneLevel.setBounds (outputArea.removeFromTop (38));
    outputArea.removeFromTop (4);
    resetClipButton.setBounds (outputArea.removeFromTop (26));
    outputArea.removeFromTop (6);
    testNoteLabel.setBounds (outputArea);

    guideLabel.setBounds (guidePanel.getContentBounds());
}

bool GoLiveDialog::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }

    return false;
}

void GoLiveDialog::timerCallback()
{
    if (! isVisible())
        return;

    if (auto* parent = getParentComponent())
        if (getBounds() != parent->getLocalBounds())
            setBounds (parent->getLocalBounds());

    updateChecklist();
    updateOutputPanel();
}

void GoLiveDialog::updateChecklist()
{
    const auto meters = app.engine.getMeters();

    const bool deviceOk = app.engine.isRunning();
    checkRows[0]->setState (deviceOk ? CheckRow::State::ok : CheckRow::State::fail,
                            deviceOk ? app.engine.getDeviceDescription()
                                     : "Buka Settings dan pilih audio device, lalu pastikan device aktif.");

    const bool limiterOn = app.params.getBool (ParamId::masterLimiterEnabled);
    checkRows[1]->setState (limiterOn ? CheckRow::State::ok : CheckRow::State::warning,
                            limiterOn ? "Ceiling " + app.params.getDisplayValue (ParamId::masterLimiterCeilingDb)
                                      : "Aktifkan master limiter di halaman Effects atau Mixer.");

    const bool testToneOn = app.params.getBool (ParamId::testToneEnabled);
    checkRows[2]->setState (testToneOn ? CheckRow::State::warning : CheckRow::State::ok,
                            testToneOn ? "Test tone sedang aktif. Matikan sebelum Go Live."
                                       : "Test tone tidak aktif.");

    checkRows[3]->setState (meters.clip ? CheckRow::State::fail : CheckRow::State::ok,
                            meters.clip ? "Clip terdeteksi. Turunkan master gain / ceiling lalu reset clip."
                                        : "Tidak ada clip pada output.");

    const bool loopbackOk = app.engine.isLoopbackActive() && app.engine.isLoopbackCapturing();
    checkRows[4]->setState (loopbackOk ? CheckRow::State::ok : CheckRow::State::warning,
                            loopbackOk ? "Capture: " + app.engine.getLoopbackEndpointName()
                                       : "Opsional. Aktifkan loopback di halaman YouTube/Settings bila musik YouTube diperlukan.");

    const bool hasSignal = meters.masterPeakDb > -60.0f;
    checkRows[5]->setState (hasSignal ? CheckRow::State::ok : CheckRow::State::warning,
                            hasSignal ? "Terdeteksi " + formatCheckDb (meters.masterPeakDb)
                                      : "Belum ada sinyal. Putar musik atau bicara untuk menguji level.");

    const bool manualConfirmed = manualOutputToggle.getToggleState();
    const bool ready = deviceOk && limiterOn && ! testToneOn && ! meters.clip && manualConfirmed;

    readyLabel.setText (ready
                            ? "Siap untuk Go Live - pastikan pemilihan sumber audio di HP/BIGO sudah benar."
                            : "Belum siap - selesaikan item merah/kuning dan centang konfirmasi output.",
                        juce::dontSendNotification);
    readyLabel.setColour (juce::Label::textColourId, ready ? theme::green : theme::amber);

    manualHeadphoneToggle.setTooltip ("Direkomendasikan untuk mencegah feedback; tidak menghalangi checklist "
                                      "karena monitoring tiap perangkat berbeda.");
}

void GoLiveDialog::updateOutputPanel()
{
    const auto meters = app.engine.getMeters();
    const auto deviceText = "Output: " + app.engine.getDeviceDescription();
    outputDeviceLabel.setText (deviceText, juce::dontSendNotification);
    outputDeviceLabel.setTooltip (deviceText);

    outputFormatLabel.setText (app.engine.isRunning()
                                   ? "Sample rate " + juce::String (app.engine.getCurrentSampleRate(), 0)
                                         + " Hz  -  buffer " + juce::String (app.engine.getCurrentBlockSize())
                                         + " samples"
                                   : "Audio device tidak aktif",
                               juce::dontSendNotification);

    outputLatencyLabel.setText ("Latency total " + juce::String (app.engine.getTotalLatencyMs(), 1) + " ms",
                                juce::dontSendNotification);

    masterChainLabel.setText ("Master " + app.params.getDisplayValue (ParamId::masterGainDb)
                                  + "  -  limiter "
                                  + (app.params.getBool (ParamId::masterLimiterEnabled) ? "ON" : "OFF")
                                  + "  -  ceiling " + app.params.getDisplayValue (ParamId::masterLimiterCeilingDb),
                              juce::dontSendNotification);

    peakLabel.setText ("Peak " + formatCheckDb (meters.masterPeakDb)
                           + "  /  RMS " + formatCheckDb (meters.masterRmsDb)
                           + (meters.clip ? "  -  CLIP" : ""),
                       juce::dontSendNotification);
    peakLabel.setColour (juce::Label::textColourId, meters.clip ? theme::red : theme::text);

    finalMeter.setLevelDb (meters.masterPeakDb, meters.masterPeakDb);
    finalMeter.setClip (meters.clip);
}

void GoLiveDialog::resetClip()
{
    app.engine.resetClipIndicators();
    app.engine.getVocalChain().clearClipFlag();
    app.engine.getMusicBooster().clearClipFlag();
    finalMeter.setClip (false);
    updateChecklist();
    updateOutputPanel();
}

} // namespace rr
