#include "UI/Pages/PresetsPage.h"

namespace rr
{
namespace
{
    constexpr std::array<PresetManager::Category, 6> presetCategories
    {
        PresetManager::Category::Vocal,
        PresetManager::Category::Music,
        PresetManager::Category::Harmonizer,
        PresetManager::Category::Master,
        PresetManager::Category::Sampler,
        PresetManager::Category::Scene
    };
}

PresetsPage::PresetsPage (AppState& state)
    : app (state)
{
    for (int i = 0; i < (int) presetCategories.size(); ++i)
    {
        const auto name = PresetManager::categoryName (presetCategories[(size_t) i]);
        auto button = std::make_unique<juce::TextButton> (name);
        styleActionButton (*button);
        button->setClickingTogglesState (true);
        button->setRadioGroupId (0x525052);
        button->setColour (juce::TextButton::buttonOnColourId, theme::cyan.withAlpha (0.22f));
        button->setColour (juce::TextButton::textColourOnId, theme::cyan);
        button->setTooltip ("Tampilkan preset kategori " + name + ".");
        button->onClick = [this, i] { selectCategory (i); };
        categoryPanel.addAndMakeVisible (*button);
        categoryButtons[(size_t) i] = std::move (button);
    }

    configureLabel (countLabel, 10.5f, theme::textDim);
    countLabel.setJustificationType (juce::Justification::centredRight);

    listBox.setRowHeight (30);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    listBox.setColour (juce::ListBox::outlineColourId, theme::border);
    listBox.setOutlineThickness (1);
    listBox.setModel (this);
    listBox.setTooltip ("Klik dua kali untuk memuat preset.");

    configureLabel (selectionLabel, 15.0f, theme::text, true);
    configureLabel (selectionMeta, 10.0f, theme::textDim);
    selectionMeta.setJustificationType (juce::Justification::topLeft);
    selectionMeta.setMinimumHorizontalScale (0.7f);

    configureLabel (statusLabel, 10.0f, theme::green);
    statusLabel.setJustificationType (juce::Justification::topLeft);

    styleActionButton (loadButton, theme::green);
    loadButton.onClick = [this] { loadSelected(); };
    styleActionButton (saveButton, theme::cyan);
    saveButton.onClick = [this] { saveNewPreset(); };
    styleActionButton (duplicateButton, theme::amber);
    duplicateButton.onClick = [this] { duplicateSelected(); };
    styleActionButton (renameButton, theme::cyan);
    renameButton.onClick = [this] { renameSelected(); };
    styleActionButton (deleteButton, theme::red);
    deleteButton.onClick = [this] { deleteSelected(); };

    configureLabel (formatLabel, 10.0f, theme::textDim);
    configureLabel (rootLabel, 10.0f, theme::textDim);
    rootLabel.setMinimumHorizontalScale (0.7f);
    configureLabel (noteLabel, 9.8f, theme::textDim.withAlpha (0.9f));
    noteLabel.setJustificationType (juce::Justification::topLeft);
    noteLabel.setMinimumHorizontalScale (0.75f);
    noteLabel.setText ("Preset Vocal mencakup parameter vokal dan pitch correction; Music mencakup booster "
                       "dan pitch; Scene mencakup semua group audio. Scene user juga menyimpan chain VST dan "
                       "state sampler. Plugin yang tidak terpasang dilaporkan saat preset dimuat. Scene factory "
                       "tidak menyentuh chain VST. Pan dan FX send belum didukung engine, jadi tidak disimpan.",
                       juce::dontSendNotification);

    formatLabel.setText ("Format: JSON .rrpreset versi " + juce::String (PresetManager::formatVersion)
                             + " - factory tertanam di aplikasi, preset user disimpan sebagai file lokal.",
                         juce::dontSendNotification);

    const auto root = app.presets.getRootFolder();
    rootLabel.setText ("Folder: " + root.getFullPathName(), juce::dontSendNotification);
    rootLabel.setTooltip (root.getFullPathName());

    styleActionButton (openFolderButton, theme::magenta);
    openFolderButton.onClick = [this]
    {
        const auto folder = app.presets.getRootFolder();
        folder.createDirectory();
        folder.revealToUser();
    };

    listPanel.addAndMakeVisible (countLabel);
    listPanel.addAndMakeVisible (listBox);

    actionPanel.addAndMakeVisible (selectionLabel);
    actionPanel.addAndMakeVisible (selectionMeta);
    actionPanel.addAndMakeVisible (loadButton);
    actionPanel.addAndMakeVisible (saveButton);
    actionPanel.addAndMakeVisible (duplicateButton);
    actionPanel.addAndMakeVisible (renameButton);
    actionPanel.addAndMakeVisible (deleteButton);
    actionPanel.addAndMakeVisible (statusLabel);

    infoPanel.addAndMakeVisible (formatLabel);
    infoPanel.addAndMakeVisible (rootLabel);
    infoPanel.addAndMakeVisible (openFolderButton);
    infoPanel.addAndMakeVisible (noteLabel);

    addAndMakeVisible (categoryPanel);
    addAndMakeVisible (listPanel);
    addAndMakeVisible (actionPanel);
    addAndMakeVisible (infoPanel);

    app.presets.addChangeListener (this);

    selectCategory (0);
    updateButtonStates();
}

PresetsPage::~PresetsPage()
{
    app.presets.removeChangeListener (this);
    listBox.setModel (nullptr);
}

void PresetsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void PresetsPage::resized()
{
    auto area = getLocalBounds().reduced (8);

    categoryPanel.setBounds (area.removeFromTop (80));
    area.removeFromTop (8);

    infoPanel.setBounds (area.removeFromBottom (188));
    area.removeFromBottom (8);

    const int listWidth = juce::jmax (300, (int) (area.getWidth() * 0.48f));
    listPanel.setBounds (area.removeFromLeft (listWidth));
    area.removeFromLeft (8);
    actionPanel.setBounds (area);

    auto categoryArea = categoryPanel.getContentBounds();
    const int buttonsWidth = (categoryArea.getWidth() - 5 * 6) / 6;

    for (int i = 0; i < (int) categoryButtons.size(); ++i)
    {
        categoryButtons[(size_t) i]->setBounds (categoryArea.removeFromLeft (buttonsWidth));

        if (i < (int) categoryButtons.size() - 1)
            categoryArea.removeFromLeft (6);
    }

    auto listArea = listPanel.getContentBounds();
    countLabel.setBounds (listArea.removeFromTop (18));
    listArea.removeFromTop (4);
    listBox.setBounds (listArea);

    auto actionArea = actionPanel.getContentBounds();
    selectionLabel.setBounds (actionArea.removeFromTop (24));
    selectionMeta.setBounds (actionArea.removeFromTop (32));
    actionArea.removeFromTop (6);

    const int buttonHeight = 30;
    loadButton.setBounds (actionArea.removeFromTop (buttonHeight));
    actionArea.removeFromTop (6);
    saveButton.setBounds (actionArea.removeFromTop (buttonHeight));
    actionArea.removeFromTop (6);
    duplicateButton.setBounds (actionArea.removeFromTop (buttonHeight));
    actionArea.removeFromTop (6);
    renameButton.setBounds (actionArea.removeFromTop (buttonHeight));
    actionArea.removeFromTop (6);
    deleteButton.setBounds (actionArea.removeFromTop (buttonHeight));
    actionArea.removeFromTop (8);
    statusLabel.setBounds (actionArea);

    auto infoArea = infoPanel.getContentBounds();
    formatLabel.setBounds (infoArea.removeFromTop (16));
    rootLabel.setBounds (infoArea.removeFromTop (18));
    infoArea.removeFromTop (4);

    auto folderRow = infoArea.removeFromTop (24);
    openFolderButton.setBounds (folderRow.removeFromLeft (juce::jmin (180, folderRow.getWidth())));

    infoArea.removeFromTop (4);
    noteLabel.setBounds (infoArea);
}

int PresetsPage::getNumRows()
{
    return names.size();
}

void PresetsPage::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                                    bool rowIsSelected)
{
    if (! juce::isPositiveAndBelow (rowNumber, names.size()))
        return;

    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 2);

    g.setColour (rowIsSelected ? theme::cyan.withAlpha (0.18f)
                               : rowNumber % 2 == 0 ? theme::panelAlt.withAlpha (0.5f)
                                                    : juce::Colours::transparentBlack);
    g.fillRoundedRectangle (area.toFloat(), 5.0f);

    if (rowIsSelected)
    {
        g.setColour (theme::cyan.withAlpha (0.7f));
        g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 5.0f, 1.0f);
    }

    const auto name = names[rowNumber];
    const bool factory = app.presets.isFactoryPreset (currentCategory(), name);

    auto textArea = area.reduced (8, 0);
    auto badge = textArea.removeFromRight (66);

    g.setColour (rowIsSelected ? theme::text : theme::text.withAlpha (0.85f));
    g.setFont (theme::font (12.0f, rowIsSelected));
    g.drawText (name, textArea, juce::Justification::centredLeft, true);

    g.setColour (factory ? theme::textDim : theme::green);
    g.setFont (theme::monoFont (9.0f));
    g.drawText (factory ? "FACTORY" : "USER", badge, juce::Justification::centredRight, true);
}

