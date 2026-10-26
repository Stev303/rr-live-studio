#pragma once

#include "App/AppState.h"
#include "UI/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace rr
{
class VST3Page : public juce::Component,
                 private juce::Timer,
                 private juce::ChangeListener,
                 private juce::ListBoxModel
{
public:
    explicit VST3Page (AppState& state);
    ~VST3Page() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class ChainPanel;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height,
                           bool rowIsSelected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void returnKeyPressed (int lastRowSelected) override;

    void chooseFolder();
    void startScan();
    void rebuildKnownList();
    void updateSelectionUi();
    void addSelectedToChain (bool masterChain);
    void showFailedFiles();

    AppState& app;
    PluginHost& host;

    Panel scanPanel { "Pemindaian VST3", theme::magenta };
    Panel knownPanel { "Plugin VST3 Terdeteksi", theme::cyan };
    std::unique_ptr<ChainPanel> vocalChainPanel;
    std::unique_ptr<ChainPanel> masterChainPanel;

    juce::Label folderLabel;
    juce::TextButton chooseFolderButton { "Ubah Folder" };
    juce::TextButton scanButton { "Scan Sekarang" };
    juce::TextButton failedFilesButton { "File Gagal" };
    double scanProgress = 0.0;
    juce::ProgressBar progressBar;
    juce::Label scanStatusLabel;
    juce::Label warningLabel;

    juce::TextEditor searchEditor;
    juce::ListBox knownList;
    juce::TextButton addVocalButton { "Tambah ke Vocal" };
    juce::TextButton addMasterButton { "Tambah ke Master" };
    juce::Label selectionLabel;

    juce::Array<juce::PluginDescription> filteredPlugins;
    std::unique_ptr<juce::FileChooser> folderChooser;
    bool wasScanning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST3Page)
};

} // namespace rr
