#include "UI/Pages/SamplerPage.h"

namespace rr
{
namespace
{
    const juce::String padFilePattern = "*.wav;*.aiff;*.aif;*.flac;*.mp3";

    juce::String formatPeakDb (float db)
    {
        if (db <= -99.0f)
            return juce::String::fromUTF8 ("\xe2\x80\x94");

        return juce::String (db, 1) + " dB";
    }
}

//==============================================================================
class SamplerPage::PadComponent : public juce::Component
{
public:
    PadComponent (int index, SamplerEngine& engine)
        : padIndex (index), sampler (engine)
    {
        configureLabel (numberLabel, 10.0f, theme::textDim, true);
        numberLabel.setText ("PAD " + juce::String (padIndex + 1).paddedLeft ('0', 2),
                             juce::dontSendNotification);

        configureLabel (nameLabel, 15.0f, theme::text, true);
        configureLabel (fileLabel, 10.0f, theme::textDim);
        fileLabel.setJustificationType (juce::Justification::topLeft);
        fileLabel.setMinimumHorizontalScale (0.7f);

        configureLabel (gainLabel, 10.0f, theme::textDim);
        gainLabel.setText ("Gain", juce::dontSendNotification);

        styleActionButton (playButton, theme::green);
        playButton.setTooltip ("Mainkan atau hentikan pad ini.");
        playButton.onClick = [this] { togglePlayback(); };

        styleActionButton (loadButton);
        loadButton.setTooltip ("Muat sample WAV, AIFF, FLAC, atau MP3.");
        loadButton.onClick = [this]
        {
            if (onLoad != nullptr)
                onLoad (padIndex);
        };

        styleActionButton (renameButton);
        renameButton.setTooltip ("Ganti nama pad.");
        renameButton.onClick = [this]
        {
            if (onRename != nullptr)
                onRename (padIndex);
        };

        styleActionButton (clearButton, theme::red);
        clearButton.setTooltip ("Kosongkan pad.");
        clearButton.onClick = [this]
        {
            if (onClear != nullptr)
                onClear (padIndex);
        };

        loopButton.setButtonText ("Loop");
        loopButton.setTooltip ("Ulangi sample terus-menerus.");
        loopButton.onClick = [this] { sampler.setPadLoop (padIndex, loopButton.getToggleState()); };

        gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
        gainSlider.setRange (-48.0, 12.0, 0.1);
        gainSlider.setDoubleClickReturnValue (true, 0.0);
        gainSlider.setValue (sampler.getPadInfo (padIndex).gainDb, juce::dontSendNotification);
        gainSlider.setTooltip ("Volume pad. Rentang -48 sampai +12 dB.");
        gainSlider.onValueChange = [this]
        {
            sampler.setPadGainDb (padIndex, (float) gainSlider.getValue());
        };

        addAndMakeVisible (numberLabel);
        addAndMakeVisible (nameLabel);
        addAndMakeVisible (fileLabel);
        addAndMakeVisible (playButton);
        addAndMakeVisible (loadButton);
        addAndMakeVisible (renameButton);
        addAndMakeVisible (clearButton);
        addAndMakeVisible (loopButton);
        addAndMakeVisible (gainLabel);
        addAndMakeVisible (gainSlider);

        refresh();
        updatePlayButton();
    }

    std::function<void (int)> onLoad;
    std::function<void (int)> onRename;
    std::function<void (int)> onClear;

