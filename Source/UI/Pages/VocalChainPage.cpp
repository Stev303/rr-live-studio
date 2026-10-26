#include "UI/Pages/VocalChainPage.h"
#include "Utilities/MusicTheory.h"

namespace rr
{
namespace
{
    juce::StringArray keyItems()
    {
        juce::StringArray items;

        for (auto* name : music::keyNames)
            items.add (name);

        return items;
    }

    juce::StringArray scaleItems()
    {
        juce::StringArray items;

        for (auto* name : music::scaleNames)
            items.add (name);

        return items;
    }

    juce::StringArray intervalItems()
    {
        juce::StringArray items;

        for (auto* name : music::intervalNames)
            items.add (name);

        return items;
    }

    void addLabelTo (juce::Component& parent, juce::Label& label, const juce::String& text,
                     float height, juce::Colour colour = theme::textDim, bool bold = false)
    {
        configureLabel (label, height, colour, bold);
        label.setText (text, juce::dontSendNotification);
        parent.addAndMakeVisible (label);
    }
}

VocalChainPage::VocalChainPage (AppState& state)
    : app (state),
      micGainSlider (app.params, ParamId::micGainDb, juce::Slider::LinearHorizontal, true),
      gateThresholdSlider (app.params, ParamId::gateThresholdDb, juce::Slider::LinearHorizontal, true),
      hpfSlider (app.params, ParamId::hpfHz, juce::Slider::LinearHorizontal, true),
      gateToggle (app.params, ParamId::gateEnabled),
      hpfToggle (app.params, ParamId::hpfEnabled),
      micMuteToggle (app.params, ParamId::micMute, "Mute Mic"),
      deEsserToggle (app.params, ParamId::deEsserEnabled),
      compToggle (app.params, ParamId::compEnabled),
      deEsserThresholdSlider (app.params, ParamId::deEsserThresholdDb, juce::Slider::LinearHorizontal, true),
      deEsserFreqSlider (app.params, ParamId::deEsserFreqHz, juce::Slider::LinearHorizontal, true),
      compThresholdSlider (app.params, ParamId::compThresholdDb, juce::Slider::LinearHorizontal, true),
      compRatioSlider (app.params, ParamId::compRatio, juce::Slider::LinearHorizontal, true),
      compAttackSlider (app.params, ParamId::compAttackMs, juce::Slider::LinearHorizontal, true),
      compReleaseSlider (app.params, ParamId::compReleaseMs, juce::Slider::LinearHorizontal, true),
      compMakeupSlider (app.params, ParamId::compMakeupDb, juce::Slider::LinearHorizontal, true),
      correctionToggle (app.params, ParamId::pitchCorrectionEnabled),
      pcKeyCombo (app.params, ParamId::pcKey, keyItems(), "Key"),
      pcScaleCombo (app.params, ParamId::pcScale, scaleItems(), "Scale"),
      pcRetuneSlider (app.params, ParamId::pcRetuneMs, juce::Slider::LinearHorizontal, true),
      pcAmountSlider (app.params, ParamId::pcAmount, juce::Slider::LinearHorizontal, true),
      eqDisplay (app.params, app.engine.getVocalChain().getSmartEQ()),
      harmonyToggle (app.params, ParamId::harmonyEnabled),
      harmonyBypassToggle (app.params, ParamId::harmonyBypass),
      hKeyCombo (app.params, ParamId::hKey, keyItems(), "Key"),
      hScaleCombo (app.params, ParamId::hScale, scaleItems(), "Scale"),
      voice1Toggle (app.params, ParamId::hVoice1On, "Voice 1"),
      voice2Toggle (app.params, ParamId::hVoice2On, "Voice 2"),
      voice1IntervalCombo (app.params, ParamId::hVoice1Interval, intervalItems(), "Interval 1"),
      voice2IntervalCombo (app.params, ParamId::hVoice2Interval, intervalItems(), "Interval 2"),
      voice1CustomSlider (app.params, ParamId::hVoice1Custom, juce::Slider::LinearHorizontal, true),
      voice2CustomSlider (app.params, ParamId::hVoice2Custom, juce::Slider::LinearHorizontal, true),
      harmonyLevelSlider (app.params, ParamId::harmonyLevel, juce::Slider::LinearHorizontal, true),
      harmonyDryWetSlider (app.params, ParamId::harmonyDryWet, juce::Slider::LinearHorizontal, true),
      humanizeSlider (app.params, ParamId::humanize, juce::Slider::LinearHorizontal, true),
      harmonyRetuneSlider (app.params, ParamId::retuneSpeedMs, juce::Slider::LinearHorizontal, true),
      trackingSlider (app.params, ParamId::trackingStrength, juce::Slider::LinearHorizontal, true),
      stereoWidthSlider (app.params, ParamId::stereoWidth, juce::Slider::LinearHorizontal, true),
      formantToggle (app.params, ParamId::formantPreserve, "Formant Preserve"),
      satToggle (app.params, ParamId::satEnabled),
      satDriveSlider (app.params, ParamId::satDrive, juce::Slider::LinearHorizontal, true),
      satMixSlider (app.params, ParamId::satMix, juce::Slider::LinearHorizontal, true),
      reverbToggle (app.params, ParamId::reverbEnabled),
      reverbMixSlider (app.params, ParamId::reverbMix, juce::Slider::LinearHorizontal, true),
      reverbSizeSlider (app.params, ParamId::reverbSize, juce::Slider::LinearHorizontal, true),
      reverbDampSlider (app.params, ParamId::reverbDamp, juce::Slider::LinearHorizontal, true),
      delayToggle (app.params, ParamId::delayEnabled),
      delayTimeSlider (app.params, ParamId::delayTimeMs, juce::Slider::LinearHorizontal, true),
      delayFeedbackSlider (app.params, ParamId::delayFeedback, juce::Slider::LinearHorizontal, true),
      delayMixSlider (app.params, ParamId::delayMix, juce::Slider::LinearHorizontal, true),
      vocalFaderSlider (app.params, ParamId::vocalFaderDb, juce::Slider::LinearHorizontal, true)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (9);

