#include "UI/Pages/MixerPage.h"

namespace rr
{
namespace
{
    juce::String formatDb (float db)
    {
        if (db <= -99.0f)
            return juce::String::fromUTF8 ("\xe2\x80\x94");

        return juce::String (db, 1) + " dB";
    }
}

MixerPage::MixerPage (AppState& state)
    : app (state),
      micFader (app.params, ParamId::micMixDb, juce::Slider::LinearVertical, true),
      micInputGain (app.params, ParamId::micGainDb, juce::Slider::LinearHorizontal, true),
      micMute (app.params, ParamId::micMute, "Mute"),
      micSolo (app.params, ParamId::micSolo, "Solo"),
      musicFader (app.params, ParamId::musicMixDb, juce::Slider::LinearVertical, true),
      musicMute (app.params, ParamId::musicMute, "Mute"),
      musicSolo (app.params, ParamId::musicSolo, "Solo"),
      samplerFader (app.params, ParamId::samplerMixDb, juce::Slider::LinearVertical, true),
      samplerMute (app.params, ParamId::samplerMute, "Mute"),
      samplerSolo (app.params, ParamId::samplerSolo, "Solo"),
      vocalFader (app.params, ParamId::vocalFaderDb, juce::Slider::LinearVertical, true),
      reverbToggle (app.params, ParamId::reverbEnabled, "Reverb"),
      reverbMixSlider (app.params, ParamId::reverbMix, juce::Slider::LinearHorizontal, true),
      delayToggle (app.params, ParamId::delayEnabled, "Delay"),
      delayMixSlider (app.params, ParamId::delayMix, juce::Slider::LinearHorizontal, true),
      masterFader (app.params, ParamId::masterGainDb, juce::Slider::LinearVertical, true),
      masterLimiterToggle (app.params, ParamId::masterLimiterEnabled, "Limiter"),
      masterCeilingSlider (app.params, ParamId::masterLimiterCeilingDb,
                           juce::Slider::LinearHorizontal, true),
      routingDiagram (app, false)
{
    configureLabel (micStatus, 10.5f, theme::textDim);
    configureLabel (musicStatus, 10.5f, theme::textDim);
    configureLabel (samplerStatus, 10.5f, theme::textDim);
    configureLabel (vocalStatus, 10.5f, theme::textDim);
    configureLabel (masterStatus, 10.5f, theme::textDim);
    masterStatus.setFont (theme::monoFont (10.0f));
    masterStatus.setMinimumHorizontalScale (0.7f);

    configureLabel (musicCaptureStatus, 10.0f, theme::textDim);
    musicCaptureStatus.setJustificationType (juce::Justification::topLeft);
    musicCaptureStatus.setMinimumHorizontalScale (0.7f);
    musicCaptureStatus.setTooltip ("WASAPI loopback menangkap seluruh audio sistem pada endpoint "
                                   "terpilih, bukan hanya aplikasi YouTube.");

    styleActionButton (samplerPageButton, theme::amber);
    samplerPageButton.onClick = [this] { app.navigateTo (5); };

    styleActionButton (resetClipButton, theme::red);
    resetClipButton.onClick = [this] { resetClipAndPeak(); };

    micStatus.setTooltip ("Level bus vokal setelah fader Mic, mute, dan solo. Fader berada setelah "
                          "vocal chain dan VST vokal.");
    musicStatus.setTooltip ("Level bus YouTube Music setelah fader Music, mute, dan solo.");
    samplerStatus.setTooltip ("Level bus Sampler setelah fader Sampler, mute, dan solo.");
    vocalStatus.setTooltip ("Vocal FX adalah bagian dari bus Mic; mute/solo vokal dikendalikan di strip Mic.");
    masterStatus.setTooltip ("Level master setelah master gain, VST master, dan limiter. "
                             "Mute/solo master terpisah tidak tersedia di engine.");

    micPanel.addAndMakeVisible (micFader);
    micPanel.addAndMakeVisible (micInputGain);
    micPanel.addAndMakeVisible (micMute);
    micPanel.addAndMakeVisible (micSolo);
    micPanel.addAndMakeVisible (micMeter);
    micPanel.addAndMakeVisible (micStatus);
    content.addAndMakeVisible (micPanel);

    musicPanel.addAndMakeVisible (musicFader);
    musicPanel.addAndMakeVisible (musicMute);
    musicPanel.addAndMakeVisible (musicSolo);
    musicPanel.addAndMakeVisible (musicMeter);
    musicPanel.addAndMakeVisible (musicStatus);
    musicPanel.addAndMakeVisible (musicCaptureStatus);
    content.addAndMakeVisible (musicPanel);

    samplerPanel.addAndMakeVisible (samplerFader);
    samplerPanel.addAndMakeVisible (samplerMute);
    samplerPanel.addAndMakeVisible (samplerSolo);
    samplerPanel.addAndMakeVisible (samplerMeter);
    samplerPanel.addAndMakeVisible (samplerStatus);
    samplerPanel.addAndMakeVisible (samplerPageButton);
    content.addAndMakeVisible (samplerPanel);

    vocalPanel.addAndMakeVisible (vocalFader);
    vocalPanel.addAndMakeVisible (vocalMeter);
    vocalPanel.addAndMakeVisible (reverbToggle);
    vocalPanel.addAndMakeVisible (reverbMixSlider);
    vocalPanel.addAndMakeVisible (delayToggle);
    vocalPanel.addAndMakeVisible (delayMixSlider);
    vocalPanel.addAndMakeVisible (vocalStatus);
    content.addAndMakeVisible (vocalPanel);

    masterPanel.addAndMakeVisible (masterFader);
    masterPanel.addAndMakeVisible (masterMeter);
    masterPanel.addAndMakeVisible (masterLimiterToggle);
    masterPanel.addAndMakeVisible (masterCeilingSlider);
    masterPanel.addAndMakeVisible (resetClipButton);
    masterPanel.addAndMakeVisible (masterStatus);
    content.addAndMakeVisible (masterPanel);

    routingPanel.setAccentColour (theme::green);
    routingPanel.addAndMakeVisible (routingDiagram);
    content.addAndMakeVisible (routingPanel);

    configureLabel (noteLabel, 10.0f, theme::textDim.withAlpha (0.85f));
    noteLabel.setJustificationType (juce::Justification::topLeft);
    noteLabel.setText ("Catatan: pan dan FX send belum diimplementasikan di engine, jadi tidak ada kontrol palsu "
                       "di halaman ini. Solo hanya berlaku untuk bus Mic, Music, dan Sampler. Vocal FX melekat "
                       "pada bus vokal; mute dan solo-nya ada di strip Mic. Master tidak memiliki mute/solo terpisah.",
                       juce::dontSendNotification);
    content.addAndMakeVisible (noteLabel);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollBarThickness (10);

    startTimerHz (20);
}

MixerPage::~MixerPage()
{
    stopTimer();
}

void MixerPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void MixerPage::resized()
{
    viewport.setBounds (getLocalBounds());

    const int width = juce::jmax (940, viewport.getMaximumVisibleWidth());
    constexpr int stripHeight = 470;
    constexpr int routingHeight = 220;
    constexpr int noteHeight = 44;
    constexpr int gap = 8;
    constexpr int margin = 6;
    const int contentHeight = margin + stripHeight + gap + routingHeight + 6 + noteHeight + margin;

    content.setSize (width, juce::jmax (contentHeight, viewport.getHeight()));

    const int stripsWidth = width - margin * 2;
    const int stripWidth = (stripsWidth - gap * 4) / 5;

    auto layoutStrip = [] (Panel& panel, LevelMeter& meter, ParamSlider& fader,
                           juce::Component* muteButton, juce::Component* soloButton,
                           juce::Label& status) -> juce::Rectangle<int>
    {
        auto area = panel.getContentBounds();
        status.setBounds (area.removeFromTop (16));
        area.removeFromTop (4);

        auto faderArea = area.removeFromTop (226);
        auto meterArea = faderArea.removeFromLeft (20);
        meter.setBounds (meterArea.reduced (2, 4));
        fader.setBounds (faderArea);

        area.removeFromTop (2);
        auto toggleRow = area.removeFromTop (24);

        if (muteButton != nullptr || soloButton != nullptr)
        {
            const int half = (toggleRow.getWidth() - 4) / 2;

            if (muteButton != nullptr)
                muteButton->setBounds (toggleRow.removeFromLeft (half));

            if (soloButton != nullptr)
            {
                toggleRow.removeFromLeft (4);
                soloButton->setBounds (toggleRow);
            }
        }

        area.removeFromTop (6);
        return area;
    };

    const int top = margin;

    micPanel.setBounds (margin, top, stripWidth, stripHeight);
    musicPanel.setBounds (margin + stripWidth + gap, top, stripWidth, stripHeight);
    samplerPanel.setBounds (margin + (stripWidth + gap) * 2, top, stripWidth, stripHeight);
    vocalPanel.setBounds (margin + (stripWidth + gap) * 3, top, stripWidth, stripHeight);
    masterPanel.setBounds (margin + (stripWidth + gap) * 4, top, stripWidth, stripHeight);

    auto micExtra = layoutStrip (micPanel, micMeter, micFader, &micMute, &micSolo, micStatus);
    micInputGain.setBounds (micExtra.removeFromTop (42));

    auto musicExtra = layoutStrip (musicPanel, musicMeter, musicFader, &musicMute, &musicSolo, musicStatus);
    musicCaptureStatus.setBounds (musicExtra.removeFromTop (44));

    auto samplerExtra = layoutStrip (samplerPanel, samplerMeter, samplerFader,
                                    &samplerMute, &samplerSolo, samplerStatus);
    samplerPageButton.setBounds (samplerExtra.removeFromTop (26));

    auto vocalExtra = layoutStrip (vocalPanel, vocalMeter, vocalFader, nullptr, nullptr, vocalStatus);
    auto reverbRow = vocalExtra.removeFromTop (24);
    reverbToggle.setBounds (reverbRow);
    vocalExtra.removeFromTop (2);
    reverbMixSlider.setBounds (vocalExtra.removeFromTop (38));
    vocalExtra.removeFromTop (2);
    auto delayRow = vocalExtra.removeFromTop (24);
    delayToggle.setBounds (delayRow);
    vocalExtra.removeFromTop (2);
    delayMixSlider.setBounds (vocalExtra.removeFromTop (38));

    auto masterExtra = layoutStrip (masterPanel, masterMeter, masterFader,
                                    nullptr, nullptr, masterStatus);
    auto limiterRow = masterExtra.removeFromTop (24);
    masterLimiterToggle.setBounds (limiterRow);
    masterExtra.removeFromTop (2);
    masterCeilingSlider.setBounds (masterExtra.removeFromTop (38));
    masterExtra.removeFromTop (2);
    resetClipButton.setBounds (masterExtra.removeFromTop (24));

    routingPanel.setBounds (margin, top + stripHeight + gap, stripsWidth, routingHeight);
    routingDiagram.setBounds (routingPanel.getContentBounds());

    noteLabel.setBounds (margin, top + stripHeight + gap + routingHeight + 6,
                         stripsWidth, noteHeight);
}

void MixerPage::timerCallback()
{
    const auto meters = app.engine.getMeters();

    auto updateMeter = [] (LevelMeter& meter, float levelDb)
    {
        meter.setLevelDb (levelDb, levelDb);
    };

    updateMeter (micMeter, meters.vocalBusPeakDb);
    updateMeter (musicMeter, meters.musicBusPeakDb);
    updateMeter (samplerMeter, meters.samplerPeakDb);
    updateMeter (vocalMeter, meters.vocalChainPeakDb);
    updateMeter (masterMeter, meters.masterPeakDb);
    masterMeter.setClip (meters.clip);

    micStatus.setText ("Fader " + app.params.getDisplayValue (ParamId::micMixDb)
                           + juce::String::fromUTF8 ("  \xc2\xb7  ") + "Input "
                           + app.params.getDisplayValue (ParamId::micGainDb),
                       juce::dontSendNotification);

    musicStatus.setText ("Fader " + app.params.getDisplayValue (ParamId::musicMixDb)
                             + juce::String::fromUTF8 ("  \xc2\xb7  ")
                             + (app.params.getBool (ParamId::musicSolo) ? "SOLO" : "bus"),
                         juce::dontSendNotification);

    samplerStatus.setText ("Fader " + app.params.getDisplayValue (ParamId::samplerMixDb)
                               + juce::String::fromUTF8 ("  \xc2\xb7  ")
                               + "Aktif " + juce::String (app.sampler.getActiveVoiceCount())
                               + "/10",
                           juce::dontSendNotification);

    vocalStatus.setText ("Fader " + app.params.getDisplayValue (ParamId::vocalFaderDb)
                             + juce::String::fromUTF8 ("  \xc2\xb7  ") + "output chain",
                         juce::dontSendNotification);

    juce::String masterText = "Peak " + formatDb (meters.masterPeakDb)
                            + " / RMS " + formatDb (meters.masterRmsDb);

    if (meters.clip)
        masterText << "  CLIP";

    masterStatus.setText (masterText, juce::dontSendNotification);
    masterStatus.setColour (juce::Label::textColourId,
                            meters.clip ? theme::red : theme::textDim);

    juce::String captureText;

    if (! app.engine.isLoopbackActive())
        captureText = "Loopback belum aktif. Aktifkan di halaman YouTube.";
    else if (app.engine.isLoopbackCapturing())
        captureText = "Loopback aktif: " + app.engine.getLoopbackEndpointName();
    else
        captureText = "Loopback diminta, menunggu device audio.";

    musicCaptureStatus.setText (captureText, juce::dontSendNotification);
}

void MixerPage::resetClipAndPeak()
{
    app.engine.resetClipIndicators();
    app.engine.getVocalChain().clearClipFlag();
    app.engine.getMusicBooster().clearClipFlag();
    masterMeter.setClip (false);
    masterStatus.setColour (juce::Label::textColourId, theme::textDim);
}

} // namespace rr
