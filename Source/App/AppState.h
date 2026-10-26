#pragma once

#include "AI/HarmonyAssistant.h"
#include "Audio/AudioEngine.h"
#include "Audio/ParamStore.h"
#include "Presets/PresetManager.h"
#include "Sampler/SamplerEngine.h"
#include "Utilities/DiagnosticsLog.h"
#include "VST3Host/PluginHost.h"

#include <juce_data_structures/juce_data_structures.h>
#include <functional>

namespace rr
{
/** Shared application state and persistence.

    All UI pages receive a reference to this object.  Parameter changes are
    written straight into the lock-free ParamStore, so the audio engine always
    sees the latest value without any UI-to-audio messaging.
*/
class AppState
{
public:
    AppState();
    ~AppState() = default;

    void initialise();
    void save();

    juce::PropertiesFile* getProperties() const;
    juce::File getDataFolder() const { return dataFolder; }

    ParamStore params;
    SamplerEngine sampler;
    PluginHost pluginHost;
    DiagnosticsLog diagnostics;
    AudioEngine engine;
    PresetManager presets;
    HarmonyAssistant harmonyAssistant;

    juce::String getDeviceInitError() const { return deviceInitError; }
    juce::String getStartupWarning() const { return startupWarning; }

    juce::String getMode() const;
    void setMode (const juce::String& newMode, juce::String& errorMessage);

    int getCurrentPage() const;
    void setCurrentPage (int pageIndex);

    void navigateTo (int pageIndex);
    std::function<void (int)> onNavigate;

    static juce::StringArray getModeNames();

private:
    void applyLatencyMode (const juce::String& modeName, juce::String& errorMessage);
    void restoreSession();
    void saveSession();

    mutable juce::ApplicationProperties applicationProperties;
    juce::File dataFolder;
    juce::String deviceInitError;
    juce::String startupWarning;
    int currentPage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppState)
};

} // namespace rr