    content.addAndMakeVisible (presetPanel);
    content.addAndMakeVisible (inputPanel);
    content.addAndMakeVisible (correctionPanel);
    content.addAndMakeVisible (dynamicsPanel);
    content.addAndMakeVisible (eqPanel);
    content.addAndMakeVisible (harmonyPanel);
    content.addAndMakeVisible (characterPanel);

    // Presets
    const auto presetNames = PresetManager::getFactoryPresetNames (PresetManager::Category::Vocal);

    for (int i = 0; i < (int) presetButtons.size(); ++i)
    {
        const auto name = presetNames[i];
        auto button = std::make_unique<juce::TextButton> (name);
        styleActionButton (*button, theme::cyan);
        button->setTooltip ("Muat preset vokal " + name);
        button->onClick = [this, name] { loadVocalPreset (name); };
        presetPanel.addAndMakeVisible (*button);
        presetButtons[(size_t) i] = std::move (button);
    }

    addLabelTo (presetPanel, presetStatusLabel, "Preset bawaan; simpan versi sendiri lewat halaman Presets.",
                10.0f, theme::textDim);

    // Input
    inputPanel.addAndMakeVisible (micGainSlider);
    inputPanel.addAndMakeVisible (gateThresholdSlider);
    inputPanel.addAndMakeVisible (hpfSlider);
    inputPanel.addAndMakeVisible (gateToggle);
    inputPanel.addAndMakeVisible (hpfToggle);
    inputPanel.addAndMakeVisible (micMuteToggle);
    inputPanel.addAndMakeVisible (inputMeter);
    addLabelTo (inputPanel, inputMeterLabel, "Input -100 dB", 9.5f, theme::textDim);
    inputMeterLabel.setFont (theme::monoFont (9.5f));
    addLabelTo (inputPanel, gateStatusLabel, "Gate: menutup", 9.5f, theme::amber);
    addLabelTo (inputPanel, chainOrderLabel,
                "Urutan nyata: Input Gain > Gate > HPF > Smart EQ > De-Esser > Compressor > "
                "Koreksi Nada > Harmonizer > Saturasi > Reverb > Delay > Vocal Fader",
                9.0f, theme::textDim);
    chainOrderLabel.setJustificationType (juce::Justification::topLeft);

    // Dynamics
    dynamicsPanel.addAndMakeVisible (deEsserToggle);
    dynamicsPanel.addAndMakeVisible (compToggle);
    dynamicsPanel.addAndMakeVisible (deEsserThresholdSlider);
    dynamicsPanel.addAndMakeVisible (deEsserFreqSlider);
    dynamicsPanel.addAndMakeVisible (compThresholdSlider);
    dynamicsPanel.addAndMakeVisible (compRatioSlider);
    dynamicsPanel.addAndMakeVisible (compAttackSlider);
    dynamicsPanel.addAndMakeVisible (compReleaseSlider);
    dynamicsPanel.addAndMakeVisible (compMakeupSlider);
    addLabelTo (dynamicsPanel, dynamicsMeterLabel, "GR de-esser 0.0 dB | GR compressor 0.0 dB",
                9.5f, theme::textDim);
    dynamicsMeterLabel.setFont (theme::monoFont (9.5f));

