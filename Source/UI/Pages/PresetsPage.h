#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

namespace rr
{
class PresetsPage : public juce::Component,
                    private juce::ChangeListener,
                    private juce::ListBoxModel
{
public:
    explicit PresetsPage (AppState& state);
    ~PresetsPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& event) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void selectCategory (int index);
    void refreshList();
    void updateDetails();
    void updateButtonStates();
    void loadSelected();
    void saveNewPreset();
    void duplicateSelected();
    void renameSelected();
    void deleteSelected();

    juce::String selectedName() const;
    bool selectedIsFactory() const;
    PresetManager::Category currentCategory() const;

    AppState& app;
    int categoryIndex = 0;
    juce::StringArray names;

    Panel categoryPanel { "Kategori Preset", theme::cyan };
    Panel listPanel { "Daftar Preset", theme::green };
    Panel actionPanel { "Aksi", theme::amber };
    Panel infoPanel { "Format, Lokasi, dan Catatan", theme::magenta };

    std::array<std::unique_ptr<juce::TextButton>, 6> categoryButtons;
    juce::Label countLabel;
    juce::ListBox listBox;
    juce::Label selectionLabel;
    juce::Label selectionMeta;
    juce::Label statusLabel;

    juce::TextButton loadButton { "Muat / Terapkan" };
    juce::TextButton saveButton { "Simpan Baru..." };
    juce::TextButton duplicateButton { "Duplikat..." };
    juce::TextButton renameButton { "Ganti Nama..." };
    juce::TextButton deleteButton { "Hapus" };

    juce::Label formatLabel;
    juce::Label rootLabel;
    juce::Label noteLabel;
    juce::TextButton openFolderButton { "Buka Folder Preset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetsPage)
};

} // namespace rr
