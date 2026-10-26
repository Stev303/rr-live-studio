#include "UI/Pages/VST3Page.h"

namespace rr
{
class VST3Page::ChainPanel : public Panel
{
public:
    ChainPanel (VST3Page& owner, bool isMaster)
        : Panel (isMaster ? "Master VST3 Chain" : "Vocal VST3 Chain",
                 isMaster ? theme::amber : theme::cyan),
          page (owner),
          masterChain (isMaster)
    {
        configureLabel (countLabel, 10.0f, theme::textDim);
        configureLabel (emptyLabel, 11.0f, theme::textDim, false, juce::Justification::centred);
        emptyLabel.setText ("Belum ada plugin. Pilih plugin di daftar lalu tekan Tambah.", juce::dontSendNotification);

        addAndMakeVisible (countLabel);
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        content.addAndMakeVisible (emptyLabel);

        refresh();
    }

    void refresh()
    {
        rows.clear();
        content.removeAllChildren();

        const auto info = page.host.getChainInfo (masterChain);

        for (int i = 0; i < (int) info.size(); ++i)
        {
            auto row = std::make_unique<Row> (page, masterChain, i, info[(size_t) i]);
            content.addAndMakeVisible (*row);
            rows.push_back (std::move (row));
        }

        countLabel.setText (juce::String ((int) info.size()) + "/"
                                + juce::String (PluginHost::maxSlotsPerChain)
                                + " plugin  |  latensi chain "
                                + juce::String (page.host.getChainLatencySamples (masterChain)) + " smp",
                            juce::dontSendNotification);

        emptyLabel.setVisible (rows.empty());

        if (rows.empty())
            content.addAndMakeVisible (emptyLabel);

        resized();
    }

    void resized() override
    {
        auto inner = getContentBounds().reduced (8, 4);
        countLabel.setBounds (inner.removeFromTop (18));
        viewport.setBounds (inner);

        const auto rowHeight = 44;
        content.setSize (juce::jmax (200, viewport.getMaximumVisibleWidth()),
                         juce::jmax (viewport.getHeight(), (int) rows.size() * rowHeight + 8));

        int y = 4;

        for (auto& row : rows)
        {
            row->setBounds (2, y, content.getWidth() - 4, rowHeight - 4);
            y += rowHeight;
        }

        if (rows.empty())
            emptyLabel.setBounds (content.getLocalBounds().reduced (12));
    }

private:
    struct Row : public juce::Component
    {
        Row (VST3Page& owner, bool isMaster, int chainIndex, const PluginHost::SlotInfo& info)
            : page (owner), masterChain (isMaster), index (chainIndex)
        {
            configureLabel (nameLabel, 12.0f, theme::text, true);
            nameLabel.setText (juce::String (index + 1) + ". " + info.name, juce::dontSendNotification);

            configureLabel (detailLabel, 9.5f, theme::textDim);
            detailLabel.setText (info.vendor + "  |  " + info.format
                                     + (info.bypassed ? "  |  BYPASS" : ""),
                                 juce::dontSendNotification);

            styleActionButton (bypassButton, theme::amber);
            bypassButton.setClickingTogglesState (true);
            bypassButton.setToggleState (info.bypassed, juce::dontSendNotification);
            bypassButton.setButtonText (info.bypassed ? "Bypass" : "Aktif");
            bypassButton.setTooltip ("Bypass plugin pada chain");
            bypassButton.onClick = [this]
            {
                page.host.setPluginBypass (masterChain, index, bypassButton.getToggleState());
            };

            styleActionButton (editorButton);
            editorButton.setButtonText ("Editor");
            editorButton.setEnabled (info.hasEditor);
            editorButton.setTooltip (info.hasEditor ? "Buka editor plugin"
                                                    : "Plugin ini tidak menyediakan editor.");
            editorButton.onClick = [this]
            {
                juce::String error;

                if (! page.host.openPluginEditor (masterChain, index, error))
                    showWarningMessage ("Editor plugin", error);
            };

            styleActionButton (upButton);
            styleActionButton (downButton);
            styleActionButton (removeButton, theme::red);
            upButton.setButtonText ("^");
            downButton.setButtonText ("v");
            removeButton.setButtonText ("Hapus");
            upButton.setTooltip ("Naikkan urutan");
            downButton.setTooltip ("Turunkan urutan");
            removeButton.setTooltip ("Hapus plugin dari chain");

            upButton.onClick = [this] { page.host.movePlugin (masterChain, index, index - 1); };
            downButton.onClick = [this] { page.host.movePlugin (masterChain, index, index + 1); };
            removeButton.onClick = [this] { page.host.removePlugin (masterChain, index); };

            addAndMakeVisible (nameLabel);
            addAndMakeVisible (detailLabel);
            addAndMakeVisible (bypassButton);
            addAndMakeVisible (editorButton);
            addAndMakeVisible (upButton);
            addAndMakeVisible (downButton);
            addAndMakeVisible (removeButton);
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (1.0f, 1.0f);
            g.setColour (theme::panelAlt);
            g.fillRoundedRectangle (bounds, 5.0f);
            g.setColour ((masterChain ? theme::amber : theme::cyan).withAlpha (0.5f));
            g.fillRect (bounds.getX(), bounds.getY() + 5.0f, 2.0f, bounds.getHeight() - 10.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8, 4);
            removeButton.setBounds (area.removeFromRight (52));
            area.removeFromRight (4);
            downButton.setBounds (area.removeFromRight (26));
            area.removeFromRight (2);
            upButton.setBounds (area.removeFromRight (26));
            area.removeFromRight (4);
            editorButton.setBounds (area.removeFromRight (54));
            area.removeFromRight (4);
            bypassButton.setBounds (area.removeFromRight (62));
            area.removeFromRight (8);
            nameLabel.setBounds (area.removeFromTop (area.getHeight() / 2));
            detailLabel.setBounds (area);
        }