    // Correction
    correctionPanel.addAndMakeVisible (correctionToggle);
    correctionPanel.addAndMakeVisible (pcKeyCombo);
    correctionPanel.addAndMakeVisible (pcScaleCombo);
    correctionPanel.addAndMakeVisible (pcRetuneSlider);
    correctionPanel.addAndMakeVisible (pcAmountSlider);
    addLabelTo (correctionPanel, detectionLabel, "Nada: belum terdeteksi", 9.5f, theme::textDim);
    detectionLabel.setFont (theme::monoFont (9.5f));
    addLabelTo (correctionPanel, correctionStatusLabel,
                "Koreksi nada memakai deteksi pitch pada audio mikrofon, bukan audio YouTube.",
                9.0f, theme::textDim);
    correctionStatusLabel.setJustificationType (juce::Justification::topLeft);

    // Smart EQ
    eqPanel.addAndMakeVisible (eqDisplay);
    for (auto* button : { &eqAnalyseButton, &eqApplyButton, &eqUndoButton, &eqResetButton })
    {
        styleActionButton (*button, theme::cyan);
        eqPanel.addAndMakeVisible (*button);
    }

    eqAnalyseButton.onClick = [this] { analyseEq(); };
    eqApplyButton.onClick = [this] { applyEq(); };
    eqUndoButton.onClick = [this] { undoEq(); };
    eqResetButton.onClick = [this] { resetEq(); };
    eqApplyButton.setEnabled (false);

    addLabelTo (eqPanel, eqStatusLabel,
                "Tekan Analisa Spektrum saat mikrofon menerima suara. Analisis berjalan di message thread.",
                9.5f, theme::textDim);
    eqStatusLabel.setJustificationType (juce::Justification::topLeft);

    const std::array<ParamId, 5> freqIds {
        ParamId::eq1Freq, ParamId::eq2Freq, ParamId::eq3Freq, ParamId::eq4Freq, ParamId::eq5Freq
    };
    const std::array<ParamId, 5> gainIds {
        ParamId::eq1Gain, ParamId::eq2Gain, ParamId::eq3Gain, ParamId::eq4Gain, ParamId::eq5Gain
    };
    const std::array<ParamId, 5> qIds {
        ParamId::eq1Q, ParamId::eq2Q, ParamId::eq3Q, ParamId::eq4Q, ParamId::eq5Q
    };

    for (int i = 0; i < 5; ++i)
    {
        auto frequency = std::make_unique<ParamSlider> (app.params, freqIds[(size_t) i],
                                                        juce::Slider::Rotary, true);
        auto gain = std::make_unique<ParamSlider> (app.params, gainIds[(size_t) i],
                                                   juce::Slider::Rotary, true);
        auto q = std::make_unique<ParamSlider> (app.params, qIds[(size_t) i],
                                                juce::Slider::Rotary, true);
        frequency->setAccentColour (theme::cyan);
        gain->setAccentColour (theme::amber);
        q->setAccentColour (theme::textDim);
        eqPanel.addAndMakeVisible (*frequency);
        eqPanel.addAndMakeVisible (*gain);
        eqPanel.addAndMakeVisible (*q);
        eqFreqSliders[(size_t) i] = std::move (frequency);
        eqGainSliders[(size_t) i] = std::move (gain);
        eqQSliders[(size_t) i] = std::move (q);
    }

    // Harmonizer
    harmonyPanel.addAndMakeVisible (harmonyToggle);
    harmonyPanel.addAndMakeVisible (harmonyBypassToggle);
    harmonyPanel.addAndMakeVisible (hKeyCombo);
    harmonyPanel.addAndMakeVisible (hScaleCombo);
    harmonyPanel.addAndMakeVisible (voice1Toggle);
    harmonyPanel.addAndMakeVisible (voice2Toggle);
    harmonyPanel.addAndMakeVisible (voice1IntervalCombo);
    harmonyPanel.addAndMakeVisible (voice2IntervalCombo);
    harmonyPanel.addAndMakeVisible (voice1CustomSlider);
    harmonyPanel.addAndMakeVisible (voice2CustomSlider);
    harmonyPanel.addAndMakeVisible (harmonyLevelSlider);
    harmonyPanel.addAndMakeVisible (harmonyDryWetSlider);
    harmonyPanel.addAndMakeVisible (humanizeSlider);
    harmonyPanel.addAndMakeVisible (harmonyRetuneSlider);
    harmonyPanel.addAndMakeVisible (trackingSlider);
    harmonyPanel.addAndMakeVisible (stereoWidthSlider);
    harmonyPanel.addAndMakeVisible (formantToggle);
    harmonyPanel.addAndMakeVisible (harmonyAnalyseButton);
    harmonyPanel.addAndMakeVisible (harmonyApplyButton);
    harmonyPanel.addAndMakeVisible (harmonyCancelButton);
    styleActionButton (harmonyAnalyseButton, theme::magenta);
    styleActionButton (harmonyApplyButton, theme::green);
    styleActionButton (harmonyCancelButton);
    harmonyApplyButton.setEnabled (false);
    harmonyCancelButton.setEnabled (false);

