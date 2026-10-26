#include "UI/Pages/EffectsPage.h"
#include "Utilities/MusicTheory.h"

namespace rr
{
EffectsPage::EffectsPage (AppState& state)
    : app (state),
      transposeSlider (app.params, ParamId::transpose, juce::Slider::LinearHorizontal, true),
      musicKeyCombo (app.params, ParamId::musicKey,
                     [] { juce::StringArray keys; for (auto* name : music::keyNames) keys.add (name); return keys; }(),
                     "Kunci Referensi"),
      pitchBypassToggle (app.params, ParamId::pitchBypass),
      videoVolumeSlider (app.params, ParamId::youtubeVideoVolume, juce::Slider::LinearHorizontal, true),
      balanceToggle (app.params, ParamId::sbEnabled),
      balanceTargetSlider (app.params, ParamId::sbTargetDb, juce::Slider::LinearHorizontal, true),
      balanceDuckSlider (app.params, ParamId::sbDuckDb, juce::Slider::LinearHorizontal, true),
      balanceAttackSlider (app.params, ParamId::sbAttackMs, juce::Slider::LinearHorizontal, true),
      balanceReleaseSlider (app.params, ParamId::sbReleaseMs, juce::Slider::LinearHorizontal, true),
      balanceStrengthSlider (app.params, ParamId::sbStrength, juce::Slider::LinearHorizontal, true),
      balanceMaxSlider (app.params, ParamId::sbMaxReductionDb, juce::Slider::LinearHorizontal, true),
      masterGainSlider (app.params, ParamId::masterGainDb, juce::Slider::LinearHorizontal, true),
      masterLimiterToggle (app.params, ParamId::masterLimiterEnabled),
      masterCeilingSlider (app.params, ParamId::masterLimiterCeilingDb, juce::Slider::LinearHorizontal, true),
      testToneToggle (app.params, ParamId::testToneEnabled),
      testToneLevelSlider (app.params, ParamId::testToneDb, juce::Slider::LinearHorizontal, true),
      boosterInputTrim (app.params, ParamId::musicInputTrimDb, juce::Slider::Rotary, true),
      boosterBass (app.params, ParamId::musicBassDb, juce::Slider::Rotary, true),
      boosterPresence (app.params, ParamId::musicPresenceDb, juce::Slider::Rotary, true),
      boosterTreble (app.params, ParamId::musicTrebleDb, juce::Slider::Rotary, true),
      boosterLoudness (app.params, ParamId::musicLoudness, juce::Slider::Rotary, true),
      boosterOutput (app.params, ParamId::boosterOutputGainDb, juce::Slider::Rotary, true),
      boosterCompThreshold (app.params, ParamId::boosterCompThresholdDb, juce::Slider::Rotary, true),
      boosterCompMakeup (app.params, ParamId::boosterCompMakeupDb, juce::Slider::Rotary, true),
      boosterCompRatio (app.params, ParamId::boosterCompRatio, juce::Slider::LinearHorizontal, true),
      boosterCompAttack (app.params, ParamId::boosterCompAttackMs, juce::Slider::LinearHorizontal, true),
      boosterCompRelease (app.params, ParamId::boosterCompReleaseMs, juce::Slider::LinearHorizontal, true),
      boosterCeiling (app.params, ParamId::boosterLimiterCeilingDb, juce::Slider::LinearHorizontal, true),
      boosterBypassToggle (app.params, ParamId::boosterBypass),
      autoLevelToggle (app.params, ParamId::boosterAutoLevel)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (9);

    content.addAndMakeVisible (pitchPanel);
    content.addAndMakeVisible (balancePanel);
    content.addAndMakeVisible (masterPanel);
    content.addAndMakeVisible (boosterPanel);

    styleActionButton (minusFiveButton, theme::cyan);
    styleActionButton (plusFiveButton, theme::cyan);
    styleActionButton (resetPitchButton);
    minusFiveButton.onClick = [this]
    {
        app.params.set (ParamId::transpose,
                        juce::jlimit (-12.0f, 12.0f, app.params.get (ParamId::transpose) - 5.0f));
    };
    plusFiveButton.onClick = [this]
    {
        app.params.set (ParamId::transpose,
                        juce::jlimit (-12.0f, 12.0f, app.params.get (ParamId::transpose) + 5.0f));
    };
    resetPitchButton.onClick = [this] { app.params.set (ParamId::transpose, 0.0f); };

    juce::Component* pitchComponents[] = { &transposeSlider, &musicKeyCombo, &pitchBypassToggle,
                                           &minusFiveButton, &plusFiveButton, &resetPitchButton,
                                           &videoVolumeSlider };