    void refresh()
    {
        const auto info = sampler.getPadInfo (padIndex);

        if (info.name != currentName)
        {
            currentName = info.name;
            nameLabel.setText (currentName, juce::dontSendNotification);
        }

        if (info.filePath != currentPath)
        {
            currentPath = info.filePath;

            if (currentPath.isEmpty())
            {
                fileLabel.setText ("Belum ada sample", juce::dontSendNotification);
                fileLabel.setTooltip ({});
            }
            else
            {
                const juce::File file (currentPath);
                fileLabel.setText (file.getFileName(), juce::dontSendNotification);
                fileLabel.setTooltip (currentPath);
            }
        }

        if (! gainSlider.isMouseButtonDown()
            && ! juce::approximatelyEqual (currentGainDb, info.gainDb))
        {
            currentGainDb = info.gainDb;
            gainSlider.setValue (currentGainDb, juce::dontSendNotification);
        }

        if (currentLoop != info.loop)
        {
            currentLoop = info.loop;
            loopButton.setToggleState (currentLoop, juce::dontSendNotification);
        }

        const bool loadedChanged = currentLoaded != info.loaded;
        const bool playingChanged = currentPlaying != info.playing;

        currentLoaded = info.loaded;
        currentPlaying = info.playing;

        if (loadedChanged || playingChanged)
        {
            updatePlayButton();
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto base = currentPlaying ? theme::green.withAlpha (0.10f) : theme::panelAlt;

        g.setColour (base);
        g.fillRoundedRectangle (bounds, 10.0f);

        g.setColour (currentPlaying ? theme::green.withAlpha (0.9f)
                                    : currentLoaded ? theme::amber.withAlpha (0.55f)
                                                    : theme::border);
        g.drawRoundedRectangle (bounds, 10.0f, currentPlaying ? 1.6f : 1.0f);

        g.setColour (currentPlaying ? theme::green : theme::amber);
        g.fillRoundedRectangle (bounds.getX() + 12.0f, bounds.getY() + 12.0f,
                                3.0f, 30.0f, 1.5f);

        if (! currentLoaded)
        {
            g.setColour (theme::textDim.withAlpha (0.06f));
            g.setFont (theme::font (34.0f, true));
            g.drawText (juce::String (padIndex + 1), getLocalBounds().withTrimmedTop (70),
                        juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (10);
        auto top = area.removeFromTop (20);
        numberLabel.setBounds (top.removeFromLeft (70));
        loopButton.setBounds (top.removeFromRight (58));

        nameLabel.setBounds (area.removeFromTop (22));

        auto fileArea = area.removeFromTop (30);
        fileLabel.setBounds (fileArea);

        area.removeFromTop (4);
        playButton.setBounds (area.removeFromTop (70));

        area.removeFromTop (6);
        auto buttonRow = area.removeFromTop (26);
        const auto buttonWidth = (buttonRow.getWidth() - 8) / 3;
        loadButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
        buttonRow.removeFromLeft (4);
        renameButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
        buttonRow.removeFromLeft (4);
        clearButton.setBounds (buttonRow);

        area.removeFromTop (8);
        auto gainRow = area.removeFromTop (juce::jmin (24, area.getHeight()));
        gainLabel.setBounds (gainRow.removeFromLeft (32));
        gainSlider.setBounds (gainRow);
    }

private:
    void updatePlayButton()
    {
        playButton.setButtonText (currentPlaying ? "STOP" : currentLoaded ? "MAIN" : "MUAT");
        playButton.setTooltip (currentLoaded ? "Mainkan atau hentikan pad ini."
                                             : "Pad kosong. Klik untuk memuat sample.");
        playButton.setColour (juce::TextButton::buttonColourId,
                              currentPlaying ? theme::red.withAlpha (0.28f)
                                             : theme::green.withAlpha (0.12f));
        playButton.setColour (juce::TextButton::textColourOffId,
                              currentPlaying ? theme::red : theme::green);
    }

    void togglePlayback()
    {
        const auto info = sampler.getPadInfo (padIndex);

        if (! info.loaded)
        {
            if (onLoad != nullptr)
                onLoad (padIndex);

            return;
        }

        if (info.playing)
            sampler.stopPad (padIndex);
        else
            sampler.triggerPad (padIndex);

        refresh();
    }

    int padIndex;
    SamplerEngine& sampler;

    juce::Label numberLabel;
    juce::Label nameLabel;
    juce::Label fileLabel;
    juce::Label gainLabel;
    juce::TextButton playButton { "MAIN" };
    juce::TextButton loadButton { "Muat" };
    juce::TextButton renameButton { "Nama" };
    juce::TextButton clearButton { "Hapus" };
    juce::ToggleButton loopButton;
    juce::Slider gainSlider;

    juce::String currentName;
    juce::String currentPath;
    float currentGainDb = 0.0f;
    bool currentLoop = false;
    bool currentPlaying = false;
    bool currentLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PadComponent)
};

//==============================================================================
SamplerPage::SamplerPage (AppState& state)
    : app (state),
      samplerFader (app.params, ParamId::samplerMixDb, juce::Slider::LinearHorizontal),
      samplerMute (app.params, ParamId::samplerMute, "Mute"),
      samplerSolo (app.params, ParamId::samplerSolo, "Solo")
{
    for (int i = 0; i < SamplerEngine::numPads; ++i)
    {
        auto pad = std::make_unique<PadComponent> (i, app.sampler);
        pad->onLoad = [this] (int index) { loadSampleForPad (index); };
        pad->onRename = [this] (int index) { renamePad (index); };
        pad->onClear = [this] (int index) { clearPadWithConfirm (index); };
        pads[(size_t) i] = std::move (pad);
        content.addAndMakeVisible (*pads[(size_t) i]);
    }

    styleActionButton (stopAllButton, theme::red);
    stopAllButton.setTooltip ("Hentikan semua pad yang sedang berbunyi.");
    stopAllButton.onClick = [this]
    {
        app.sampler.stopAll();
        refreshPadStates();
        updateStatus();
    };

    styleActionButton (saveSessionButton, theme::green);
    saveSessionButton.setTooltip ("Simpan sample, nama, gain, dan loop ke sesi aplikasi.");
    saveSessionButton.onClick = [this]
    {
        app.save();
        showInfoMessage ("Sesi disimpan",
                         "State sampler, parameter, chain VST, dan perangkat audio disimpan.");
    };

    configureLabel (meterLabel, 10.5f, theme::text);
    meterLabel.setFont (theme::monoFont (10.5f));
    configureLabel (statusLabel, 10.5f, theme::textDim);
    configureLabel (infoLabel, 10.0f, theme::textDim.withAlpha (0.85f));
    infoLabel.setJustificationType (juce::Justification::topLeft);
    infoLabel.setText ("Format yang didukung: WAV, AIFF, FLAC, dan MP3 (codec MP3 harus aktif di build). "
                       "Sample dimuat di message thread dan diputar dari bus Sampler. "
                       "Fader, mute, dan solo sampler juga diatur di halaman Mixer.",
                       juce::dontSendNotification);

    headerPanel.addAndMakeVisible (samplerFader);
    headerPanel.addAndMakeVisible (samplerMute);
    headerPanel.addAndMakeVisible (samplerSolo);
    headerPanel.addAndMakeVisible (samplerMeter);
    headerPanel.addAndMakeVisible (stopAllButton);
    headerPanel.addAndMakeVisible (saveSessionButton);
    headerPanel.addAndMakeVisible (meterLabel);
    headerPanel.addAndMakeVisible (statusLabel);
    headerPanel.addAndMakeVisible (infoLabel);
    content.addAndMakeVisible (headerPanel);

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);

    startTimerHz (15);
    updateStatus();
}

SamplerPage::~SamplerPage()
{
    stopTimer();
}

void SamplerPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void SamplerPage::resized()
{
    viewport.setBounds (getLocalBounds());

    const auto width = juce::jmax (480, viewport.getMaximumVisibleWidth());
    constexpr int headerHeight = 178;
    constexpr int gap = 10;
    constexpr int padHeight = 238;
    const int margin = 6;
    const int usableWidth = width - margin * 2;
    const int columns = juce::jlimit (2, 5, juce::jmax (2, usableWidth / 220));
    const int rows = (SamplerEngine::numPads + columns - 1) / columns;
    const int padWidth = (usableWidth - gap * (columns - 1)) / columns;
    const int contentHeight = 4 + headerHeight + gap + rows * (padHeight + gap) + 4;

    content.setSize (width, juce::jmax (contentHeight, viewport.getHeight()));

    headerPanel.setBounds (margin, 4, usableWidth, headerHeight);

    const int padsTop = 4 + headerHeight + gap;

    for (int i = 0; i < SamplerEngine::numPads; ++i)
    {
        const int column = i % columns;
        const int row = i / columns;
        pads[(size_t) i]->setBounds (margin + column * (padWidth + gap),
                                     padsTop + row * (padHeight + gap),
                                     padWidth, padHeight);
    }

    auto header = headerPanel.getContentBounds();
    auto topRow = header.removeFromTop (58);
    auto left = topRow.removeFromLeft (juce::jmin (300, topRow.getWidth() / 2));
    samplerFader.setBounds (left.removeFromTop (44));
    auto toggles = left.removeFromTop (22);
    samplerMute.setBounds (toggles.removeFromLeft (86));
    samplerSolo.setBounds (toggles.removeFromLeft (86));

    auto actions = topRow.removeFromLeft (juce::jmin (240, juce::jmax (120, topRow.getWidth() - 180)));
    stopAllButton.setBounds (actions.removeFromTop (26));
    actions.removeFromTop (4);
    saveSessionButton.setBounds (actions.removeFromTop (26));

    auto meterArea = topRow.reduced (8, 0);
    meterLabel.setBounds (meterArea.removeFromRight (70));
    samplerMeter.setBounds (meterArea.withSizeKeepingCentre (meterArea.getWidth(), 18));

    header.removeFromTop (6);
    statusLabel.setBounds (header.removeFromTop (18));
    infoLabel.setBounds (header);
}

void SamplerPage::timerCallback()
{
    refreshPadStates();

    const auto meters = app.engine.getMeters();
    samplerMeter.setLevelDb (meters.samplerPeakDb, meters.samplerPeakDb);
    samplerMeter.setClip (meters.samplerPeakDb >= 0.0f);
    meterLabel.setText (formatPeakDb (meters.samplerPeakDb), juce::dontSendNotification);

    updateStatus();
}

void SamplerPage::refreshPadStates()
{
    for (auto& pad : pads)
        if (pad != nullptr)
            pad->refresh();
}

void SamplerPage::updateStatus()
{
    const auto active = app.sampler.getActiveVoiceCount();
    const auto separator = juce::String::fromUTF8 (" \xc2\xb7 ");
    auto text = juce::String ("Pad aktif: ") + juce::String (active) + " / "
                + juce::String (SamplerEngine::numPads)
                + separator + "Puncak sampler: " + formatPeakDb (app.sampler.getPeakDb())
                + separator + "Engine: " + (app.engine.isRunning() ? "berjalan" : "berhenti");

    if (! app.engine.isRunning())
        text << " (sampler tidak terdengar sampai audio device aktif)";

    if (text != lastStatus)
    {
        lastStatus = text;
        statusLabel.setText (text, juce::dontSendNotification);
    }
}

void SamplerPage::loadSampleForPad (int padIndex)
{
    auto startFolder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    const auto existing = app.sampler.getPadInfo (padIndex).filePath;

    if (existing.isNotEmpty())
    {
        const juce::File file (existing);

        if (file.getParentDirectory().isDirectory())
            startFolder = file.getParentDirectory();
    }

    fileChooser = std::make_unique<juce::FileChooser> ("Pilih sample untuk Pad "
                                                       + juce::String (padIndex + 1),
                                                       startFolder,
                                                       padFilePattern);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this, padIndex] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();