    harmonyAnalyseButton.onClick = [this] { startHarmonyAnalysis(); };
    harmonyApplyButton.onClick = [this] { applyHarmonySuggestion(); };
    harmonyCancelButton.onClick = [this] { cancelHarmonyAnalysis(); };

    addLabelTo (harmonyPanel, harmonySuggestionLabel,
                "Belum ada analisis. Tekan Analisa Nada dan nyanyikan frasa pendek yang stabil.",
                9.5f, theme::textDim);
    harmonySuggestionLabel.setJustificationType (juce::Justification::topLeft);
    addLabelTo (harmonyPanel, harmonyStatusLabel,
                "Saran harmony dibuat lokal dari histogram pitch vokal (rule-based, bukan model AI terlatih).",
                9.0f, theme::textDim);
    harmonyStatusLabel.setJustificationType (juce::Justification::topLeft);

    // Character
    characterPanel.addAndMakeVisible (satToggle);
    characterPanel.addAndMakeVisible (satDriveSlider);
    characterPanel.addAndMakeVisible (satMixSlider);
    characterPanel.addAndMakeVisible (reverbToggle);
    characterPanel.addAndMakeVisible (reverbMixSlider);
    characterPanel.addAndMakeVisible (reverbSizeSlider);
    characterPanel.addAndMakeVisible (reverbDampSlider);
    characterPanel.addAndMakeVisible (delayToggle);
    characterPanel.addAndMakeVisible (delayTimeSlider);
    characterPanel.addAndMakeVisible (delayFeedbackSlider);
    characterPanel.addAndMakeVisible (delayMixSlider);
    characterPanel.addAndMakeVisible (vocalFaderSlider);
    characterPanel.addAndMakeVisible (outputMeter);
    addLabelTo (characterPanel, masterVocalMeterLabel, "Vocal bus -100 dB", 9.5f, theme::textDim);
    masterVocalMeterLabel.setFont (theme::monoFont (9.5f));

    startTimerHz (12);
}

VocalChainPage::~VocalChainPage()
{
    stopTimer();
}

void VocalChainPage::paint (juce::Graphics&)
{
}

void VocalChainPage::resized()
{
    auto area = getLocalBounds().reduced (14);
    viewport.setBounds (area);
    content.setSize (juce::jmax (880, viewport.getMaximumVisibleWidth()), 1690);
    layoutContent();
}

