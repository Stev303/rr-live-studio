#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <memory>
#include <vector>

namespace rr
{
/** VST3 host for the vocal and master chains.

    Plugins run in-process.  A bad plugin can therefore crash or hang the host,
    which is stated in the UI; the scanner isolates failed loads but cannot
    guarantee compatibility with arbitrary third-party code.

    Chain mutations take a spin lock that the audio thread only ever *tries* to
    take.  If a mutation is in progress the audio thread simply passes audio
    through for that block instead of blocking the real-time thread.
*/
class PluginHost : public juce::ChangeBroadcaster
{
public:
    static constexpr int maxSlotsPerChain = 8;

    struct SlotInfo
    {
        juce::String name;
        juce::String vendor;
        juce::String format;
        bool bypassed = false;
        bool hasEditor = false;
    };

    PluginHost();
    ~PluginHost() override;

    void prepare (double newSampleRate, int maximumBlockSize);
    void releaseResources();

    //==========================================================================
    juce::File getScanFolder() const;
    void setScanFolder (const juce::File& folder);
    void scanFolderAsync (const juce::File& folder);
    bool isScanning() const;
    float getScanProgress() const;
    juce::String getScanStatus() const;
    juce::StringArray getFailedScanFiles() const;
    juce::Array<juce::PluginDescription> getKnownPlugins() const;

    static juce::File getDefaultVST3Folder();

    //==========================================================================
    bool addPlugin (bool masterChain, const juce::PluginDescription& description, juce::String& errorMessage);
    bool removePlugin (bool masterChain, int index);
    bool movePlugin (bool masterChain, int fromIndex, int toIndex);
    void setPluginBypass (bool masterChain, int index, bool shouldBypass);
    bool openPluginEditor (bool masterChain, int index, juce::String& errorMessage);
    void closeAllEditors();
    void clearChain (bool masterChain);

    int getChainSize (bool masterChain) const;
    std::vector<SlotInfo> getChainInfo (bool masterChain) const;
    int getTotalLatencySamples() const noexcept { return publishedLatency.load (std::memory_order_relaxed); }
    int getChainLatencySamples (bool masterChain) const noexcept
    {
        return (masterChain ? masterLatency : vocalLatency).load (std::memory_order_relaxed);
    }
    juce::String getLastError() const;

    void processVocal (float* left, float* right, int numSamples) noexcept;
    void processMaster (float* left, float* right, int numSamples) noexcept;

    juce::var saveChain (bool masterChain) const;
    bool restoreChain (bool masterChain, const juce::var& state, juce::String& errorMessage);

private:
    struct Slot;
    struct ScanContext;
    class PluginEditorWindow;
    class Scanner;

    void processChain (bool masterChain, float* left, float* right, int numSamples) noexcept;
    void prepareSlot (Slot& slot);
    void recalculateLatency();
    bool instantiatePlugin (const juce::PluginDescription& description,
                            Slot& destination, juce::String& errorMessage);
    void initialiseKnownPluginsFile();
    void saveKnownPlugins();
    void scanCompleted();
    juce::AudioPluginFormat* getVST3Format();
    void editorWindowClosed (PluginEditorWindow* window);
    void closeEditorWindowsForSlot (Slot* slot);
    void closeEditorWindowsForChain (bool masterChain);
    static juce::File getKnownPluginsFile();

    juce::AudioPluginFormatManager formatManager;
    std::shared_ptr<ScanContext> scanContext;
    std::unique_ptr<Scanner> scanner;
    std::vector<std::unique_ptr<Slot>> vocalSlots;
    std::vector<std::unique_ptr<Slot>> masterSlots;
    std::vector<std::unique_ptr<PluginEditorWindow>> editorWindows;
    mutable juce::SpinLock chainLock;
    juce::File scanFolder;
    std::vector<float> monoScratch;

    double sampleRate = 48000.0;
    int maximumBlockSize = 512;
    std::atomic<int> publishedLatency { 0 };
    std::atomic<int> vocalLatency { 0 };
    std::atomic<int> masterLatency { 0 };
    mutable juce::CriticalSection errorLock;
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHost)
};

} // namespace rr
