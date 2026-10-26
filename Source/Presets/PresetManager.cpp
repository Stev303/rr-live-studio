#include "Presets/PresetManager.h"

#include <vector>

namespace rr
{
namespace
{
    juce::StringArray makeNameList (std::initializer_list<const char*> names)
    {
        juce::StringArray result;

        for (const auto* name : names)
            result.add (name);

        return result;
    }

    std::vector<ParamGroup> presetGroups (PresetManager::Category category)
    {
        switch (category)
        {
            case PresetManager::Category::Vocal:
                return { ParamGroup::Vocal, ParamGroup::Correction };
            case PresetManager::Category::Music:
                return { ParamGroup::Booster, ParamGroup::Pitch };
            case PresetManager::Category::Harmonizer:
                return { ParamGroup::Harmony };
            case PresetManager::Category::Master:
                return { ParamGroup::Master };
            case PresetManager::Category::Sampler:
                return { ParamGroup::Sampler };
            case PresetManager::Category::Scene:
                return { ParamGroup::Master, ParamGroup::Music, ParamGroup::Booster,
                         ParamGroup::Balance, ParamGroup::Pitch, ParamGroup::Vocal,
                         ParamGroup::Correction, ParamGroup::Harmony, ParamGroup::Sampler };
        }

        return { ParamGroup::Master };
    }

    bool isAllowedParam (PresetManager::Category category, ParamId id)
    {
        const auto group = ParamStore::info (id).group;

        if (group == ParamGroup::Test)
            return false;

        for (const auto allowed : presetGroups (category))
            if (group == allowed)
                return true;

        return false;
    }

    void resetGroupsForCategory (ParamStore& params, PresetManager::Category category)
    {
        for (const auto group : presetGroups (category))
            params.resetGroup (group);
    }