void VocalChainPage::layoutContent()
{
    auto area = content.getLocalBounds();

    presetPanel.setBounds (area.removeFromTop (78));
    area.removeFromTop (12);

    auto topRow = area.removeFromTop (310);
    inputPanel.setBounds (topRow.removeFromLeft (topRow.getWidth() * 28 / 100));
    topRow.removeFromLeft (10);
    correctionPanel.setBounds (topRow.removeFromLeft (topRow.getWidth() * 38 / 100));
    topRow.removeFromLeft (10);
    dynamicsPanel.setBounds (topRow);

    area.removeFromTop (12);
    eqPanel.setBounds (area.removeFromTop (480));
    area.removeFromTop (12);
    harmonyPanel.setBounds (area.removeFromTop (470));
    area.removeFromTop (12);
    characterPanel.setBounds (area.removeFromTop (juce::jmin (300, area.getHeight())));

    {
        auto contentArea = presetPanel.getContentBounds().reduced (8, 4);
        auto row = contentArea.removeFromTop (30);
        const auto width = row.getWidth() / (int) presetButtons.size();

        for (auto& button : presetButtons)
            if (button != nullptr)
                button->setBounds (row.removeFromLeft (width).reduced (2, 0));

        presetStatusLabel.setBounds (contentArea.removeFromTop (16));
    }

    {
        auto contentArea = inputPanel.getContentBounds().reduced (10, 6);
        micGainSlider.setBounds (contentArea.removeFromTop (30));
        contentArea.removeFromTop (2);

        auto toggleRow = contentArea.removeFromTop (24);
        gateToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 3).reduced (1, 0));
        hpfToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2).reduced (1, 0));
        micMuteToggle.setBounds (toggleRow.reduced (1, 0));
        contentArea.removeFromTop (2);

        gateThresholdSlider.setBounds (contentArea.removeFromTop (30));
        hpfSlider.setBounds (contentArea.removeFromTop (30));
        contentArea.removeFromTop (2);
        inputMeter.setBounds (contentArea.removeFromTop (18));
        inputMeterLabel.setBounds (contentArea.removeFromTop (15));
        gateStatusLabel.setBounds (contentArea.removeFromTop (15));
        contentArea.removeFromTop (2);
        chainOrderLabel.setBounds (contentArea);
    }

    {
        auto contentArea = correctionPanel.getContentBounds().reduced (10, 6);
        correctionToggle.setBounds (contentArea.removeFromTop (24));
        contentArea.removeFromTop (2);
        pcKeyCombo.setBounds (contentArea.removeFromTop (30));
        pcScaleCombo.setBounds (contentArea.removeFromTop (30));
        pcRetuneSlider.setBounds (contentArea.removeFromTop (30));
        pcAmountSlider.setBounds (contentArea.removeFromTop (30));
        contentArea.removeFromTop (2);
        detectionLabel.setBounds (contentArea.removeFromTop (16));
        correctionStatusLabel.setBounds (contentArea);
    }

    {
        auto contentArea = dynamicsPanel.getContentBounds().reduced (10, 6);
        auto toggleRow = contentArea.removeFromTop (24);
        deEsserToggle.setBounds (toggleRow.removeFromLeft (toggleRow.getWidth() / 2).reduced (1, 0));
        compToggle.setBounds (toggleRow.reduced (1, 0));
        contentArea.removeFromTop (2);

        for (auto* slider : { &deEsserThresholdSlider, &deEsserFreqSlider, &compThresholdSlider,
                              &compRatioSlider, &compAttackSlider, &compReleaseSlider, &compMakeupSlider })
        {
            slider->setBounds (contentArea.removeFromTop (28));
        }

        contentArea.removeFromTop (2);
        dynamicsMeterLabel.setBounds (contentArea.removeFromTop (16));
    }

    {
        auto contentArea = eqPanel.getContentBounds().reduced (10, 6);
        auto toolbar = contentArea.removeFromTop (28);
        eqAnalyseButton.setBounds (toolbar.removeFromLeft (128));
        toolbar.removeFromLeft (6);
        eqApplyButton.setBounds (toolbar.removeFromLeft (82));
        toolbar.removeFromLeft (6);
        eqUndoButton.setBounds (toolbar.removeFromLeft (70));
        toolbar.removeFromLeft (6);
        eqResetButton.setBounds (toolbar.removeFromLeft (82));
        contentArea.removeFromTop (3);
        eqStatusLabel.setBounds (contentArea.removeFromTop (18));
        contentArea.removeFromTop (3);
        eqDisplay.setBounds (contentArea.removeFromTop (205));
        contentArea.removeFromTop (4);

        const auto columnWidth = contentArea.getWidth() / 5;

        for (int i = 0; i < 5; ++i)
        {
            auto column = contentArea.removeFromLeft (columnWidth);
            eqFreqSliders[(size_t) i]->setBounds (column.removeFromTop (56).reduced (1, 0));
            eqGainSliders[(size_t) i]->setBounds (column.removeFromTop (56).reduced (1, 0));
            eqQSliders[(size_t) i]->setBounds (column.removeFromTop (56).reduced (1, 0));
        }
    }

    {
        auto contentArea = harmonyPanel.getContentBounds().reduced (10, 6);
        auto mainRow = contentArea.removeFromTop (28);
        harmonyToggle.setBounds (mainRow.removeFromLeft (130).reduced (1, 0));
        harmonyBypassToggle.setBounds (mainRow.removeFromLeft (120).reduced (1, 0));
        mainRow.removeFromLeft (8);
        hKeyCombo.setBounds (mainRow.removeFromLeft (juce::jmin (200, mainRow.getWidth() / 2)));
        hScaleCombo.setBounds (mainRow);
        contentArea.removeFromTop (2);

        auto voice1Row = contentArea.removeFromTop (28);
        voice1Toggle.setBounds (voice1Row.removeFromLeft (92).reduced (1, 0));
        voice1IntervalCombo.setBounds (voice1Row.removeFromLeft (180));
        voice1Row.removeFromLeft (8);
        voice1CustomSlider.setBounds (voice1Row);
        contentArea.removeFromTop (2);

        auto voice2Row = contentArea.removeFromTop (28);
        voice2Toggle.setBounds (voice2Row.removeFromLeft (92).reduced (1, 0));
        voice2IntervalCombo.setBounds (voice2Row.removeFromLeft (180));
        voice2Row.removeFromLeft (8);
        voice2CustomSlider.setBounds (voice2Row);
        contentArea.removeFromTop (4);

        auto grid = contentArea.removeFromTop (114);
        auto left = grid.removeFromLeft (grid.getWidth() / 2);
        auto right = grid;

        harmonyLevelSlider.setBounds (left.removeFromTop (28));
        harmonyDryWetSlider.setBounds (left.removeFromTop (28));
        humanizeSlider.setBounds (left.removeFromTop (28));
        harmonyRetuneSlider.setBounds (left.removeFromTop (28));
        trackingSlider.setBounds (right.removeFromTop (28));
        stereoWidthSlider.setBounds (right.removeFromTop (28));
        contentArea.removeFromTop (2);
        formantToggle.setBounds (contentArea.removeFromTop (24).removeFromLeft (180));
        contentArea.removeFromTop (4);

        auto analysisRow = contentArea.removeFromTop (28);
        harmonyAnalyseButton.setBounds (analysisRow.removeFromLeft (120));
        analysisRow.removeFromLeft (6);
        harmonyApplyButton.setBounds (analysisRow.removeFromLeft (120));
        analysisRow.removeFromLeft (6);
        harmonyCancelButton.setBounds (analysisRow.removeFromLeft (84));
        contentArea.removeFromTop (4);
        harmonySuggestionLabel.setBounds (contentArea.removeFromTop (38));
        harmonyStatusLabel.setBounds (contentArea);
    }

    {
        auto contentArea = characterPanel.getContentBounds().reduced (10, 6);
        auto left = contentArea.removeFromLeft (contentArea.getWidth() / 2);
        contentArea.removeFromLeft (12);

        satToggle.setBounds (left.removeFromTop (24).removeFromLeft (150));
        satDriveSlider.setBounds (left.removeFromTop (28));
        satMixSlider.setBounds (left.removeFromTop (28));
        left.removeFromTop (2);
        reverbToggle.setBounds (left.removeFromTop (24).removeFromLeft (150));
        reverbMixSlider.setBounds (left.removeFromTop (28));
        reverbSizeSlider.setBounds (left.removeFromTop (28));
        reverbDampSlider.setBounds (left.removeFromTop (28));

        delayToggle.setBounds (contentArea.removeFromTop (24).removeFromLeft (150));
        delayTimeSlider.setBounds (contentArea.removeFromTop (28));
        delayFeedbackSlider.setBounds (contentArea.removeFromTop (28));
        delayMixSlider.setBounds (contentArea.removeFromTop (28));
        vocalFaderSlider.setBounds (contentArea.removeFromTop (30));
        outputMeter.setBounds (contentArea.removeFromTop (18));
        masterVocalMeterLabel.setBounds (contentArea.removeFromTop (16));
    }
}