        VST3Page& page;
        bool masterChain;
        int index;
        juce::Label nameLabel;
        juce::Label detailLabel;
        juce::TextButton bypassButton;
        juce::TextButton editorButton;
        juce::TextButton upButton;
        juce::TextButton downButton;
        juce::TextButton removeButton;
    };

    VST3Page& page;
    bool masterChain;
    juce::Viewport viewport;
    juce::Component content;
    juce::Label countLabel;
    juce::Label emptyLabel;
    std::vector<std::unique_ptr<Row>> rows;
};

//==============================================================================
VST3Page::VST3Page (AppState& state)
    : app (state),
      host (state.pluginHost),
      progressBar (scanProgress),
      knownList ("Plugin VST3", this)
{
    vocalChainPanel = std::make_unique<ChainPanel> (*this, false);
    masterChainPanel = std::make_unique<ChainPanel> (*this, true);

    addAndMakeVisible (scanPanel);
    addAndMakeVisible (knownPanel);
    addAndMakeVisible (*vocalChainPanel);
    addAndMakeVisible (*masterChainPanel);

    configureLabel (folderLabel, 10.5f, theme::textDim);
    folderLabel.setText (host.getScanFolder().getFullPathName(), juce::dontSendNotification);
    folderLabel.setTooltip (host.getScanFolder().getFullPathName());

    styleActionButton (chooseFolderButton);
    chooseFolderButton.onClick = [this] { chooseFolder(); };
    styleActionButton (scanButton, theme::magenta);
    scanButton.onClick = [this] { startScan(); };
    styleActionButton (failedFilesButton, theme::amber);
    failedFilesButton.onClick = [this] { showFailedFiles(); };

    progressBar.setColour (juce::ProgressBar::backgroundColourId, theme::panelAlt);
    progressBar.setColour (juce::ProgressBar::foregroundColourId, theme::magenta);

    configureLabel (scanStatusLabel, 10.0f, theme::textDim);
    scanStatusLabel.setText (host.getScanStatus(), juce::dontSendNotification);

    configureLabel (warningLabel, 9.5f, theme::amber);
    warningLabel.setText ("Plugin berjalan in-process; plugin cacat dapat membuat aplikasi crash atau hang. "
                          "Tidak ada jaminan kompatibilitas semua VST3.",
                          juce::dontSendNotification);
    warningLabel.setTooltip ("Rr Live Studio memakai host VST3 resmi JUCE. Plugin pihak ketiga tetap "
                             "berjalan di proses aplikasi, sehingga crash plugin tidak dapat diisolasi.");

    scanPanel.addAndMakeVisible (folderLabel);
    scanPanel.addAndMakeVisible (chooseFolderButton);
    scanPanel.addAndMakeVisible (scanButton);
    scanPanel.addAndMakeVisible (failedFilesButton);
    scanPanel.addAndMakeVisible (progressBar);
    scanPanel.addAndMakeVisible (scanStatusLabel);
    scanPanel.addAndMakeVisible (warningLabel);

    searchEditor.setFont (theme::font (11.5f));
    searchEditor.setTextToShowWhenEmpty ("Cari nama plugin atau vendor...", theme::textDim);
    searchEditor.onTextChange = [this] { rebuildKnownList(); };

    styleActionButton (addVocalButton, theme::cyan);
    styleActionButton (addMasterButton, theme::amber);
    addVocalButton.onClick = [this] { addSelectedToChain (false); };
    addMasterButton.onClick = [this] { addSelectedToChain (true); };
    addVocalButton.setEnabled (false);
    addMasterButton.setEnabled (false);

    knownList.setRowHeight (36);
    knownList.setMultipleSelectionEnabled (false);
    knownList.setColour (juce::ListBox::backgroundColourId, theme::panelAlt);
    knownList.setColour (juce::ListBox::outlineColourId, theme::border);
    knownList.setOutlineThickness (1);
    knownList.setModel (this);

    configureLabel (selectionLabel, 9.5f, theme::textDim);
    selectionLabel.setText ("Pilih plugin untuk menambahkannya ke chain.", juce::dontSendNotification);

    knownPanel.addAndMakeVisible (searchEditor);
    knownPanel.addAndMakeVisible (knownList);
    knownPanel.addAndMakeVisible (addVocalButton);
    knownPanel.addAndMakeVisible (addMasterButton);
    knownPanel.addAndMakeVisible (selectionLabel);

    host.addChangeListener (this);
    rebuildKnownList();
    startTimerHz (5);
}

VST3Page::~VST3Page()
{
    stopTimer();
    host.removeChangeListener (this);
    vocalChainPanel.reset();
    masterChainPanel.reset();
}

void VST3Page::paint (juce::Graphics&)
{
}

void VST3Page::resized()
{
    auto area = getLocalBounds().reduced (14);
    scanPanel.setBounds (area.removeFromTop (140));
    area.removeFromTop (10);

    auto chains = area.removeFromTop (260);
    const auto half = chains.getWidth() / 2;
    vocalChainPanel->setBounds (chains.removeFromLeft (half - 5));
    chains.removeFromLeft (10);
    masterChainPanel->setBounds (chains);

    area.removeFromTop (10);
    knownPanel.setBounds (area);

    {
        auto inner = scanPanel.getContentBounds().reduced (10, 5);
        auto folderRow = inner.removeFromTop (24);
        chooseFolderButton.setBounds (folderRow.removeFromRight (96));
        folderRow.removeFromRight (6);
        folderLabel.setBounds (folderRow);
        inner.removeFromTop (4);

        auto scanRow = inner.removeFromTop (26);
        scanButton.setBounds (scanRow.removeFromLeft (110));
        scanRow.removeFromLeft (6);
        progressBar.setBounds (scanRow.removeFromLeft (140));
        scanRow.removeFromLeft (6);
        failedFilesButton.setBounds (scanRow.removeFromLeft (82));
        scanRow.removeFromLeft (8);
        scanStatusLabel.setBounds (scanRow);
        inner.removeFromTop (4);
        warningLabel.setBounds (inner.removeFromTop (16));
    }

    {
        auto inner = knownPanel.getContentBounds().reduced (10, 5);
        auto topRow = inner.removeFromTop (28);
        addMasterButton.setBounds (topRow.removeFromRight (118));
        topRow.removeFromRight (6);
        addVocalButton.setBounds (topRow.removeFromRight (118));
        topRow.removeFromRight (6);
        searchEditor.setBounds (topRow);
        inner.removeFromTop (5);
        selectionLabel.setBounds (inner.removeFromBottom (16));
        knownList.setBounds (inner);
    }
}

void VST3Page::timerCallback()
{
    const auto scanning = host.isScanning();
    scanProgress = (double) host.getScanProgress();
    progressBar.repaint();

    auto status = host.getScanStatus();

    if (! scanning && wasScanning)
    {
        wasScanning = false;
        rebuildKnownList();
    }
    else if (scanning)
    {
        wasScanning = true;
    }

    if (! scanning)
    {
        const auto failed = host.getFailedScanFiles().size();
        status << "  |  " << juce::String (host.getKnownPlugins().size()) << " plugin terdeteksi";

        if (failed > 0)
            status << "  |  " << juce::String (failed) << " file gagal";
    }

    scanStatusLabel.setText (status, juce::dontSendNotification);
    scanButton.setEnabled (! scanning && host.getScanFolder().isDirectory());
    failedFilesButton.setEnabled (! host.getFailedScanFiles().isEmpty());
    progressBar.setVisible (scanning);
}

void VST3Page::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source != &host)
        return;

    folderLabel.setText (host.getScanFolder().getFullPathName(), juce::dontSendNotification);
    folderLabel.setTooltip (host.getScanFolder().getFullPathName());

    if (vocalChainPanel != nullptr)
        vocalChainPanel->refresh();

    if (masterChainPanel != nullptr)
        masterChainPanel->refresh();

    rebuildKnownList();
    updateSelectionUi();
}