    for (auto* component : pitchComponents)
        pitchPanel.addAndMakeVisible (*component);

    configureLabel (keyDisplayLabel, 10.5f, theme::cyan, true);
    configureLabel (pitchStatusLabel, 9.5f, theme::textDim);
    pitchStatusLabel.setJustificationType (juce::Justification::topLeft);
    pitchPanel.addAndMakeVisible (keyDisplayLabel);
    pitchPanel.addAndMakeVisible (pitchStatusLabel);

    juce::Component* balanceComponents[] = { &balanceToggle, &balanceTargetSlider,
                                             &balanceDuckSlider, &balanceAttackSlider,
                                             &balanceReleaseSlider, &balanceStrengthSlider,
                                             &balanceMaxSlider, &balanceMeter };

    for (auto* component : balanceComponents)
        balancePanel.addAndMakeVisible (*component);

    configureLabel (balanceStatusLabel, 9.5f, theme::textDim);
    balanceStatusLabel.setJustificationType (juce::Justification::topLeft);
    balancePanel.addAndMakeVisible (balanceStatusLabel);

    juce::Component* masterComponents[] = { &masterGainSlider, &masterLimiterToggle,
                                            &masterCeilingSlider, &testToneToggle,
                                            &testToneLevelSlider, &masterMeter,
                                            &resetMasterClipButton };

    for (auto* component : masterComponents)
        masterPanel.addAndMakeVisible (*component);

    resetMasterClipButton.onClick = [this]
    {
        app.engine.resetClipIndicators();
        app.engine.getMusicBooster().clearClipFlag();
    };
    styleActionButton (resetMasterClipButton, theme::amber);

    configureLabel (masterStatusLabel, 9.5f, theme::textDim);
    masterStatusLabel.setJustificationType (juce::Justification::topLeft);
    masterPanel.addAndMakeVisible (masterStatusLabel);

    juce::Component* boosterComponents[] = { &boosterInputTrim, &boosterBass, &boosterPresence,
                                             &boosterTreble, &boosterLoudness, &boosterOutput,
                                             &boosterCompThreshold, &boosterCompMakeup,
                                             &boosterCompRatio, &boosterCompAttack,
                                             &boosterCompRelease, &boosterCeiling,
                                             &boosterBypassToggle, &autoLevelToggle,
                                             &autoBoostButton, &autoBoostResetButton,
                                             &boosterMeter };

    for (auto* component : boosterComponents)
        boosterPanel.addAndMakeVisible (*component);

    styleActionButton (autoBoostButton, theme::green);
    styleActionButton (autoBoostResetButton);
    autoBoostButton.onClick = [this] { app.engine.requestAutoBoost(); };
    autoBoostResetButton.onClick = [this] { app.engine.resetAutoBoost(); };

    const juce::StringArray presetNames { "Original", "Karaoke Clear", "Warm Music",
                                          "Punchy", "Loud but Safe" };

    for (int i = 0; i < (int) presetButtons.size(); ++i)
    {
        auto button = std::make_unique<juce::TextButton> (presetNames[i]);
        styleActionButton (*button, theme::magenta);
        button->onClick = [this, name = presetNames[i]] { loadMusicPreset (name); };
        boosterPanel.addAndMakeVisible (*button);
        presetButtons[(size_t) i] = std::move (button);
    }

    configureLabel (boosterMeterLabel, 9.5f, theme::textDim);
    boosterMeterLabel.setFont (theme::monoFont (9.5f));
    configureLabel (boosterStatusLabel, 9.5f, theme::textDim);
    boosterStatusLabel.setJustificationType (juce::Justification::topLeft);
    boosterPanel.addAndMakeVisible (boosterMeterLabel);
    boosterPanel.addAndMakeVisible (boosterStatusLabel);

    startTimerHz (10);
}

EffectsPage::~EffectsPage()
{
    stopTimer();
}

void EffectsPage::paint (juce::Graphics&)
{
}

void EffectsPage::resized()
{
    auto area = getLocalBounds().reduced (14);
    viewport.setBounds (area);
    content.setSize (juce::jmax (900, viewport.getMaximumVisibleWidth()), 790);
    layoutContent();
}

