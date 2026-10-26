#include "VST3Host/PluginHost.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

namespace rr
{
namespace
{
    juce::File getAppDataFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("Rr Live Studio");
    }

    juce::String slotDisplayName (const juce::PluginDescription& description)
    {
        return description.name.isNotEmpty() ? description.name
                                             : description.fileOrIdentifier;
    }
}

//==============================================================================
struct PluginHost::Slot
{
    juce::PluginDescription description;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::MidiBuffer midi;
    bool bypassed = false;
    juce::String displayName;
};

struct PluginHost::ScanContext
{
    juce::KnownPluginList knownPlugins;
    juce::File deadMansPedalFile;
    juce::StringArray failedFiles;
    juce::String status;
    mutable juce::CriticalSection lock;
    std::atomic<bool> scanning { false };
    std::atomic<float> progress { 0.0f };
};

//==============================================================================
class PluginHost::PluginEditorWindow final : public juce::DocumentWindow
{
public:
    PluginEditorWindow (PluginHost& ownerToUse, Slot& slotToUse, int chainIndexToUse, bool isMasterChain)
        : juce::DocumentWindow (slotToUse.displayName, juce::Colours::black,
                                juce::DocumentWindow::closeButton, true),
          owner (ownerToUse), slot (&slotToUse), chainIndex (chainIndexToUse), master (isMasterChain)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);
        setResizeLimits (320, 180, 4096, 2160);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
        owner.editorWindowClosed (this);
    }

    Slot* getSlot() const noexcept { return slot; }
    int getChainIndex() const noexcept { return chainIndex; }
    bool isMasterChain() const noexcept { return master; }

private:
    PluginHost& owner;
    Slot* slot;
    int chainIndex;
    bool master;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditorWindow)
};

//==============================================================================
class PluginHost::Scanner final : public juce::Thread
{
public:
    Scanner (PluginHost& ownerToUse, std::shared_ptr<ScanContext> contextToUse, juce::File folder)
        : juce::Thread ("VST3 Scanner"), owner (ownerToUse),
          context (std::move (contextToUse)), scanFolder (std::move (folder))
    {
        setPriority (juce::Thread::Priority::low);
    }

    void run() override
    {
        auto* format = owner.getVST3Format();

        if (format == nullptr)
        {
            {
                const juce::ScopedLock lock (context->lock);
                context->status = "Format VST3 tidak tersedia di build ini.";
            }

            finish();
            return;
        }

        if (! scanFolder.isDirectory())
        {
            {
                const juce::ScopedLock lock (context->lock);
                context->status = "Folder VST3 tidak ditemukan: " + scanFolder.getFullPathName();
            }

            finish();
            return;
        }

        juce::FileSearchPath paths;
        paths.add (scanFolder.getFullPathName());

        juce::PluginDirectoryScanner directoryScanner (context->knownPlugins,
                                                        *format,
                                                        paths,
                                                        true,
                                                        context->deadMansPedalFile,
                                                        true);

        juce::String currentName;

        while (! threadShouldExit())
        {
            // KnownPluginList is mutated by the scanner; the same lock guards the
            // copy that the UI reads from getKnownPlugins().
            const juce::ScopedLock lock (context->lock);
            const bool hasMore = directoryScanner.scanNextFile (true, currentName);
            const auto progress = juce::jlimit (0.0f, 0.99f, directoryScanner.getProgress());

            context->progress.store (progress, std::memory_order_relaxed);
            context->failedFiles = directoryScanner.getFailedFiles();
            context->status = hasMore ? ("Memindai: " + currentName)
                                      : "Selesai memindai VST3.";

            if (! hasMore)
                break;
        }

        finish();
    }

private:
    void finish()
    {
        {
            const juce::ScopedLock lock (context->lock);
            context->progress.store (1.0f, std::memory_order_relaxed);
            context->scanning.store (false, std::memory_order_relaxed);
        }

        owner.scanCompleted();
    }

    PluginHost& owner;
    std::shared_ptr<ScanContext> context;
    juce::File scanFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Scanner)
};