                                  if (! file.existsAsFile())
                                      return;

                                  juce::MouseCursor::showWaitCursor();

                                  juce::String error;
                                  const bool loaded = app.sampler.loadPad (padIndex, file, error);

                                  juce::MouseCursor::hideWaitCursor();

                                  if (loaded)
                                  {
                                      if (pads[(size_t) padIndex] != nullptr)
                                          pads[(size_t) padIndex]->refresh();

                                      refreshPadStates();
                                      updateStatus();
                                  }
                                  else
                                  {
                                      showWarningMessage ("Gagal memuat sample", error);
                                  }
                              });
}

void SamplerPage::renamePad (int padIndex)
{
    const auto info = app.sampler.getPadInfo (padIndex);

    promptForText ("Ganti Nama Pad",
                   "Nama baru untuk pad " + juce::String (padIndex + 1) + ":",
                   info.name,
                   [this, padIndex] (juce::String newName)
                   {
                       app.sampler.setPadName (padIndex, newName);

                       if (pads[(size_t) padIndex] != nullptr)
                           pads[(size_t) padIndex]->refresh();
                   });
}

void SamplerPage::clearPadWithConfirm (int padIndex)
{
    const auto info = app.sampler.getPadInfo (padIndex);

    if (! info.loaded && info.filePath.isEmpty())
        return;

    showConfirm ("Kosongkan Pad " + juce::String (padIndex + 1),
                 "Sample \"" + info.name + "\" akan dilepas dari pad ini. File asli tidak dihapus.",
                 "Kosongkan",
                 [this, padIndex]
                 {
                     app.sampler.clearPad (padIndex);

                     if (pads[(size_t) padIndex] != nullptr)
                         pads[(size_t) padIndex]->refresh();

                     updateStatus();
                 });
}

} // namespace rr