void EffectsPage::layoutContent()
{
    auto area = content.getLocalBounds();
    auto top = area.removeFromTop (360);
    pitchPanel.setBounds (top.removeFromLeft (top.getWidth() * 29 / 100));
    top.removeFromLeft (10);
    balancePanel.setBounds (top.removeFromLeft (top.getWidth() * 46 / 100));
    top.removeFromLeft (10);
    masterPanel.setBounds (top);
    area.removeFromTop (12);
    boosterPanel.setBounds (area.removeFromTop (400));

    {
        auto inner = pitchPanel.getContentBounds().reduced (10, 6);
        pitchBypassToggle.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (2);
        musicKeyCombo.setBounds (inner.removeFromTop (30));
        transposeSlider.setBounds (inner.removeFromTop (30));
        inner.removeFromTop (2);

        auto quick = inner.removeFromTop (26);
        const auto third = quick.getWidth() / 3;
        minusFiveButton.setBounds (quick.removeFromLeft (third).reduced (2, 0));
        plusFiveButton.setBounds (quick.removeFromLeft (third).reduced (2, 0));
        resetPitchButton.setBounds (quick.reduced (2, 0));
        inner.removeFromTop (4);
        videoVolumeSlider.setBounds (inner.removeFromTop (30));
        inner.removeFromTop (4);
        keyDisplayLabel.setBounds (inner.removeFromTop (18));
        pitchStatusLabel.setBounds (inner);
    }

    {
        auto inner = balancePanel.getContentBounds().reduced (10, 6);
        balanceToggle.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (2);
        balanceTargetSlider.setBounds (inner.removeFromTop (28));
        balanceDuckSlider.setBounds (inner.removeFromTop (28));
        balanceAttackSlider.setBounds (inner.removeFromTop (28));
        balanceReleaseSlider.setBounds (inner.removeFromTop (28));
        balanceStrengthSlider.setBounds (inner.removeFromTop (28));
        balanceMaxSlider.setBounds (inner.removeFromTop (28));
        inner.removeFromTop (2);
        balanceMeter.setBounds (inner.removeFromTop (18));
        inner.removeFromTop (2);
        balanceStatusLabel.setBounds (inner);
    }

    {
        auto inner = masterPanel.getContentBounds().reduced (10, 6);
        masterGainSlider.setBounds (inner.removeFromTop (30));
        inner.removeFromTop (2);
        auto limiterRow = inner.removeFromTop (28);
        masterLimiterToggle.setBounds (limiterRow.removeFromLeft (150));
        masterCeilingSlider.setBounds (limiterRow);
        inner.removeFromTop (2);

        auto testRow = inner.removeFromTop (24);
        testToneToggle.setBounds (testRow.removeFromLeft (testRow.getWidth() / 2));
        resetMasterClipButton.setBounds (testRow.reduced (2, 0));
        testToneLevelSlider.setBounds (inner.removeFromTop (30));
        inner.removeFromTop (2);
        masterMeter.setBounds (inner.removeFromTop (18));
        inner.removeFromTop (2);
        masterStatusLabel.setBounds (inner);
    }

    {
        auto inner = boosterPanel.getContentBounds().reduced (10, 6);
        auto topArea = inner.removeFromTop (170);
        auto knobs = topArea.removeFromLeft (topArea.getWidth() * 56 / 100);
        topArea.removeFromLeft (12);
        auto right = topArea;

        const auto knobWidth = knobs.getWidth() / 4;
        auto row1 = knobs.removeFromTop (85);
        auto row2 = knobs.removeFromTop (85);
        boosterInputTrim.setBounds (row1.removeFromLeft (knobWidth));
        boosterBass.setBounds (row1.removeFromLeft (knobWidth));
        boosterPresence.setBounds (row1.removeFromLeft (knobWidth));
        boosterTreble.setBounds (row1);
        boosterLoudness.setBounds (row2.removeFromLeft (knobWidth));
        boosterOutput.setBounds (row2.removeFromLeft (knobWidth));
        boosterCompThreshold.setBounds (row2.removeFromLeft (knobWidth));
        boosterCompMakeup.setBounds (row2);

        boosterCompRatio.setBounds (right.removeFromTop (28));
        boosterCompAttack.setBounds (right.removeFromTop (28));
        boosterCompRelease.setBounds (right.removeFromTop (28));
        boosterCeiling.setBounds (right.removeFromTop (28));
        right.removeFromTop (2);
        auto toggleRow = right.removeFromTop (24);
        boosterBypassToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2).reduced (2, 0));
        autoLevelToggle.setBounds (toggleRow.reduced (2, 0));
        right.removeFromTop (2);
        auto autoRow = right.removeFromTop (26);
        autoBoostButton.setBounds (autoRow.removeFromLeft (autoRow.getWidth() / 2).reduced (2, 0));
        autoBoostResetButton.setBounds (autoRow.reduced (2, 0));

        inner.removeFromTop (4);
        boosterMeter.setBounds (inner.removeFromTop (18));
        boosterMeterLabel.setBounds (inner.removeFromTop (16));
        inner.removeFromTop (4);

        auto presets = inner.removeFromTop (28);
        const auto presetWidth = presets.getWidth() / 5;

        for (auto& button : presetButtons)
            if (button != nullptr)
                button->setBounds (presets.removeFromLeft (presetWidth).reduced (2, 0));

        inner.removeFromTop (4);
        boosterStatusLabel.setBounds (inner);
    }
}