//==============================================================================
PluginHost::PluginHost()
    : scanContext (std::make_shared<ScanContext>())
{
    formatManager.addDefaultFormats();

    scanContext->deadMansPedalFile = getKnownPluginsFile().getSiblingFile ("vst3-dead-mans-pedal.txt");
    scanFolder = getDefaultVST3Folder();

    if (auto* format = getVST3Format())
        scanContext->status = "Siap memindai " + format->getName() + " di " + scanFolder.getFullPathName();
    else
        scanContext->status = "Format VST3 tidak tersedia.";

    initialiseKnownPluginsFile();
}

PluginHost::~PluginHost()
{
    if (scanner != nullptr)
    {
        scanner->signalThreadShouldExit();
        scanner->notify();
        scanner->waitForThreadToExit (10000);
        scanner.reset();
    }

    closeAllEditors();

    const juce::SpinLock::ScopedLockType lock (chainLock);
    vocalSlots.clear();
    masterSlots.clear();
}

void PluginHost::prepare (double newSampleRate, int newBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maximumBlockSize = juce::jmax (32, newBlockSize);

    monoScratch.assign ((size_t) juce::jmax (maximumBlockSize, 4096), 0.0f);

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);

        for (auto& slot : vocalSlots)
            prepareSlot (*slot);

        for (auto& slot : masterSlots)
            prepareSlot (*slot);
    }

    recalculateLatency();
}

void PluginHost::releaseResources()
{
    const juce::SpinLock::ScopedLockType lock (chainLock);

    for (auto& slot : vocalSlots)
        if (slot->instance != nullptr)
            slot->instance->releaseResources();

    for (auto& slot : masterSlots)
        if (slot->instance != nullptr)
            slot->instance->releaseResources();
}

//==============================================================================
juce::File PluginHost::getDefaultVST3Folder()
{
    auto common = juce::SystemStats::getEnvironmentVariable ("CommonProgramFiles",
                                                             "C:\\Program Files\\Common Files");
    return juce::File (common).getChildFile ("VST3");
}

juce::File PluginHost::getKnownPluginsFile()
{
    return getAppDataFolder().getChildFile ("vst3-known-plugins.xml");
}

void PluginHost::initialiseKnownPluginsFile()
{
    const auto file = getKnownPluginsFile();

    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        const juce::ScopedLock lock (scanContext->lock);
        scanContext->knownPlugins.recreateFromXml (*xml);
    }
}

void PluginHost::saveKnownPlugins()
{
    const juce::ScopedLock lock (scanContext->lock);

    if (auto xml = scanContext->knownPlugins.createXml())
    {
        const auto file = getKnownPluginsFile();
        file.getParentDirectory().createDirectory();
        xml->writeTo (file);
    }
}

juce::File PluginHost::getScanFolder() const
{
    return scanFolder;
}

void PluginHost::setScanFolder (const juce::File& folder)
{
    if (folder != scanFolder)
    {
        scanFolder = folder;
        sendChangeMessage();
    }
}

void PluginHost::scanFolderAsync (const juce::File& folder)
{
    if (isScanning())
        return;

    setScanFolder (folder);

    {
        const juce::ScopedLock lock (scanContext->lock);
        scanContext->status = "Menyiapkan scan VST3...";
        scanContext->failedFiles.clear();
        scanContext->progress.store (0.0f, std::memory_order_relaxed);
        scanContext->scanning.store (true, std::memory_order_relaxed);
    }

    scanner = std::make_unique<Scanner> (*this, scanContext, scanFolder);
    scanner->startThread();
    sendChangeMessage();
}

bool PluginHost::isScanning() const
{
    return scanContext->scanning.load (std::memory_order_relaxed);
}

float PluginHost::getScanProgress() const
{
    return scanContext->progress.load (std::memory_order_relaxed);
}

juce::String PluginHost::getScanStatus() const
{
    const juce::ScopedLock lock (scanContext->lock);
    return scanContext->status;
}

juce::StringArray PluginHost::getFailedScanFiles() const
{
    const juce::ScopedLock lock (scanContext->lock);
    return scanContext->failedFiles;
}

juce::Array<juce::PluginDescription> PluginHost::getKnownPlugins() const
{
    const juce::ScopedLock lock (scanContext->lock);
    return scanContext->knownPlugins.getTypes();
}