void PresetsPage::selectedRowsChanged (int lastRowSelected)
{
    juce::ignoreUnused (lastRowSelected);
    updateDetails();
}

void PresetsPage::listBoxItemDoubleClicked (int row, const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

    if (juce::isPositiveAndBelow (row, names.size()))
    {
        listBox.selectRow (row);
        loadSelected();
    }
}

void PresetsPage::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);
    refreshList();
}

void PresetsPage::selectCategory (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presetCategories.size()))
        return;

    categoryIndex = index;

    for (int i = 0; i < (int) categoryButtons.size(); ++i)
        categoryButtons[(size_t) i]->setToggleState (i == index, juce::dontSendNotification);

    statusLabel.setText ({}, juce::dontSendNotification);
    refreshList();
}

void PresetsPage::refreshList()
{
    const auto previous = selectedName();

    names = app.presets.getNames (currentCategory());
    listBox.updateContent();

    const int row = names.indexOf (previous);

    if (row >= 0)
        listBox.selectRow (row);
    else
        listBox.deselectAllRows();

    int factoryCount = 0;

    for (const auto& name : names)
        if (app.presets.isFactoryPreset (currentCategory(), name))
            ++factoryCount;

    countLabel.setText (juce::String (names.size()) + " preset  -  "
                            + juce::String (factoryCount) + " factory  -  "
                            + juce::String (names.size() - factoryCount) + " user",
                        juce::dontSendNotification);

    updateDetails();
}