void VST3Page::chooseFolder()
{
    auto initial = host.getScanFolder();

    if (! initial.isDirectory())
        initial = PluginHost::getDefaultVST3Folder();

    folderChooser = std::make_unique<juce::FileChooser> ("Folder VST3", initial, "*");

    folderChooser->launchAsync (juce::FileBrowserComponent::openMode
                                    | juce::FileBrowserComponent::canSelectDirectories,
                                [this] (const juce::FileChooser& chooser)
                                {
                                    const auto result = chooser.getResult();

                                    if (result.isDirectory())
                                    {
                                        host.setScanFolder (result);
                                        folderLabel.setText (result.getFullPathName(),
                                                             juce::dontSendNotification);
                                        folderLabel.setTooltip (result.getFullPathName());
                                    }
                                });
}

void VST3Page::startScan()
{
    const auto folder = host.getScanFolder();

    if (! folder.isDirectory())
    {
        showWarningMessage ("Scan VST3", "Folder VST3 belum valid. Pilih folder terlebih dahulu.");
        return;
    }

    scanButton.setEnabled (false);
    wasScanning = true;
    host.scanFolderAsync (folder);
}

void VST3Page::rebuildKnownList()
{
    const auto query = searchEditor.getText().trim().toLowerCase();
    filteredPlugins.clear();

    for (const auto& plugin : host.getKnownPlugins())
    {
        if (query.isNotEmpty()
            && ! plugin.name.toLowerCase().contains (query)
            && ! plugin.manufacturerName.toLowerCase().contains (query)
            && ! plugin.pluginFormatName.toLowerCase().contains (query))
            continue;

        filteredPlugins.add (plugin);
    }

    std::sort (filteredPlugins.begin(), filteredPlugins.end(),
               [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
               {
                   return a.name.compareIgnoreCase (b.name) < 0;
               });

    knownList.updateContent();
    knownList.deselectAllRows();
    updateSelectionUi();
}

int VST3Page::getNumRows()
{
    return filteredPlugins.size();
}

void VST3Page::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                                 bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, filteredPlugins.size()))
        return;

    const auto& plugin = filteredPlugins[rowNumber];

    if (rowIsSelected)
    {
        g.setColour (theme::cyan.withAlpha (0.16f));
        g.fillRect (0, 0, width, height);
    }

    g.setColour (theme::border.withAlpha (0.4f));
    g.fillRect (0, height - 1, width, 1);

    g.setColour (theme::text);
    g.setFont (theme::font (12.0f, true));
    g.drawText (plugin.name, 8, 3, width - 16, height / 2, juce::Justification::centredLeft, true);

    g.setColour (theme::textDim);
    g.setFont (theme::font (9.5f));
    g.drawText (plugin.manufacturerName + "  |  " + plugin.pluginFormatName,
                8, height / 2, width - 16, height / 2 - 3,
                juce::Justification::centredLeft, true);
}