juce::AudioPluginFormat* PluginHost::getVST3Format()
{
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        if (auto* format = formatManager.getFormat (i))
            if (format->getName().containsIgnoreCase ("VST3"))
                return format;
    }

    return nullptr;
}

void PluginHost::scanCompleted()
{
    saveKnownPlugins();
    sendChangeMessage();
}

//==============================================================================
void PluginHost::prepareSlot (Slot& slot)
{
    if (slot.instance == nullptr)
        return;

    slot.instance->setPlayConfigDetails (2, 2, sampleRate, maximumBlockSize);
    slot.instance->prepareToPlay (sampleRate, maximumBlockSize);
}

bool PluginHost::instantiatePlugin (const juce::PluginDescription& description,
                                    Slot& destination,
                                    juce::String& errorMessage)
{
    juce::String creationError;
    auto instance = formatManager.createPluginInstance (description, sampleRate,
                                                        maximumBlockSize, creationError);

    if (instance == nullptr)
    {
        errorMessage = "Gagal memuat plugin \"" + slotDisplayName (description) + "\": "
                     + (creationError.isNotEmpty() ? creationError : juce::String ("error tidak diketahui."));
        return false;
    }

    destination.description = description;
    destination.displayName = slotDisplayName (description);
    destination.instance = std::move (instance);
    prepareSlot (destination);

    return true;
}

bool PluginHost::addPlugin (bool masterChain, const juce::PluginDescription& description,
                            juce::String& errorMessage)
{
    if (getChainSize (masterChain) >= maxSlotsPerChain)
    {
        errorMessage = "Batas " + juce::String (maxSlotsPerChain) + " plugin per chain sudah tercapai.";
        return false;
    }

    auto slot = std::make_unique<Slot>();

    if (! instantiatePlugin (description, *slot, errorMessage))
        return false;

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        (masterChain ? masterSlots : vocalSlots).push_back (std::move (slot));
    }

    recalculateLatency();
    sendChangeMessage();
    return true;
}

bool PluginHost::removePlugin (bool masterChain, int index)
{
    Slot* target = nullptr;

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        auto& slots = masterChain ? masterSlots : vocalSlots;

        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return false;

        target = slots[(size_t) index].get();
    }

    closeEditorWindowsForSlot (target);

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        auto& slots = masterChain ? masterSlots : vocalSlots;

        if (! juce::isPositiveAndBelow (index, (int) slots.size())
            || slots[(size_t) index].get() != target)
            return false;

        slots.erase (slots.begin() + index);
    }

    recalculateLatency();
    sendChangeMessage();
    return true;
}

bool PluginHost::movePlugin (bool masterChain, int fromIndex, int toIndex)
{
    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        auto& slots = masterChain ? masterSlots : vocalSlots;
        const auto size = (int) slots.size();

        if (! juce::isPositiveAndBelow (fromIndex, size)
            || ! juce::isPositiveAndBelow (toIndex, size)
            || fromIndex == toIndex)
            return false;

        auto moved = std::move (slots[(size_t) fromIndex]);
        slots.erase (slots.begin() + fromIndex);
        slots.insert (slots.begin() + toIndex, std::move (moved));
    }

    sendChangeMessage();
    return true;
}

void PluginHost::setPluginBypass (bool masterChain, int index, bool shouldBypass)
{
    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        auto& slots = masterChain ? masterSlots : vocalSlots;

        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
            return;

        slots[(size_t) index]->bypassed = shouldBypass;
    }

    recalculateLatency();
    sendChangeMessage();
}

void PluginHost::clearChain (bool masterChain)
{
    closeEditorWindowsForChain (masterChain);

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        (masterChain ? masterSlots : vocalSlots).clear();
    }

    recalculateLatency();
    sendChangeMessage();
}

int PluginHost::getChainSize (bool masterChain) const
{
    const juce::SpinLock::ScopedLockType lock (chainLock);
    return (int) (masterChain ? masterSlots : vocalSlots).size();
}