void VocalChainPage::timerCallback()
{
    const auto meters = app.engine.getMeters();
    auto& vocalChain = app.engine.getVocalChain();

    inputMeter.setLevelDb (meters.micPeakDb);
    outputMeter.setLevelDb (meters.vocalBusPeakDb);
    inputMeter.setClip (meters.clip);
    outputMeter.setClip (meters.clip);

    inputMeterLabel.setText ("Input " + juce::String (meters.micPeakDb, 1)
                                 + " dB  |  Chain out " + juce::String (meters.vocalChainPeakDb, 1) + " dB",
                             juce::dontSendNotification);

    gateStatusLabel.setText (vocalChain.getGateOpen() ? "Gate: terbuka" : "Gate: menutup",
                             juce::dontSendNotification);
    gateStatusLabel.setColour (juce::Label::textColourId,
                               vocalChain.getGateOpen() ? theme::green : theme::amber);

    dynamicsMeterLabel.setText ("GR de-esser " + juce::String (meters.vocalDeEsserGrDb, 1)
                                    + " dB | GR compressor " + juce::String (meters.vocalCompressorGrDb, 1)
                                    + " dB | out " + juce::String (meters.vocalChainPeakDb, 1) + " dB",
                                juce::dontSendNotification);

    if (vocalChain.isPitchVoiced())
    {
        const auto midi = juce::roundToInt (vocalChain.getDetectedMidiNote());
        detectionLabel.setText ("Nada: " + music::noteNameForMidi (midi)
                                    + "  |  " + juce::String (vocalChain.getDetectedFrequencyHz(), 1) + " Hz"
                                    + "  |  keyakinan " + juce::String (juce::roundToInt (vocalChain.getPitchConfidence() * 100.0f)) + "%",
                                juce::dontSendNotification);
        detectionLabel.setColour (juce::Label::textColourId, theme::green);
    }
    else
    {
        detectionLabel.setText ("Nada: belum ada suara stabil", juce::dontSendNotification);
        detectionLabel.setColour (juce::Label::textColourId, theme::textDim);
    }

    if (app.params.getBool (ParamId::pitchCorrectionEnabled))
    {
        correctionStatusLabel.setText (
            "Aktif " + app.params.getDisplayValue (ParamId::pcAmount) + "  |  retune "
                + app.params.getDisplayValue (ParamId::pcRetuneMs)
                + "  |  koreksi sekarang " + juce::String (vocalChain.getAppliedCorrectionSemitones(), 2) + " st\n"
                + "Key/scale: " + music::keyAndScaleName (app.params.getInt (ParamId::pcKey),
                                                          app.params.getInt (ParamId::pcScale)),
            juce::dontSendNotification);
    }
    else
    {
        correctionStatusLabel.setText ("Bypass: deteksi nada tetap berjalan untuk harmonizer.", juce::dontSendNotification);
    }

    auto& smartEq = vocalChain.getSmartEQ();

    if (! hasEqAnalysis)
    {
        eqStatusLabel.setText ("Frame analisis: " + juce::String (smartEq.getAnalysisFrameCount())
                                   + (smartEq.hasEnoughData() ? " (cukup untuk analisis)" : " (butuh minimal 5 frame)")
                                   + "  |  analisis tidak memblokir audio thread.",
                               juce::dontSendNotification);
    }
    else
    {
        juce::String notes = lastEqAnalysis.notes.joinIntoString ("  |  ");

        if (notes.isEmpty())
            notes = "Tidak ada masalah spektrum dominan yang terdeteksi.";

        eqStatusLabel.setText ("Saran: " + notes + "  |  confidence "
                                   + juce::String (juce::roundToInt (lastEqAnalysis.confidence * 100.0f)) + "%",
                               juce::dontSendNotification);
    }

    eqApplyButton.setEnabled (hasEqAnalysis && lastEqAnalysis.hasData);
    eqUndoButton.setEnabled (smartEq.canUndo());

    if (collectingHarmony)
    {
        if (vocalChain.isPitchVoiced() && vocalChain.getPitchConfidence() >= 0.55f)
            app.harmonyAssistant.addPitchObservation (vocalChain.getDetectedMidiNote(),
                                                      vocalChain.getPitchConfidence());

        ++harmonyCollectionTicks;

        harmonyStatusLabel.setText ("Mengumpulkan nada vokal: "
                                        + juce::String (app.harmonyAssistant.getObservationCount())
                                        + " observasi. Nyanyikan frasa pendek yang stabil.",
                                    juce::dontSendNotification);

        if (app.harmonyAssistant.getObservationCount() >= 60 || harmonyCollectionTicks > 200)
            finishHarmonyAnalysis();
    }

    masterVocalMeterLabel.setText ("Vocal bus " + juce::String (meters.vocalBusPeakDb, 1)
                                       + " dB  |  pitch " + app.params.getDisplayValue (ParamId::transpose),
                                   juce::dontSendNotification);

    updateHarmonyUi();
}

