#include "Audio/ParamStore.h"

namespace rr
{
static const ParamInfo paramInfos[] =
{
#define RR_INFO_ENTRY(name, key, label, unit, minv, maxv, defv, skewv, group, toggle, decimals) \
    { ParamId::name, key, label, unit, minv, maxv, defv, skewv, group, toggle, decimals },
    RR_PARAM_LIST (RR_INFO_ENTRY)
#undef RR_INFO_ENTRY
};

static_assert ((size_t) ParamId::count == (sizeof (paramInfos) / sizeof (paramInfos[0])),
               "Param list and ParamId enum are out of sync");

ParamStore::ParamStore()
{
    for (int i = 0; i < numParams; ++i)
        values[(size_t) i].store (paramInfos[i].defaultValue, std::memory_order_relaxed);
}

const ParamInfo& ParamStore::info (ParamId id) noexcept
{
    const auto index = (size_t) id;
    jassert (index < (size_t) numParams);
    return paramInfos[juce::jlimit ((size_t) 0, (size_t) numParams - 1, index)];
}

ParamId ParamStore::idFromKey (const juce::String& key) noexcept
{
    for (int i = 0; i < numParams; ++i)
        if (key == paramInfos[i].key)
            return paramInfos[i].id;

    return ParamId::count;
}

juce::String ParamStore::groupName (ParamGroup group)
{
    switch (group)
    {
        case ParamGroup::Master:     return "Master";
        case ParamGroup::Music:      return "Music Mix";
        case ParamGroup::Booster:    return "YouTube Booster";
        case ParamGroup::Balance:    return "Smart Balance";
        case ParamGroup::Pitch:      return "Pitch / Key";
        case ParamGroup::Vocal:      return "Vocal Chain";
        case ParamGroup::Correction: return "Pitch Correction";
        case ParamGroup::Harmony:    return "Harmonizer";
        case ParamGroup::Sampler:    return "Sampler";
        case ParamGroup::Test:       return "Test Signal";
    }

    return {};
}

void ParamStore::set (ParamId id, float newValue)
{
    const auto& p = info (id);
    newValue = juce::jlimit (p.minValue, p.maxValue, newValue);

    if (p.isToggle)
        newValue = newValue > 0.5f ? 1.0f : 0.0f;

    const auto previous = values[(size_t) id].exchange (newValue, std::memory_order_relaxed);

    if (! juce::approximatelyEqual (previous, newValue))
        sendChangeMessage();
}

void ParamStore::setSilently (ParamId id, float newValue) noexcept
{
    const auto& p = info (id);
    newValue = juce::jlimit (p.minValue, p.maxValue, newValue);

    if (p.isToggle)
        newValue = newValue > 0.5f ? 1.0f : 0.0f;

    values[(size_t) id].store (newValue, std::memory_order_relaxed);
}

void ParamStore::resetAll()
{
    for (int i = 0; i < numParams; ++i)
        setSilently (paramInfos[i].id, paramInfos[i].defaultValue);

    sendChangeMessage();
}

void ParamStore::resetGroup (ParamGroup group)
{
    for (int i = 0; i < numParams; ++i)
        if (paramInfos[i].group == group)
            setSilently (paramInfos[i].id, paramInfos[i].defaultValue);

    sendChangeMessage();
}

juce::var ParamStore::toVar() const
{
    auto object = juce::DynamicObject::Ptr (new juce::DynamicObject());

    for (int i = 0; i < numParams; ++i)
        object->setProperty (paramInfos[i].key, values[(size_t) i].load (std::memory_order_relaxed));

    return juce::var (object.get());
}

bool ParamStore::fromVar (const juce::var& source, juce::StringArray* warnings)
{
    auto* object = source.getDynamicObject();

    if (object == nullptr)
    {
        if (warnings != nullptr)
            warnings->add ("Preset: root object tidak valid.");

        return false;
    }

    for (const auto& property : object->getProperties())
    {
        const auto id = idFromKey (property.name.toString());

        if (id == ParamId::count)
        {
            if (warnings != nullptr)
                warnings->add ("Parameter tidak dikenal diabaikan: " + property.name.toString());
            continue;
        }

        if (! property.value.isInt() && ! property.value.isInt64()
            && ! property.value.isDouble() && ! property.value.isBool())
        {
            if (warnings != nullptr)
                warnings->add ("Nilai bukan angka diabaikan: " + property.name.toString());
            continue;
        }

        setSilently (id, (float) property.value);
    }

    sendChangeMessage();
    return true;
}

juce::String ParamStore::getDisplayValue (ParamId id) const
{
    const auto& p = info (id);
    const auto value = get (id);

    if (p.isToggle)
        return value > 0.5f ? "ON" : "OFF";

    juce::String text;

    if (p.decimals <= 0)
        text = juce::String ((int) std::lround (value));
    else
        text = juce::String (value, p.decimals);

    if (p.unit != nullptr && p.unit[0] != 0)
        text << " " << p.unit;

    return text;
}

} // namespace rr
