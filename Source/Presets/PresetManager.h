#pragma once

#include "Audio/ParamStore.h"
#include "Sampler/SamplerEngine.h"
#include "VST3Host/PluginHost.h"
#include <juce_events/juce_events.h>

namespace rr
{
/** Versioned local preset / scene storage.

    Presets are JSON files under the user's application data folder.  Factory
    presets are built into the binary and can be shadowed by a user preset with
    the same name.  Factory scenes never touch VST chains; user scenes save and
    restore both chains, and report plugins that are not installed.
*/
class PresetManager : public juce::ChangeBroadcaster
{
public:
    enum class Category
    {
        Vocal,
        Music,
        Harmonizer,
        Master,
        Sampler,
        Scene
    };

    static constexpr int formatVersion = 1;

    PresetManager (ParamStore& parameters, SamplerEngine& samplerEngine, PluginHost& pluginHost);
    ~PresetManager() override = default;

    void initialise();

    static juce::String categoryName (Category category);
    static juce::String categoryFolder (Category category);
    static juce::StringArray getFactoryPresetNames (Category category);

    juce::StringArray getNames (Category category) const;
    bool isFactoryPreset (Category category, const juce::String& name) const;

    bool load (Category category, const juce::String& name,
               juce::String& errorMessage, juce::StringArray* warnings = nullptr);
    bool save (Category category, const juce::String& name, juce::String& errorMessage);
    bool remove (Category category, const juce::String& name, juce::String& errorMessage);
    bool rename (Category category, const juce::String& oldName, const juce::String& newName,
                 juce::String& errorMessage);
    bool duplicate (Category category, const juce::String& sourceName, const juce::String& newName,
                    juce::String& errorMessage);

    juce::File getRootFolder() const { return rootFolder; }
    juce::File getFileFor (Category category, const juce::String& name) const;

private:
    juce::var buildPreset (Category category, const juce::String& name) const;
    bool applyPresetData (Category category, const juce::var& root,
                          juce::String& errorMessage, juce::StringArray* warnings);
    bool applyParams (Category category, const juce::var& paramsVar,
                      juce::StringArray* warnings);
    bool applyFactoryPreset (Category category, const juce::String& name,
                             juce::String& errorMessage);
    bool writeJsonFile (const juce::File& file, const juce::var& data, juce::String& errorMessage) const;
    static bool validateName (const juce::String& name, juce::String& errorMessage);

    ParamStore& params;
    SamplerEngine& sampler;
    PluginHost& plugins;
    juce::File rootFolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace rr