void EffectsPage::timerCallback()
{
    const auto meters = app.engine.getMeters();

    const auto key = juce::jlimit (0, 11, app.params.getInt (ParamId::musicKey));
    const auto shift = juce::roundToInt (app.params.get (ParamId::transpose));
    const auto currentKey = ((key + shift) % 12 + 12) % 12;
    keyDisplayLabel.setText ("Referensi: " + juce::String (music::keyNames[key])
                                 + "   ->   Sekarang: " + juce::String (music::keyNames[currentKey])
                                 + "   (" + (shift >= 0 ? "+" : "") + juce::String (shift) + " st)",
                             juce::dontSendNotification);

    pitchStatusLabel.setText (
        (app.params.getBool (ParamId::pitchBypass)
             ? juce::String ("Bypass. Musik melewati pitch shifter.")
             : juce::String ("Aktif: stereo phase vocoder, tempo dipertahankan, gain smoothing."))
            + "\nKunci referensi diatur manual; aplikasi tidak mendeteksi kunci lagu secara otomatis.",
        juce::dontSendNotification);

    balanceMeter.setLevelDb (meters.musicBusPeakDb);
    balanceStatusLabel.setText (
        juce::String (app.params.getBool (ParamId::sbEnabled) ? "Aktif. " : "Nonaktif. ")
            + "Duck musik sekarang " + juce::String (meters.smartBalanceGrDb, 1) + " dB"
            + "  |  prioritas vokal " + app.params.getDisplayValue (ParamId::sbTargetDb)
            + "\nHanya mengatur bus musik terhadap vokal, sebelum master. Tidak menaikkan gain master.",
        juce::dontSendNotification);

    masterMeter.setLevelDb (meters.masterPeakDb);
    masterMeter.setClip (meters.clip);
    masterStatusLabel.setText (
        "Peak " + juce::String (meters.masterPeakDb, 1) + " dB  |  RMS "
            + juce::String (meters.masterRmsDb, 1) + " dB  |  GR limiter "
            + juce::String (meters.masterLimiterGrDb, 1) + " dB"
            + (meters.clip ? "  |  CLIP" : "")
            + "\nLatensi total " + juce::String (app.engine.getTotalLatencyMs(), 1)
            + " ms (device + engine). Test tone 1 kHz untuk cek output.",
        juce::dontSendNotification);
    masterStatusLabel.setColour (juce::Label::textColourId, meters.clip ? theme::red : theme::textDim);

    boosterMeter.setLevelDb (meters.musicBusPeakDb);
    boosterMeter.setClip (app.engine.getMusicBooster().getClipFlag());
    boosterMeterLabel.setText ("Peak " + juce::String (meters.musicBoosterPeakDb, 1)
                                   + " dB  |  GR comp " + juce::String (meters.musicCompressorGrDb, 1)
                                   + " dB  |  GR limiter " + juce::String (meters.musicLimiterGrDb, 1) + " dB",
                               juce::dontSendNotification);

    juce::String autoState;

    switch (meters.autoBoostState)
    {
        case 1:  autoState = "mengukur"; break;
        case 2:  autoState = "diterapkan " + juce::String (meters.autoBoostDb, 1) + " dB"; break;
        default: autoState = "siap"; break;
    }

    boosterStatusLabel.setText ("Auto Boost: " + autoState
                                    + "  |  auto level +" + juce::String (app.engine.getMusicBooster().getAutoLevelGainDb(), 1)
                                    + " dB  |  batas keamanan limiter aktif.",
                                juce::dontSendNotification);
}

void EffectsPage::loadMusicPreset (const juce::String& name)
{
    juce::String error;
    juce::StringArray warnings;

    if (! app.presets.load (PresetManager::Category::Music, name, error, &warnings))
        showWarningMessage ("Preset music", error);
    else if (! warnings.isEmpty())
        showWarningMessage ("Preset music", warnings.joinIntoString ("\n"));
}

} // namespace rr