    void writeValues (ParamStore& params, std::initializer_list<std::pair<ParamId, float>> values)
    {
        for (const auto& item : values)
            params.setSilently (item.first, item.second);
    }
}

//==============================================================================
PresetManager::PresetManager (ParamStore& parameters, SamplerEngine& samplerEngine, PluginHost& pluginHost)
    : params (parameters), sampler (samplerEngine), plugins (pluginHost)
{
    rootFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                     .getChildFile ("Rr Live Studio")
                     .getChildFile ("Presets");
}

void PresetManager::initialise()
{
    rootFolder.createDirectory();

    for (const auto category : { Category::Vocal, Category::Music, Category::Harmonizer,
                                 Category::Master, Category::Sampler, Category::Scene })
        rootFolder.getChildFile (categoryFolder (category)).createDirectory();

    sendChangeMessage();
}

juce::String PresetManager::categoryName (Category category)
{
    switch (category)
    {
        case Category::Vocal:      return "Vocal";
        case Category::Music:      return "Music";
        case Category::Harmonizer: return "Harmonizer";
        case Category::Master:     return "Master";
        case Category::Sampler:    return "Sampler";
        case Category::Scene:      return "Scene";
    }

    return {};
}

juce::String PresetManager::categoryFolder (Category category)
{
    return category == Category::Scene ? "Scenes" : categoryName (category);
}

juce::StringArray PresetManager::getFactoryPresetNames (Category category)
{
    switch (category)
    {
        case Category::Vocal:
            return makeNameList ({ "Male Natural", "Female Bright", "Warm", "Clear",
                                   "Live Karaoke", "Clean Studio" });
        case Category::Music:
            return makeNameList ({ "Original", "Karaoke Clear", "Warm Music", "Punchy", "Loud but Safe" });
        case Category::Harmonizer:
            return makeNameList ({ "Third Above", "Fifth + Octave", "Subtle Double", "Wide Choir" });
        case Category::Master:
            return makeNameList ({ "Safe Broadcast", "Loud Live" });
        case Category::Sampler:
            return {};
        case Category::Scene:
            return makeNameList ({ "Pop", "Rock", "Dangdut", "Ballad", "Custom" });
    }

    return {};
}

juce::StringArray PresetManager::getNames (Category category) const
{
    auto names = getFactoryPresetNames (category);

    const auto folder = rootFolder.getChildFile (categoryFolder (category));

    if (folder.isDirectory())
    {
        const auto files = folder.findChildFiles (juce::File::findFiles, false, "*.rrpreset");

        for (const auto& file : files)
        {
            const auto name = file.getFileNameWithoutExtension();

            if (name.isNotEmpty() && ! names.contains (name))
                names.add (name);
        }
    }

    names.sort (true);
    return names;
}

bool PresetManager::isFactoryPreset (Category category, const juce::String& name) const
{
    return getFactoryPresetNames (category).contains (name)
        && ! getFileFor (category, name).existsAsFile();
}

//==============================================================================
bool PresetManager::validateName (const juce::String& name, juce::String& errorMessage)
{
    const auto trimmed = name.trim();

    if (trimmed.isEmpty())
    {
        errorMessage = "Nama preset tidak boleh kosong.";
        return false;
    }

    if (trimmed.length() > 64)
    {
        errorMessage = "Nama preset maksimal 64 karakter.";
        return false;
    }

    if (trimmed.startsWithChar ('.') || trimmed.endsWithChar ('.') || trimmed.endsWithChar (' '))
    {
        errorMessage = "Nama preset tidak boleh diawali/diakhiri titik atau spasi.";
        return false;
    }

    for (const auto character : trimmed)
    {
        if (character < 32 || juce::String ("<>:\"/\\|?*").containsChar (character))
        {
            errorMessage = "Nama preset mengandung karakter yang tidak diperbolehkan.";
            return false;
        }
    }

    return true;
}

juce::File PresetManager::getFileFor (Category category, const juce::String& name) const
{
    return rootFolder.getChildFile (categoryFolder (category))
                     .getChildFile (name.trim() + ".rrpreset");
}

bool PresetManager::writeJsonFile (const juce::File& file, const juce::var& data,
                                   juce::String& errorMessage) const
{
    if (! file.getParentDirectory().createDirectory())
    {
        errorMessage = "Tidak bisa membuat folder preset: " + file.getParentDirectory().getFullPathName();
        return false;
    }

    if (! file.replaceWithText (juce::JSON::toString (data, false)))
    {
        errorMessage = "Gagal menulis preset: " + file.getFullPathName();
        return false;
    }

    return true;
}

//==============================================================================
juce::var PresetManager::buildPreset (Category category, const juce::String& name) const
{
    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("format", "RrLiveStudioPreset");
    root->setProperty ("version", formatVersion);
    root->setProperty ("name", name);
    root->setProperty ("category", categoryFolder (category));
    root->setProperty ("createdAt", juce::Time::getCurrentTime().toISO8601 (true));

    auto paramObject = juce::DynamicObject::Ptr (new juce::DynamicObject());

    for (int i = 0; i < ParamStore::numParams; ++i)
    {
        const auto id = (ParamId) i;

        if (isAllowedParam (category, id))
            paramObject->setProperty (ParamStore::info (id).key, params.get (id));
    }

    root->setProperty ("parameters", juce::var (paramObject.get()));

    if (category == Category::Sampler || category == Category::Scene)
        root->setProperty ("samplerState", sampler.saveState());

    if (category == Category::Vocal || category == Category::Scene)
        root->setProperty ("vocalChain", plugins.saveChain (false));

    if (category == Category::Master || category == Category::Scene)
        root->setProperty ("masterChain", plugins.saveChain (true));

    return juce::var (root.get());
}

bool PresetManager::save (Category category, const juce::String& name, juce::String& errorMessage)
{
    if (! validateName (name, errorMessage))
        return false;

    const auto trimmed = name.trim();
    const auto file = getFileFor (category, trimmed);

    // Saving under a factory name is allowed: the user file shadows the factory
    // preset, which can later be removed to reveal the built-in again.
    if (! writeJsonFile (file, buildPreset (category, trimmed), errorMessage))
        return false;

    sendChangeMessage();
    return true;
}

bool PresetManager::remove (Category category, const juce::String& name, juce::String& errorMessage)
{
    const auto file = getFileFor (category, name);

    if (! file.existsAsFile())
    {
        errorMessage = isFactoryPreset (category, name)
                         ? "Preset bawaan tidak dapat dihapus."
                         : "Preset tidak ditemukan.";
        return false;
    }

    if (! file.deleteFile())
    {
        errorMessage = "Gagal menghapus file preset: " + file.getFullPathName();
        return false;
    }

    sendChangeMessage();
    return true;
}

bool PresetManager::rename (Category category, const juce::String& oldName, const juce::String& newName,
                            juce::String& errorMessage)
{
    const auto sourceFile = getFileFor (category, oldName);

    if (! sourceFile.existsAsFile())
    {
        errorMessage = isFactoryPreset (category, oldName)
                         ? "Preset bawaan tidak dapat diganti nama. Duplikat dulu sebagai preset pengguna."
                         : "Preset tidak ditemukan.";
        return false;
    }

    if (! validateName (newName, errorMessage))
        return false;

    const auto trimmed = newName.trim();

    if (getFactoryPresetNames (category).contains (trimmed))
    {
        errorMessage = "Nama tersebut dipakai preset bawaan. Pilih nama lain.";
        return false;
    }

    const auto destination = getFileFor (category, trimmed);

    if (destination.existsAsFile())
    {
        errorMessage = "Preset dengan nama tersebut sudah ada.";
        return false;
    }

    auto data = juce::JSON::parse (sourceFile);

    if (auto* object = data.getDynamicObject())
        object->setProperty ("name", trimmed);

    if (! writeJsonFile (destination, data, errorMessage))
        return false;

    if (! sourceFile.deleteFile())
    {
        errorMessage = "Preset baru tersimpan, tetapi preset lama gagal dihapus.";
        sendChangeMessage();
        return false;
    }

    sendChangeMessage();
    return true;
}

bool PresetManager::duplicate (Category category, const juce::String& sourceName,
                               const juce::String& newName, juce::String& errorMessage)
{
    if (! validateName (newName, errorMessage))
        return false;

    const auto trimmed = newName.trim();

    if (trimmed == sourceName.trim())
    {
        errorMessage = "Nama duplikat harus berbeda dari nama sumber.";
        return false;
    }

    if (getFactoryPresetNames (category).contains (trimmed))
    {
        errorMessage = "Nama tersebut dipakai preset bawaan. Pilih nama lain.";
        return false;
    }

    const auto destination = getFileFor (category, trimmed);

    if (destination.existsAsFile())
    {
        errorMessage = "Preset dengan nama tersebut sudah ada.";
        return false;
    }

    const auto sourceFile = getFileFor (category, sourceName);

    if (sourceFile.existsAsFile())
    {
        auto data = juce::JSON::parse (sourceFile);

        if (data.isVoid())
        {
            errorMessage = "File preset sumber tidak bisa dibaca.";
            return false;
        }

        if (auto* object = data.getDynamicObject())
            object->setProperty ("name", trimmed);

        if (! writeJsonFile (destination, data, errorMessage))
            return false;
    }
    else if (getFactoryPresetNames (category).contains (sourceName))
    {
        auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
        root->setProperty ("format", "RrLiveStudioPreset");
        root->setProperty ("version", formatVersion);
        root->setProperty ("name", trimmed);
        root->setProperty ("category", categoryFolder (category));
        root->setProperty ("baseFactory", sourceName);
        root->setProperty ("createdAt", juce::Time::getCurrentTime().toISO8601 (true));

        if (! writeJsonFile (destination, juce::var (root.get()), errorMessage))
            return false;
    }
    else
    {
        errorMessage = "Preset sumber tidak ditemukan.";
        return false;
    }

    sendChangeMessage();
    return true;
}

//==============================================================================
bool PresetManager::applyFactoryPreset (Category category, const juce::String& name,
                                        juce::String& errorMessage)
{
    if (! getFactoryPresetNames (category).contains (name))
    {
        errorMessage = "Preset bawaan tidak dikenal: " + name;
        return false;
    }

    const auto savedPcKey = params.getInt (ParamId::pcKey);
    const auto savedPcScale = params.getInt (ParamId::pcScale);
    const auto savedHKey = params.getInt (ParamId::hKey);
    const auto savedHScale = params.getInt (ParamId::hScale);
    const auto savedVideoVolume = params.get (ParamId::youtubeVideoVolume);

    resetGroupsForCategory (params, category);

    if (category == Category::Vocal || category == Category::Scene)
    {
        params.setSilently (ParamId::pcKey, (float) savedPcKey);
        params.setSilently (ParamId::pcScale, (float) savedPcScale);
    }

    if (category == Category::Harmonizer || category == Category::Scene)
    {
        params.setSilently (ParamId::hKey, (float) savedHKey);
        params.setSilently (ParamId::hScale, (float) savedHScale);
    }

    if (category == Category::Scene)
    {
        params.setSilently (ParamId::youtubeVideoVolume, savedVideoVolume);
        params.setSilently (ParamId::testToneEnabled, 0.0f);
        params.setSilently (ParamId::masterLimiterEnabled, 1.0f);
    }

    if (category == Category::Vocal)
    {
        if (name == "Male Natural")
        {
            writeValues (params, {
                { ParamId::micGainDb, 6.0f }, { ParamId::micMixDb, 0.0f }, { ParamId::micMute, 0.0f }, { ParamId::micSolo, 0.0f },
                { ParamId::gateEnabled, 1.0f }, { ParamId::gateThresholdDb, -52.0f },
                { ParamId::hpfEnabled, 1.0f }, { ParamId::hpfHz, 90.0f },
                { ParamId::deEsserEnabled, 1.0f }, { ParamId::deEsserThresholdDb, -30.0f }, { ParamId::deEsserFreqHz, 6300.0f },
                { ParamId::compEnabled, 1.0f }, { ParamId::compThresholdDb, -20.0f }, { ParamId::compRatio, 3.0f },
                { ParamId::compAttackMs, 12.0f }, { ParamId::compReleaseMs, 140.0f }, { ParamId::compMakeupDb, 3.0f },
                { ParamId::eqEnabled, 1.0f }, { ParamId::satEnabled, 0.0f },
                { ParamId::reverbEnabled, 1.0f }, { ParamId::reverbMix, 10.0f }, { ParamId::reverbSize, 45.0f }, { ParamId::reverbDamp, 50.0f },
                { ParamId::delayEnabled, 0.0f }, { ParamId::vocalFaderDb, 0.0f }
            });
        }
        else if (name == "Female Bright")
        {
            writeValues (params, {
                { ParamId::micGainDb, 7.0f }, { ParamId::gateEnabled, 1.0f }, { ParamId::gateThresholdDb, -52.0f },
                { ParamId::hpfEnabled, 1.0f }, { ParamId::hpfHz, 105.0f },
                { ParamId::deEsserEnabled, 1.0f }, { ParamId::deEsserThresholdDb, -34.0f }, { ParamId::deEsserFreqHz, 7200.0f },
                { ParamId::compEnabled, 1.0f }, { ParamId::compThresholdDb, -24.0f }, { ParamId::compRatio, 3.5f },
                { ParamId::compAttackMs, 8.0f }, { ParamId::compReleaseMs, 120.0f }, { ParamId::compMakeupDb, 3.0f },
                { ParamId::eq1Gain, -1.5f }, { ParamId::eq2Gain, -1.0f }, { ParamId::eq4Freq, 3800.0f }, { ParamId::eq4Gain, 1.8f },
                { ParamId::reverbEnabled, 1.0f }, { ParamId::reverbMix, 12.0f }, { ParamId::reverbSize, 48.0f },
                { ParamId::satEnabled, 0.0f }, { ParamId::delayEnabled, 0.0f }, { ParamId::vocalFaderDb, 0.0f }
            });
        }
        else if (name == "Warm")
        {
            writeValues (params, {
                { ParamId::micGainDb, 6.0f }, { ParamId::gateEnabled, 1.0f }, { ParamId::gateThresholdDb, -52.0f },
                { ParamId::hpfEnabled, 1.0f }, { ParamId::hpfHz, 80.0f },
                { ParamId::deEsserEnabled, 1.0f }, { ParamId::deEsserThresholdDb, -30.0f }, { ParamId::deEsserFreqHz, 6000.0f },
                { ParamId::compEnabled, 1.0f }, { ParamId::compThresholdDb, -22.0f }, { ParamId::compRatio, 2.5f },
                { ParamId::compAttackMs, 15.0f }, { ParamId::compReleaseMs, 150.0f }, { ParamId::compMakeupDb, 3.0f },
                { ParamId::eq1Freq, 110.0f }, { ParamId::eq1Gain, 2.0f }, { ParamId::eq4Gain, -1.0f },
                { ParamId::satEnabled, 1.0f }, { ParamId::satDrive, 25.0f }, { ParamId::satMix, 20.0f },
                { ParamId::reverbEnabled, 1.0f }, { ParamId::reverbMix, 14.0f }, { ParamId::reverbSize, 50.0f },
                { ParamId::delayEnabled, 0.0f }, { ParamId::vocalFaderDb, 0.0f }
            });
        }
        else if (name == "Clear")
        {
            writeValues (params, {
                { ParamId::micGainDb, 6.0f }, { ParamId::gateEnabled, 1.0f }, { ParamId::gateThresholdDb, -50.0f },
                { ParamId::hpfEnabled, 1.0f }, { ParamId::hpfHz, 100.0f },
                { ParamId::deEsserEnabled, 1.0f }, { ParamId::deEsserThresholdDb, -32.0f }, { ParamId::deEsserFreqHz, 6800.0f },
                { ParamId::compEnabled, 1.0f }, { ParamId::compThresholdDb, -22.0f }, { ParamId::compRatio, 3.0f },
                { ParamId::compAttackMs, 10.0f }, { ParamId::compReleaseMs, 130.0f }, { ParamId::compMakeupDb, 3.0f },
                { ParamId::eq2Freq, 300.0f }, { ParamId::eq2Gain, -2.5f }, { ParamId::eq4Freq, 3800.0f }, { ParamId::eq4Gain, 1.5f },
                { ParamId::satEnabled, 0.0f }, { ParamId::reverbEnabled, 1.0f }, { ParamId::reverbMix, 8.0f },
                { ParamId::delayEnabled, 0.0f }, { ParamId::vocalFaderDb, 0.0f }
            });
        }
        else if (name == "Live Karaoke")
        {
            writeValues (params, {
                { ParamId::micGainDb, 7.0f }, { ParamId::gateEnabled, 1.0f }, { ParamId::gateThresholdDb, -52.0f },
                { ParamId::hpfEnabled, 1.0f }, { ParamId::hpfHz, 95.0f },
                { ParamId::deEsserEnabled, 1.0f }, { ParamId::deEsserThresholdDb, -32.0f }, { ParamId::deEsserFreqHz, 6500.0f },
                { ParamId::compEnabled, 1.0f }, { ParamId::compThresholdDb, -24.0f }, { ParamId::compRatio, 4.0f },
                { ParamId::compAttackMs, 10.0f }, { ParamId::compReleaseMs, 150.0f }, { ParamId::compMakeupDb, 4.0f },
                { ParamId::eq4Gain, 1.2f }, { ParamId::satEnabled, 0.0f },
                { ParamId::reverbEnabled, 1.0f }, { ParamId::reverbMix, 18.0f }, { ParamId::reverbSize, 55.0f },
                { ParamId::delayEnabled, 1.0f }, { ParamId::delayTimeMs, 240.0f }, { ParamId::delayFeedback, 22.0f }, { ParamId::delayMix, 12.0f },
                { ParamId::vocalFaderDb, 0.0f }
            });
        }
        else if (name == "Clean Studio")
        {
            writeValues (params, {
                { ParamId::micGainDb, 6.0f }, { ParamId::gateEnabled, 1.0f }, { ParamId::gateThresholdDb, -56.0f },
                { ParamId::hpfEnabled, 1.0f }, { ParamId::hpfHz, 95.0f },
                { ParamId::deEsserEnabled, 1.0f }, { ParamId::deEsserThresholdDb, -30.0f }, { ParamId::deEsserFreqHz, 6200.0f },
                { ParamId::compEnabled, 1.0f }, { ParamId::compThresholdDb, -20.0f }, { ParamId::compRatio, 2.5f },
                { ParamId::compAttackMs, 18.0f }, { ParamId::compReleaseMs, 120.0f }, { ParamId::compMakeupDb, 2.0f },
                { ParamId::satEnabled, 0.0f }, { ParamId::reverbEnabled, 1.0f }, { ParamId::reverbMix, 6.0f },
                { ParamId::delayEnabled, 0.0f }, { ParamId::vocalFaderDb, 0.0f }
            });
        }
        else
        {
            errorMessage = "Preset vokal tidak dikenal: " + name;
            return false;
        }
    }
    else if (category == Category::Music)
    {
        writeValues (params, { { ParamId::transpose, 0.0f }, { ParamId::pitchBypass, 0.0f } });

        if (name == "Original")
        {
            writeValues (params, {
                { ParamId::boosterBypass, 1.0f }, { ParamId::musicGainDb, 0.0f }, { ParamId::musicInputTrimDb, 0.0f },
                { ParamId::boosterAutoLevel, 0.0f }
            });
        }
        else if (name == "Karaoke Clear")
        {
            writeValues (params, {
                { ParamId::boosterBypass, 0.0f }, { ParamId::musicGainDb, 0.0f }, { ParamId::musicInputTrimDb, 0.0f },
                { ParamId::musicBassDb, 2.0f }, { ParamId::musicPresenceDb, 2.5f }, { ParamId::musicTrebleDb, 1.0f },
                { ParamId::musicLoudness, 20.0f }, { ParamId::boosterCompThresholdDb, -22.0f },
                { ParamId::boosterCompRatio, 2.5f }, { ParamId::boosterCompAttackMs, 12.0f },
                { ParamId::boosterCompReleaseMs, 120.0f }, { ParamId::boosterCompMakeupDb, 2.5f },
                { ParamId::boosterLimiterCeilingDb, -1.5f }, { ParamId::boosterOutputGainDb, 0.0f },
                { ParamId::boosterAutoLevel, 0.0f }
            });
        }
        else if (name == "Warm Music")
        {
            writeValues (params, {
                { ParamId::boosterBypass, 0.0f }, { ParamId::musicGainDb, 0.0f }, { ParamId::musicInputTrimDb, 0.0f },
                { ParamId::musicBassDb, 3.0f }, { ParamId::musicPresenceDb, 0.0f }, { ParamId::musicTrebleDb, -1.0f },
                { ParamId::musicLoudness, 30.0f }, { ParamId::boosterCompThresholdDb, -22.0f },
                { ParamId::boosterCompRatio, 2.0f }, { ParamId::boosterCompAttackMs, 20.0f },
                { ParamId::boosterCompReleaseMs, 160.0f }, { ParamId::boosterCompMakeupDb, 3.0f },
                { ParamId::boosterLimiterCeilingDb, -1.5f }, { ParamId::boosterOutputGainDb, 0.0f },
                { ParamId::boosterAutoLevel, 0.0f }
            });
        }
        else if (name == "Punchy")
        {
            writeValues (params, {
                { ParamId::boosterBypass, 0.0f }, { ParamId::musicGainDb, 0.0f }, { ParamId::musicInputTrimDb, 0.0f },
                { ParamId::musicBassDb, 2.5f }, { ParamId::musicPresenceDb, 3.0f }, { ParamId::musicTrebleDb, 1.5f },
                { ParamId::musicLoudness, 40.0f }, { ParamId::boosterCompThresholdDb, -24.0f },
                { ParamId::boosterCompRatio, 3.5f }, { ParamId::boosterCompAttackMs, 8.0f },
                { ParamId::boosterCompReleaseMs, 100.0f }, { ParamId::boosterCompMakeupDb, 4.0f },
                { ParamId::boosterLimiterCeilingDb, -1.0f }, { ParamId::boosterOutputGainDb, 0.0f },
                { ParamId::boosterAutoLevel, 0.0f }
            });
        }
        else if (name == "Loud but Safe")
        {
            writeValues (params, {
                { ParamId::boosterBypass, 0.0f }, { ParamId::musicGainDb, 0.0f }, { ParamId::musicInputTrimDb, 0.0f },
                { ParamId::musicBassDb, 2.0f }, { ParamId::musicPresenceDb, 2.0f }, { ParamId::musicTrebleDb, 1.0f },
                { ParamId::musicLoudness, 55.0f }, { ParamId::boosterCompThresholdDb, -26.0f },
                { ParamId::boosterCompRatio, 4.0f }, { ParamId::boosterCompAttackMs, 10.0f },
                { ParamId::boosterCompReleaseMs, 120.0f }, { ParamId::boosterCompMakeupDb, 6.0f },
                { ParamId::boosterLimiterCeilingDb, -1.0f }, { ParamId::boosterOutputGainDb, 0.0f },
                { ParamId::boosterAutoLevel, 1.0f }
            });
        }
        else
        {
            errorMessage = "Preset musik tidak dikenal: " + name;
            return false;
        }
    }
    else if (category == Category::Harmonizer)
    {
        writeValues (params, { { ParamId::harmonyEnabled, 1.0f }, { ParamId::harmonyBypass, 0.0f }, { ParamId::formantPreserve, 1.0f } });

        if (name == "Third Above")
        {
            writeValues (params, {
                { ParamId::hVoice1On, 1.0f }, { ParamId::hVoice2On, 0.0f }, { ParamId::hVoice1Interval, 0.0f },
                { ParamId::harmonyLevel, 40.0f }, { ParamId::harmonyDryWet, 30.0f }, { ParamId::humanize, 20.0f },
                { ParamId::retuneSpeedMs, 60.0f }, { ParamId::trackingStrength, 60.0f }, { ParamId::stereoWidth, 55.0f }
            });
        }
        else if (name == "Fifth + Octave")
        {
            writeValues (params, {
                { ParamId::hVoice1On, 1.0f }, { ParamId::hVoice1Interval, 1.0f },
                { ParamId::hVoice2On, 1.0f }, { ParamId::hVoice2Interval, 2.0f },
                { ParamId::harmonyLevel, 35.0f }, { ParamId::harmonyDryWet, 35.0f }, { ParamId::humanize, 25.0f },
                { ParamId::retuneSpeedMs, 60.0f }, { ParamId::trackingStrength, 65.0f }, { ParamId::stereoWidth, 70.0f }
            });
        }
        else if (name == "Subtle Double")
        {
            writeValues (params, {
                { ParamId::hVoice1On, 1.0f }, { ParamId::hVoice2On, 0.0f }, { ParamId::hVoice1Interval, 3.0f },
                { ParamId::hVoice1Custom, 0.0f }, { ParamId::harmonyLevel, 25.0f }, { ParamId::harmonyDryWet, 20.0f },
                { ParamId::humanize, 65.0f }, { ParamId::retuneSpeedMs, 80.0f }, { ParamId::trackingStrength, 55.0f },
                { ParamId::stereoWidth, 40.0f }
            });
        }
        else if (name == "Wide Choir")
        {
            writeValues (params, {
                { ParamId::hVoice1On, 1.0f }, { ParamId::hVoice1Interval, 0.0f },
                { ParamId::hVoice2On, 1.0f }, { ParamId::hVoice2Interval, 1.0f },
                { ParamId::harmonyLevel, 55.0f }, { ParamId::harmonyDryWet, 45.0f }, { ParamId::humanize, 35.0f },
                { ParamId::retuneSpeedMs, 50.0f }, { ParamId::trackingStrength, 75.0f }, { ParamId::stereoWidth, 85.0f }
            });
        }
        else
        {
            errorMessage = "Preset harmonizer tidak dikenal: " + name;
            return false;
        }
    }
    else if (category == Category::Master)
    {
        writeValues (params, { { ParamId::masterLimiterEnabled, 1.0f }, { ParamId::testToneEnabled, 0.0f } });

        if (name == "Safe Broadcast")
        {
            writeValues (params, { { ParamId::masterGainDb, 0.0f }, { ParamId::masterLimiterCeilingDb, -1.0f } });
        }
        else if (name == "Loud Live")
        {
            writeValues (params, { { ParamId::masterGainDb, 1.5f }, { ParamId::masterLimiterCeilingDb, -0.8f } });
        }
        else
        {
            errorMessage = "Preset master tidak dikenal: " + name;
            return false;
        }
    }
    else if (category == Category::Scene)
    {
        // Common bus and chain baseline.
        writeValues (params, {
            { ParamId::micGainDb, 6.0f }, { ParamId::micMixDb, 0.0f }, { ParamId::micMute, 0.0f }, { ParamId::micSolo, 0.0f },
            { ParamId::musicMixDb, 0.0f }, { ParamId::musicMute, 0.0f }, { ParamId::musicSolo, 0.0f },
            { ParamId::samplerMixDb, 0.0f }, { ParamId::samplerMute, 0.0f }, { ParamId::samplerSolo, 0.0f },
            { ParamId::masterGainDb, 0.0f }, { ParamId::boosterBypass, 0.0f },
            { ParamId::gateEnabled, 1.0f }, { ParamId::gateThresholdDb, -52.0f },
            { ParamId::hpfEnabled, 1.0f }, { ParamId::hpfHz, 95.0f },
            { ParamId::deEsserEnabled, 1.0f }, { ParamId::deEsserThresholdDb, -32.0f }, { ParamId::deEsserFreqHz, 6500.0f },
            { ParamId::compEnabled, 1.0f }, { ParamId::eqEnabled, 1.0f }, { ParamId::satEnabled, 0.0f },
            { ParamId::reverbEnabled, 1.0f }, { ParamId::delayEnabled, 0.0f }, { ParamId::vocalFaderDb, 0.0f },
            { ParamId::pitchCorrectionEnabled, 0.0f }, { ParamId::harmonyEnabled, 0.0f },
            { ParamId::sbEnabled, 1.0f }, { ParamId::sbTargetDb, 4.0f }, { ParamId::sbDuckDb, 6.0f },
            { ParamId::sbAttackMs, 25.0f }, { ParamId::sbReleaseMs, 400.0f }, { ParamId::sbStrength, 70.0f },
            { ParamId::sbMaxReductionDb, 10.0f },
            { ParamId::musicGainDb, 0.0f }, { ParamId::musicInputTrimDb, 0.0f }
        });

        if (name == "Pop")
        {
            writeValues (params, {
                { ParamId::musicBassDb, 2.0f }, { ParamId::musicPresenceDb, 2.5f }, { ParamId::musicTrebleDb, 1.0f },
                { ParamId::musicLoudness, 25.0f }, { ParamId::boosterCompThresholdDb, -22.0f },
                { ParamId::boosterCompRatio, 2.5f }, { ParamId::boosterCompAttackMs, 12.0f },
                { ParamId::boosterCompReleaseMs, 130.0f }, { ParamId::boosterCompMakeupDb, 3.0f },
                { ParamId::boosterLimiterCeilingDb, -1.2f }, { ParamId::boosterAutoLevel, 1.0f },
                { ParamId::compThresholdDb, -22.0f }, { ParamId::compRatio, 3.0f }, { ParamId::compAttackMs, 10.0f },
                { ParamId::compReleaseMs, 130.0f }, { ParamId::compMakeupDb, 3.0f },
                { ParamId::eq2Freq, 300.0f }, { ParamId::eq2Gain, -1.0f }, { ParamId::eq4Freq, 3800.0f }, { ParamId::eq4Gain, 1.5f },
                { ParamId::reverbMix, 12.0f }, { ParamId::reverbSize, 50.0f },
                { ParamId::masterLimiterCeilingDb, -1.0f },
                { ParamId::sbTargetDb, 4.0f }, { ParamId::sbDuckDb, 6.0f }
            });
        }
        else if (name == "Rock")
        {
            writeValues (params, {
                { ParamId::musicBassDb, 3.0f }, { ParamId::musicPresenceDb, 2.0f }, { ParamId::musicTrebleDb, 0.5f },
                { ParamId::musicLoudness, 20.0f }, { ParamId::boosterCompThresholdDb, -24.0f },
                { ParamId::boosterCompRatio, 3.5f }, { ParamId::boosterCompAttackMs, 8.0f },
                { ParamId::boosterCompReleaseMs, 100.0f }, { ParamId::boosterCompMakeupDb, 4.0f },
                { ParamId::boosterLimiterCeilingDb, -1.2f }, { ParamId::boosterAutoLevel, 0.0f },
                { ParamId::compThresholdDb, -26.0f }, { ParamId::compRatio, 4.0f }, { ParamId::compAttackMs, 8.0f },
                { ParamId::compReleaseMs, 120.0f }, { ParamId::compMakeupDb, 4.0f },
                { ParamId::eq4Freq, 3800.0f }, { ParamId::eq4Gain, 1.0f },
                { ParamId::satEnabled, 1.0f }, { ParamId::satDrive, 30.0f }, { ParamId::satMix, 25.0f },
                { ParamId::reverbMix, 8.0f }, { ParamId::reverbSize, 42.0f },
                { ParamId::masterLimiterCeilingDb, -1.0f },
                { ParamId::sbTargetDb, 3.0f }, { ParamId::sbDuckDb, 5.0f }, { ParamId::sbStrength, 65.0f }
            });
        }
        else if (name == "Dangdut")
        {
            writeValues (params, {
                { ParamId::musicBassDb, 3.5f }, { ParamId::musicPresenceDb, 3.0f }, { ParamId::musicTrebleDb, 1.5f },
                { ParamId::musicLoudness, 40.0f }, { ParamId::boosterCompThresholdDb, -24.0f },
                { ParamId::boosterCompRatio, 3.5f }, { ParamId::boosterCompAttackMs, 10.0f },
                { ParamId::boosterCompReleaseMs, 100.0f }, { ParamId::boosterCompMakeupDb, 4.0f },
                { ParamId::boosterLimiterCeilingDb, -1.0f }, { ParamId::boosterAutoLevel, 1.0f },
                { ParamId::compThresholdDb, -22.0f }, { ParamId::compRatio, 3.5f }, { ParamId::compAttackMs, 10.0f },
                { ParamId::compReleaseMs, 130.0f }, { ParamId::compMakeupDb, 3.5f },
                { ParamId::eq4Freq, 3800.0f }, { ParamId::eq4Gain, 1.5f },
                { ParamId::reverbMix, 15.0f }, { ParamId::reverbSize, 52.0f },
                { ParamId::delayEnabled, 1.0f }, { ParamId::delayTimeMs, 280.0f }, { ParamId::delayFeedback, 25.0f }, { ParamId::delayMix, 12.0f },
                { ParamId::harmonyEnabled, 1.0f }, { ParamId::hVoice1On, 1.0f }, { ParamId::hVoice1Interval, 0.0f },
                { ParamId::hVoice2On, 1.0f }, { ParamId::hVoice2Interval, 1.0f },
                { ParamId::harmonyLevel, 30.0f }, { ParamId::harmonyDryWet, 22.0f }, { ParamId::humanize, 30.0f },
                { ParamId::retuneSpeedMs, 60.0f }, { ParamId::trackingStrength, 70.0f }, { ParamId::stereoWidth, 65.0f },
                { ParamId::masterLimiterCeilingDb, -1.2f },
                { ParamId::sbTargetDb, 5.0f }, { ParamId::sbDuckDb, 7.0f }, { ParamId::sbStrength, 75.0f }
            });
        }
        else if (name == "Ballad")
        {
            writeValues (params, {
                { ParamId::musicBassDb, 1.5f }, { ParamId::musicPresenceDb, 1.0f }, { ParamId::musicTrebleDb, 0.0f },
                { ParamId::musicLoudness, 15.0f }, { ParamId::boosterCompThresholdDb, -20.0f },
                { ParamId::boosterCompRatio, 2.0f }, { ParamId::boosterCompAttackMs, 20.0f },
                { ParamId::boosterCompReleaseMs, 150.0f }, { ParamId::boosterCompMakeupDb, 2.0f },
                { ParamId::boosterLimiterCeilingDb, -1.5f }, { ParamId::boosterAutoLevel, 0.0f },
                { ParamId::compThresholdDb, -20.0f }, { ParamId::compRatio, 2.5f }, { ParamId::compAttackMs, 15.0f },
                { ParamId::compReleaseMs, 160.0f }, { ParamId::compMakeupDb, 2.0f },
                { ParamId::eq2Freq, 300.0f }, { ParamId::eq2Gain, -1.0f },
                { ParamId::reverbEnabled, 1.0f }, { ParamId::reverbMix, 24.0f }, { ParamId::reverbSize, 65.0f }, { ParamId::reverbDamp, 55.0f },
                { ParamId::delayEnabled, 1.0f }, { ParamId::delayTimeMs, 330.0f }, { ParamId::delayFeedback, 28.0f }, { ParamId::delayMix, 12.0f },
                { ParamId::masterLimiterCeilingDb, -1.5f },
                { ParamId::sbTargetDb, 5.0f }, { ParamId::sbDuckDb, 5.0f }, { ParamId::sbAttackMs, 30.0f },
                { ParamId::sbReleaseMs, 500.0f }, { ParamId::sbStrength, 65.0f }
            });
        }
        else if (name == "Custom")
        {
            params.setSilently (ParamId::masterLimiterEnabled, 1.0f);
            params.setSilently (ParamId::masterLimiterCeilingDb, -1.0f);
            params.setSilently (ParamId::reverbEnabled, 1.0f);
            params.setSilently (ParamId::reverbMix, 12.0f);
            params.setSilently (ParamId::testToneEnabled, 0.0f);
        }
        else
        {
            errorMessage = "Scene tidak dikenal: " + name;
            return false;
        }
    }
    else
    {
        errorMessage = "Kategori ini tidak memiliki preset bawaan.";
        return false;
    }

    params.sendChangeMessage();
    sendChangeMessage();
    return true;
}

//==============================================================================
bool PresetManager::applyParams (Category category, const juce::var& paramsVar,
                                 juce::StringArray* warnings)
{
    if (paramsVar.isVoid())
        return true;

    auto* object = paramsVar.getDynamicObject();

    if (object == nullptr)
    {
        if (warnings != nullptr)
            warnings->add ("Bagian parameter preset tidak valid dan diabaikan.");

        return true;
    }

    auto filtered = juce::DynamicObject::Ptr (new juce::DynamicObject());

    for (const auto& property : object->getProperties())
    {
        const auto id = ParamStore::idFromKey (property.name.toString());

        if (id == ParamId::count)
        {
            if (warnings != nullptr)
                warnings->add ("Parameter tidak dikenal diabaikan: " + property.name.toString());
            continue;
        }

        if (! isAllowedParam (category, id))
        {
            if (warnings != nullptr)
                warnings->add ("Parameter di luar kategori diabaikan: " + property.name.toString());
            continue;
        }

        if (! property.value.isInt() && ! property.value.isInt64()
            && ! property.value.isDouble() && ! property.value.isBool())
        {
            if (warnings != nullptr)
                warnings->add ("Nilai parameter bukan angka diabaikan: " + property.name.toString());
            continue;
        }

        filtered->setProperty (property.name, property.value);
    }

    return params.fromVar (juce::var (filtered.get()), warnings);
}

bool PresetManager::applyPresetData (Category category, const juce::var& root,
                                     juce::String& errorMessage, juce::StringArray* warnings)
{
    auto* object = root.getDynamicObject();

    if (object == nullptr)
    {
        errorMessage = "File preset bukan objek JSON yang valid.";
        return false;
    }

    const auto format = object->getProperty ("format").toString();

    if (format.isNotEmpty() && format != "RrLiveStudioPreset")
    {
        errorMessage = "Format preset tidak dikenal: " + format;
        return false;
    }

    const auto version = (int) object->getProperty ("version");

    if (version <= 0)
    {
        errorMessage = "Preset tidak memiliki nomor versi yang valid.";
        return false;
    }

    if (version > formatVersion)
    {
        errorMessage = "Preset dibuat oleh versi Rr Live Studio yang lebih baru.";
        return false;
    }

    const auto storedCategory = object->getProperty ("category").toString();

    if (storedCategory.isNotEmpty() && storedCategory != categoryFolder (category))
    {
        errorMessage = "Kategori preset tidak cocok (file: " + storedCategory + ").";
        return false;
    }

    const auto baseFactory = object->getProperty ("baseFactory").toString();

    if (baseFactory.isNotEmpty())
    {
        if (! applyFactoryPreset (category, baseFactory, errorMessage))
            return false;
    }
    else
    {
        resetGroupsForCategory (params, category);
    }

    if (! applyParams (category, object->getProperty ("parameters"), warnings))
        if (warnings != nullptr)
            warnings->add ("Beberapa parameter preset gagal diterapkan.");

    if ((category == Category::Sampler || category == Category::Scene)
        && object->hasProperty ("samplerState"))
    {
        juce::String samplerError;

        if (! sampler.restoreState (object->getProperty ("samplerState"), samplerError))
            if (warnings != nullptr)
                warnings->add ("Sampler: " + samplerError);
    }

    if ((category == Category::Vocal || category == Category::Scene)
        && object->hasProperty ("vocalChain"))
    {
        juce::String chainError;

        if (! plugins.restoreChain (false, object->getProperty ("vocalChain"), chainError))
            if (warnings != nullptr)
                warnings->add ("VST vokal: " + chainError);
    }

    if ((category == Category::Master || category == Category::Scene)
        && object->hasProperty ("masterChain"))
    {
        juce::String chainError;

        if (! plugins.restoreChain (true, object->getProperty ("masterChain"), chainError))
            if (warnings != nullptr)
                warnings->add ("VST master: " + chainError);
    }

    params.sendChangeMessage();
    sendChangeMessage();
    return true;
}

bool PresetManager::load (Category category, const juce::String& name,
                          juce::String& errorMessage, juce::StringArray* warnings)
{
    const auto file = getFileFor (category, name);

    if (file.existsAsFile())
    {
        auto data = juce::JSON::parse (file);

        if (data.isVoid())
        {
            errorMessage = "File preset rusak atau bukan JSON: " + file.getFullPathName();
            return false;
        }

        return applyPresetData (category, data, errorMessage, warnings);
    }

    if (getFactoryPresetNames (category).contains (name))
        return applyFactoryPreset (category, name, errorMessage);

    errorMessage = "Preset tidak ditemukan: " + name;
    return false;
}

} // namespace rr