void VocalChainPage::loadVocalPreset (const juce::String& name)
{
    juce::String error;
    juce::StringArray warnings;

    if (! app.presets.load (PresetManager::Category::Vocal, name, error, &warnings))
    {
        showWarningMessage ("Preset vokal", error);
        return;
    }

    presetStatusLabel.setText ("Preset dimuat: " + name
                                   + (warnings.isEmpty() ? "" : " (dengan catatan)"),
                               juce::dontSendNotification);
    presetStatusLabel.setColour (juce::Label::textColourId, theme::green);

    if (! warnings.isEmpty())
        showWarningMessage ("Preset vokal", warnings.joinIntoString ("\n"));
}

void VocalChainPage::analyseEq()
{
    hasEqAnalysis = true;
    lastEqAnalysis = app.engine.getVocalChain().getSmartEQ().analyse();

    if (! lastEqAnalysis.hasData)
    {
        eqDisplay.clearPreview();
        eqStatusLabel.setText ("Belum cukup data spektrum. Pastikan mikrofon aktif dan ada suara, "
                               "lalu tunggu beberapa frame.", juce::dontSendNotification);
        eqStatusLabel.setColour (juce::Label::textColourId, theme::amber);
        eqApplyButton.setEnabled (false);
        return;
    }

    eqDisplay.setPreview (lastEqAnalysis);
    eqApplyButton.setEnabled (true);
    eqStatusLabel.setColour (juce::Label::textColourId, theme::textDim);

    juce::String notes = lastEqAnalysis.notes.joinIntoString ("  |  ");
    eqStatusLabel.setText ("Saran EQ (garis sebelum = amber): " + notes, juce::dontSendNotification);
}

void VocalChainPage::applyEq()
{
    if (! hasEqAnalysis || ! lastEqAnalysis.hasData)
        return;

    app.engine.getVocalChain().getSmartEQ().apply (lastEqAnalysis);
    eqDisplay.clearPreview();
    hasEqAnalysis = false;
    eqStatusLabel.setText ("EQ diterapkan ke 5 band. Gunakan Undo untuk kembali ke nilai sebelumnya.",
                           juce::dontSendNotification);
    eqApplyButton.setEnabled (false);
    eqUndoButton.setEnabled (app.engine.getVocalChain().getSmartEQ().canUndo());
}