std::vector<PluginHost::SlotInfo> PluginHost::getChainInfo (bool masterChain) const
{
    std::vector<SlotInfo> result;

    const juce::SpinLock::ScopedLockType lock (chainLock);
    const auto& slots = masterChain ? masterSlots : vocalSlots;

    result.reserve (slots.size());

    for (const auto& slot : slots)
    {
        SlotInfo info;
        info.name = slot->displayName;
        info.vendor = slot->description.manufacturerName;
        info.format = slot->description.pluginFormatName;
        info.bypassed = slot->bypassed;
        info.hasEditor = slot->instance != nullptr && slot->instance->hasEditor();
        result.push_back (std::move (info));
    }

    return result;
}

//==============================================================================
void PluginHost::editorWindowClosed (PluginEditorWindow* window)
{
    if (window != nullptr)
        window->setVisible (false);
}

void PluginHost::closeEditorWindowsForSlot (Slot* slot)
{
    for (auto it = editorWindows.begin(); it != editorWindows.end();)
    {
        if ((*it)->getSlot() == slot)
            it = editorWindows.erase (it);
        else
            ++it;
    }
}

void PluginHost::closeEditorWindowsForChain (bool masterChain)
{
    for (auto it = editorWindows.begin(); it != editorWindows.end();)
    {
        if ((*it)->isMasterChain() == masterChain)
            it = editorWindows.erase (it);
        else
            ++it;
    }
}

bool PluginHost::openPluginEditor (bool masterChain, int index, juce::String& errorMessage)
{
    Slot* slot = nullptr;
    int chainIndex = index;

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        auto& slots = masterChain ? masterSlots : vocalSlots;

        if (! juce::isPositiveAndBelow (index, (int) slots.size()))
        {
            errorMessage = "Plugin tidak ditemukan di chain.";
            return false;
        }

        slot = slots[(size_t) index].get();
    }

    for (auto& window : editorWindows)
    {
        if (window->getSlot() == slot)
        {
            window->setVisible (true);
            window->toFront (true);
            return true;
        }
    }

    if (slot->instance == nullptr || ! slot->instance->hasEditor())
    {
        errorMessage = "Plugin \"" + slot->displayName + "\" tidak menyediakan editor.";
        return false;
    }

    auto* editor = slot->instance->createEditorIfNeeded();

    if (editor == nullptr)
    {
        errorMessage = "Editor plugin \"" + slot->displayName + "\" gagal dibuat.";
        return false;
    }

    auto window = std::make_unique<PluginEditorWindow> (*this, *slot, chainIndex, masterChain);
    window->setContentOwned (editor, true);
    window->setResizable (editor->isResizable(), false);
    window->centreWithSize (juce::jmax (editor->getWidth(), 320),
                            juce::jmax (editor->getHeight(), 180));
    window->setVisible (true);
    editorWindows.push_back (std::move (window));

    return true;
}

void PluginHost::closeAllEditors()
{
    editorWindows.clear();
}

void PluginHost::recalculateLatency()
{
    int vocal = 0;
    int master = 0;

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);

        for (const auto& slot : vocalSlots)
            if (! slot->bypassed && slot->instance != nullptr)
                vocal += juce::jmax (0, slot->instance->getLatencySamples());

        for (const auto& slot : masterSlots)
            if (! slot->bypassed && slot->instance != nullptr)
                master += juce::jmax (0, slot->instance->getLatencySamples());
    }

    vocalLatency.store (vocal, std::memory_order_relaxed);
    masterLatency.store (master, std::memory_order_relaxed);
    publishedLatency.store (vocal + master, std::memory_order_relaxed);
}

juce::String PluginHost::getLastError() const
{
    const juce::ScopedLock lock (errorLock);
    return lastError;
}

//==============================================================================
void PluginHost::processVocal (float* left, float* right, int numSamples) noexcept
{
    processChain (false, left, right, numSamples);
}

void PluginHost::processMaster (float* left, float* right, int numSamples) noexcept
{
    processChain (true, left, right, numSamples);
}