void PresetsPage::updateDetails()
{
    const auto name = selectedName();

    if (name.isEmpty())
    {
        selectionLabel.setText ("Belum ada preset dipilih", juce::dontSendNotification);
        selectionMeta.setText ("Pilih preset di daftar atau simpan parameter saat ini sebagai preset baru.",
                               juce::dontSendNotification);
        updateButtonStates();
        return;
    }

    selectionLabel.setText (name, juce::dontSendNotification);

    const auto category = currentCategory();
    selectionMeta.setText (juce::String ("Kategori ") + PresetManager::categoryName (category)
                               + "  -  "
                               + (app.presets.isFactoryPreset (category, name)
                                      ? "preset factory (read-only)"
                                      : "preset user: " + app.presets.getFileFor (category, name).getFileName()),
                           juce::dontSendNotification);

    listBox.repaint();
    updateButtonStates();
}

void PresetsPage::updateButtonStates()
{
    const auto name = selectedName();
    const bool hasSelection = name.isNotEmpty();
    const bool factory = selectedIsFactory();

    loadButton.setEnabled (hasSelection);
    saveButton.setEnabled (true);
    duplicateButton.setEnabled (hasSelection);
    renameButton.setEnabled (hasSelection && ! factory);
    deleteButton.setEnabled (hasSelection && ! factory);

    renameButton.setTooltip (hasSelection && factory
                                 ? "Preset factory tidak bisa diganti nama. Duplikat dulu menjadi preset user."
                                 : "Ganti nama preset user.");
    deleteButton.setTooltip (hasSelection && factory
                                 ? "Preset factory tidak bisa dihapus. Duplikat dulu menjadi preset user."
                                 : "Hapus preset user dari disk.");
}

void PresetsPage::loadSelected()
{
    const auto name = selectedName();

    if (name.isEmpty())
        return;

    const auto category = currentCategory();
    juce::String error;
    juce::StringArray warnings;

    juce::MouseCursor::showWaitCursor();
    const bool loaded = app.presets.load (category, name, error, &warnings);
    juce::MouseCursor::hideWaitCursor();

    if (! loaded)
    {
        statusLabel.setColour (juce::Label::textColourId, theme::red);
        statusLabel.setText ("Gagal memuat: " + error, juce::dontSendNotification);
        showWarningMessage ("Preset gagal dimuat", error);
        return;
    }

    statusLabel.setColour (juce::Label::textColourId, theme::green);
    statusLabel.setText ("Dimuat: " + name + " (" + PresetManager::categoryName (category) + ")",
                         juce::dontSendNotification);

    if (! warnings.isEmpty())
        showWarningMessage ("Preset dimuat dengan catatan", warnings.joinIntoString ("\n"));

    updateDetails();
}