void VST3Page::selectedRowsChanged (int)
{
    updateSelectionUi();
}

void VST3Page::returnKeyPressed (int)
{
    addSelectedToChain (false);
}

void VST3Page::updateSelectionUi()
{
    const auto row = knownList.getSelectedRow();
    const auto valid = juce::isPositiveAndBelow (row, filteredPlugins.size());

    if (valid)
    {
        const auto& plugin = filteredPlugins[row];
        selectionLabel.setText (plugin.name + " oleh "
                                   + (plugin.manufacturerName.isEmpty() ? "vendor tidak dikenal"
                                                                        : plugin.manufacturerName),
                               juce::dontSendNotification);

        const auto canVocal = host.getChainSize (false) < PluginHost::maxSlotsPerChain;
        const auto canMaster = host.getChainSize (true) < PluginHost::maxSlotsPerChain;
        addVocalButton.setEnabled (canVocal);
        addMasterButton.setEnabled (canMaster);
    }
    else
    {
        selectionLabel.setText ("Pilih plugin untuk menambahkannya ke chain.", juce::dontSendNotification);
        addVocalButton.setEnabled (false);
        addMasterButton.setEnabled (false);
    }
}

void VST3Page::addSelectedToChain (bool masterChain)
{
    const auto row = knownList.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, filteredPlugins.size()))
        return;

    juce::String error;

    if (! host.addPlugin (masterChain, filteredPlugins[row], error))
        showWarningMessage ("Tambah plugin", error);
}

void VST3Page::showFailedFiles()
{
    const auto failed = host.getFailedScanFiles();

    if (failed.isEmpty())
    {
        showInfoMessage ("Scan VST3", "Tidak ada file yang gagal pada pemindaian terakhir.");
        return;
    }

    showWarningMessage ("File VST3 gagal dimuat",
                        failed.joinIntoString ("\n")
                            + "\n\nFile bisa berupa plugin 32-bit, rusak, atau tidak kompatibel.");
}

} // namespace rr