void VocalChainPage::undoEq()
{
    app.engine.getVocalChain().getSmartEQ().undo();
    eqDisplay.clearPreview();
    hasEqAnalysis = false;
    eqStatusLabel.setText ("Undo Smart EQ diterapkan.", juce::dontSendNotification);
}

void VocalChainPage::resetEq()
{
    app.engine.getVocalChain().getSmartEQ().resetEq();
    eqDisplay.clearPreview();
    hasEqAnalysis = false;
    eqStatusLabel.setText ("Semua band EQ direset ke gain 0 dB.", juce::dontSendNotification);
}

void VocalChainPage::startHarmonyAnalysis()
{
    app.harmonyAssistant.beginAnalysis();
    currentSuggestion = {};
    collectingHarmony = true;
    harmonyCollectionTicks = 0;
    harmonyApplyButton.setEnabled (false);
    harmonyCancelButton.setEnabled (true);
    harmonySuggestionLabel.setText ("Menganalisis nada vokal. Nyanyikan beberapa detik dengan stabil...",
                                    juce::dontSendNotification);
    harmonyStatusLabel.setText ("Analisa lokal dimulai. Tidak ada audio yang dikirim ke cloud.",
                                juce::dontSendNotification);
}

void VocalChainPage::finishHarmonyAnalysis()
{
    collectingHarmony = false;
    harmonyCancelButton.setEnabled (true);
    currentSuggestion = app.harmonyAssistant.buildSuggestion();

    if (! currentSuggestion.valid)
    {
        harmonyApplyButton.setEnabled (false);
        harmonySuggestionLabel.setText ("Belum cukup nada stabil untuk membuat saran. "
                                        "Coba lagi sambil menyanyikan frasa yang lebih panjang.",
                                        juce::dontSendNotification);
        harmonySuggestionLabel.setColour (juce::Label::textColourId, theme::amber);
        harmonyStatusLabel.setText ("Saran tidak dibuat agar tidak menampilkan hasil yang tidak valid.",
                                    juce::dontSendNotification);
        return;
    }

    harmonyApplyButton.setEnabled (true);
    harmonySuggestionLabel.setColour (juce::Label::textColourId, theme::textDim);
    harmonySuggestionLabel.setText (currentSuggestion.summary + "\n"
                                        + "Catatan: " + currentSuggestion.notes.joinIntoString (", "),
                                    juce::dontSendNotification);
    harmonyStatusLabel.setText ("Saran siap: key " + juce::String (music::keyNames[juce::jlimit (0, 11, currentSuggestion.key)])
                                    + " / " + juce::String (music::scaleNames[juce::jlimit (0, 3, currentSuggestion.scale)])
                                    + ". Rule-based, bukan model AI terlatih.",
                                juce::dontSendNotification);
}

void VocalChainPage::cancelHarmonyAnalysis()
{
    collectingHarmony = false;
    currentSuggestion = {};
    harmonyApplyButton.setEnabled (false);
    harmonyCancelButton.setEnabled (false);
    harmonySuggestionLabel.setColour (juce::Label::textColourId, theme::textDim);
    harmonySuggestionLabel.setText ("Analisis dibatalkan. Tidak ada parameter yang diubah.",
                                    juce::dontSendNotification);
    harmonyStatusLabel.setText ("Belum ada analisis. Tekan Analisa Nada bila ingin mencoba lagi.",
                                juce::dontSendNotification);
}

void VocalChainPage::applyHarmonySuggestion()
{
    if (! currentSuggestion.valid)
        return;

    app.harmonyAssistant.applySuggestion (app.params, currentSuggestion);
    harmonyStatusLabel.setText ("Saran diterapkan ke parameter harmonizer. Dengarkan hasilnya dan "
                                "sesuaikan level/dry-wet bila perlu.",
                                juce::dontSendNotification);
}

void VocalChainPage::updateHarmonyUi()
{
    const auto controllable = app.params.getBool (ParamId::harmonyEnabled)
                           && ! app.params.getBool (ParamId::harmonyBypass);

    juce::Component* controllableComponents[] = { &voice1Toggle, &voice2Toggle,
                                                  &voice1IntervalCombo, &voice2IntervalCombo,
                                                  &voice1CustomSlider, &voice2CustomSlider,
                                                  &harmonyLevelSlider, &harmonyDryWetSlider,
                                                  &humanizeSlider, &harmonyRetuneSlider,
                                                  &trackingSlider, &stereoWidthSlider,
                                                  &formantToggle };

    for (auto* component : controllableComponents)
        component->setEnabled (controllable);
}

} // namespace rr