void PresetsPage::saveNewPreset()
{
    const auto category = currentCategory();
    const auto initial = selectedName().isNotEmpty() ? selectedName() + " Copy"
                                                     : "My " + PresetManager::categoryName (category);

    promptForText ("Simpan Preset Baru",
                   "Nama preset baru untuk kategori " + PresetManager::categoryName (category) + ":",
                   initial,
                   [this, category] (juce::String newName)
                   {
                       juce::String error;

                       if (! app.presets.save (category, newName.trim(), error))
                       {
                           showWarningMessage ("Preset tidak tersimpan", error);
                           return;
                       }

                       statusLabel.setColour (juce::Label::textColourId, theme::green);
                       statusLabel.setText ("Tersimpan: " + newName.trim(), juce::dontSendNotification);
                       refreshList();

                       const int row = names.indexOf (newName.trim());

                       if (row >= 0)
                           listBox.selectRow (row);
                   });
}

void PresetsPage::duplicateSelected()
{
    const auto name = selectedName();

    if (name.isEmpty())
        return;

    const auto category = currentCategory();

    promptForText ("Duplikat Preset",
                   "Nama untuk salinan \"" + name + "\":",
                   name + " Copy",
                   [this, category, name] (juce::String newName)
                   {
                       juce::String error;

                       if (! app.presets.duplicate (category, name, newName.trim(), error))
                       {
                           showWarningMessage ("Preset tidak diduplikasi", error);
                           return;
                       }

                       statusLabel.setColour (juce::Label::textColourId, theme::green);
                       statusLabel.setText ("Duplikat dibuat: " + newName.trim(), juce::dontSendNotification);
                       refreshList();

                       const int row = names.indexOf (newName.trim());

                       if (row >= 0)
                           listBox.selectRow (row);
                   });
}

void PresetsPage::renameSelected()
{
    const auto name = selectedName();

    if (name.isEmpty() || selectedIsFactory())
        return;

    const auto category = currentCategory();

    promptForText ("Ganti Nama Preset",
                   "Nama baru untuk \"" + name + "\":",
                   name,
                   [this, category, name] (juce::String newName)
                   {
                       juce::String error;

                       if (! app.presets.rename (category, name, newName.trim(), error))
                       {
                           showWarningMessage ("Nama preset tidak berubah", error);
                           return;
                       }

                       statusLabel.setColour (juce::Label::textColourId, theme::green);
                       statusLabel.setText ("Nama diubah: " + newName.trim(), juce::dontSendNotification);
                       refreshList();

                       const int row = names.indexOf (newName.trim());

                       if (row >= 0)
                           listBox.selectRow (row);
                   });
}

void PresetsPage::deleteSelected()
{
    const auto name = selectedName();

    if (name.isEmpty() || selectedIsFactory())
        return;

    const auto category = currentCategory();

    showConfirm ("Hapus Preset",
                 "Preset user \"" + name + "\" akan dihapus dari disk. Tindakan ini tidak bisa dibatalkan.",
                 "Hapus",
                 [this, category, name]
                 {
                     juce::String error;

                     if (! app.presets.remove (category, name, error))
                     {
                         showWarningMessage ("Preset tidak dihapus", error);
                         return;
                     }

                     statusLabel.setColour (juce::Label::textColourId, theme::green);
                     statusLabel.setText ("Dihapus: " + name, juce::dontSendNotification);
                     refreshList();
                 });
}

juce::String PresetsPage::selectedName() const
{
    const int row = listBox.getSelectedRow();

    if (juce::isPositiveAndBelow (row, names.size()))
        return names[row];

    return {};
}

bool PresetsPage::selectedIsFactory() const
{
    const auto name = selectedName();
    return name.isNotEmpty() && app.presets.isFactoryPreset (currentCategory(), name);
}

PresetManager::Category PresetsPage::currentCategory() const
{
    return presetCategories[(size_t) juce::jlimit (0, (int) presetCategories.size() - 1, categoryIndex)];
}

} // namespace rr