void PluginHost::processChain (bool masterChain, float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    const bool mono = right == nullptr;
    float* workingRight = right;

    if (mono)
    {
        if ((size_t) numSamples > monoScratch.size())
            return;

        workingRight = monoScratch.data();
        juce::FloatVectorOperations::copy (workingRight, left, numSamples);
    }

    const juce::SpinLock::ScopedTryLockType lock (chainLock);

    if (! lock.isLocked())
        return;

    float* channels[2] = { left, workingRight };
    juce::AudioBuffer<float> buffer (channels, 2, numSamples);
    juce::MidiBuffer midi;

    const auto& slots = masterChain ? masterSlots : vocalSlots;

    for (const auto& slot : slots)
    {
        if (slot->bypassed || slot->instance == nullptr)
            continue;

        midi.clear();

        try
        {
            slot->instance->processBlock (buffer, midi);
        }
        catch (...)
        {
            // A plugin throwing is broken; bypass it for this block. The error lock is
            // only tried, never waited for, because this runs on the audio thread.
            slot->bypassed = true;
            const juce::ScopedTryLock errorGuard (errorLock);

            if (errorGuard.isLocked())
                lastError = "Plugin \"" + slot->displayName + "\" melempar exception dan dinonaktifkan.";
        }
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto value = data[i];
            data[i] = std::isfinite (value) ? juce::jlimit (-32.0f, 32.0f, value) : 0.0f;
        }
    }

    if (mono)
        juce::FloatVectorOperations::copy (left, buffer.getReadPointer (0), numSamples);
}

//==============================================================================
juce::var PluginHost::saveChain (bool masterChain) const
{
    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    juce::Array<juce::var> plugins;

    const juce::SpinLock::ScopedLockType lock (chainLock);
    const auto& slots = masterChain ? masterSlots : vocalSlots;

    for (const auto& slot : slots)
    {
        auto entry = juce::DynamicObject::Ptr (new juce::DynamicObject());

        if (auto xml = slot->description.createXml())
            entry->setProperty ("descriptionXml", xml->toString (juce::XmlElement::TextFormat().singleLine()));

        entry->setProperty ("bypassed", slot->bypassed);
        entry->setProperty ("name", slot->displayName);

        if (slot->instance != nullptr)
        {
            juce::MemoryBlock state;
            slot->instance->getStateInformation (state);
            entry->setProperty ("state", state.toBase64Encoding());
        }

        plugins.add (juce::var (entry.get()));
    }

    root->setProperty ("version", 1);
    root->setProperty ("plugins", plugins);
    return juce::var (root.get());
}

bool PluginHost::restoreChain (bool masterChain, const juce::var& state, juce::String& errorMessage)
{
    auto* root = state.getDynamicObject();

    if (root == nullptr || ! root->hasProperty ("plugins"))
    {
        errorMessage = "State chain VST3 tidak valid.";
        return false;
    }

    const auto* pluginsVar = root->getProperty ("plugins").getArray();

    if (pluginsVar == nullptr)
    {
        errorMessage = "State chain VST3 tidak berisi daftar plugin.";
        return false;
    }

    clearChain (masterChain);

    juce::StringArray failures;

    for (const auto& entryVar : *pluginsVar)
    {
        auto* entry = entryVar.getDynamicObject();

        if (entry == nullptr)
            continue;

        const auto xmlText = entry->getProperty ("descriptionXml").toString();

        if (xmlText.isEmpty())
        {
            failures.add ("Entri plugin tanpa deskripsi diabaikan.");
            continue;
        }

        auto xml = juce::XmlDocument::parse (xmlText);
        juce::PluginDescription description;

        if (xml == nullptr || ! description.loadFromXml (*xml))
        {
            failures.add ("Deskripsi plugin tidak bisa dibaca.");
            continue;
        }

        auto slot = std::make_unique<Slot>();
        juce::String creationError;

        if (! instantiatePlugin (description, *slot, creationError))
        {
            failures.add (creationError);
            continue;
        }

        slot->bypassed = (bool) entry->getProperty ("bypassed");

        const auto stateBase64 = entry->getProperty ("state").toString();

        if (stateBase64.isNotEmpty())
        {
            juce::MemoryBlock memory;
            memory.fromBase64Encoding (stateBase64);

            if (memory.getSize() > 0)
                slot->instance->setStateInformation (memory.getData(), (int) memory.getSize());
        }

        {
            const juce::SpinLock::ScopedLockType lock (chainLock);
            (masterChain ? masterSlots : vocalSlots).push_back (std::move (slot));
        }
    }

    recalculateLatency();
    sendChangeMessage();

    if (! failures.isEmpty())
    {
        errorMessage = failures.joinIntoString ("\n");
        return false;
    }

    return true;
}

} // namespace rr
