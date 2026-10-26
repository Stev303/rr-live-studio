#include "Audio/BusRouter.h"
#include "Audio/ParamStore.h"
#include "Presets/PresetManager.h"

#include <juce_core/juce_core.h>
#include <vector>

namespace
{
class PresetRoutingTests final : public juce::UnitTest
{
public:
    PresetRoutingTests() : juce::UnitTest ("Routing and Presets", "RrLiveStudio") {}

    void runTest() override
    {
        runBusRouterTests();
        runParamStoreTests();
        runFactoryPresetMetadataTests();
    }

private:
    void runBusRouterTests()
    {
        beginTest ("BusRouter gain, mute dan solo");

        rr::BusRouter::BusState bus;
        expectWithinAbsoluteError (rr::BusRouter::gainForBus (bus, false), 1.0f, 1.0e-6f);

        bus.faderDb = -6.0f;
        expectWithinAbsoluteError (rr::BusRouter::gainForBus (bus, false),
                                   (float) juce::Decibels::decibelsToGain (-6.0), 1.0e-6f);

        bus.muted = true;
        expectEquals (rr::BusRouter::gainForBus (bus, false), 0.0f, "Mute harus menghasilkan gain 0");

        bus.muted = false;
        bus.soloed = false;
        expectEquals (rr::BusRouter::gainForBus (bus, true), 0.0f,
                      "Bus tanpa solo harus bisu saat solo aktif");

        bus.soloed = true;
        expectGreaterThan (rr::BusRouter::gainForBus (bus, true), 0.0f, "Bus solo harus terdengar");

        bus.muted = true;
        expectEquals (rr::BusRouter::gainForBus (bus, true), 0.0f, "Mute harus mengalahkan solo");

        beginTest ("BusRouter summing dan konversi mono ke stereo");

        std::vector<float> destL (4, 0.25f), destR (4, -0.25f);
        const std::vector<float> srcL { 0.1f, 0.2f, 0.3f, 0.4f };
        const std::vector<float> srcR { -0.1f, -0.2f, -0.3f, -0.4f };

        rr::BusRouter::sumInto (destL.data(), destR.data(), srcL.data(), srcR.data(), 4, 0.5f);
        expectWithinAbsoluteError (destL[0], 0.3f, 1.0e-6f);
        expectWithinAbsoluteError (destR[3], -0.45f, 1.0e-6f);

        rr::BusRouter::sumInto (destL.data(), destR.data(), srcL.data(), srcR.data(), 4, 0.0f);
        expectWithinAbsoluteError (destL[1], 0.35f, 1.0e-6f, "Gain 0 tidak boleh mengubah tujuan");

        std::vector<float> monoDest (4, 0.0f);
        rr::BusRouter::sumInto (monoDest.data(), nullptr, srcL.data(), nullptr, 4, 1.0f);
        expectWithinAbsoluteError (monoDest[2], 0.3f, 1.0e-6f);

        std::vector<float> outL (4, 9.0f), outR (4, 9.0f);
        rr::BusRouter::copyToStereoOutput (srcL.data(), nullptr, outL.data(), outR.data(), 4, 2);
        expectWithinAbsoluteError (outL[2], 0.3f, 1.0e-6f);
        expectWithinAbsoluteError (outR[2], 0.3f, 1.0e-6f, "Sumber mono harus disalin ke kanan");

        rr::BusRouter::copyToStereoOutput (srcL.data(), srcR.data(), outL.data(), outR.data(), 4, 2);
        expectWithinAbsoluteError (outR[1], -0.2f, 1.0e-6f);

        std::vector<float> singleOutL (4, 0.0f), singleOutR (4, 7.0f);
        rr::BusRouter::copyToStereoOutput (srcL.data(), srcR.data(), singleOutL.data(), singleOutR.data(), 4, 1);
        expectWithinAbsoluteError (singleOutR[0], 7.0f, 1.0e-6f, "Output mono tidak boleh menyentuh kanan");
    }

    void runParamStoreTests()
    {
        beginTest ("ParamStore clamping dan toggle");

        rr::ParamStore store;

        expectWithinAbsoluteError (store.get (rr::ParamId::micGainDb),
                                   rr::ParamStore::info (rr::ParamId::micGainDb).defaultValue, 1.0e-6f);

        store.set (rr::ParamId::micGainDb, 999.0f);
        expectWithinAbsoluteError (store.get (rr::ParamId::micGainDb), 24.0f, 1.0e-6f,
                                   "Nilai harus di-clamp ke batas maksimum");

        store.set (rr::ParamId::reverbEnabled, 0.4f);
        expectEquals (store.get (rr::ParamId::reverbEnabled), 0.0f, "Toggle harus dinormalisasi ke 0");

        store.setBool (rr::ParamId::reverbEnabled, true);
        expectEquals (store.get (rr::ParamId::reverbEnabled), 1.0f);

        expect (store.idFromKey ("vocal.inputGain") == rr::ParamId::micGainDb, "Key parameter harus dikenali");
        expect (store.idFromKey ("tidak.ada") == rr::ParamId::count, "Key asing harus menghasilkan count");

        beginTest ("ParamStore JSON round-trip");

        rr::ParamStore original;
        original.set (rr::ParamId::micGainDb, 9.5f);
        original.set (rr::ParamId::masterLimiterCeilingDb, -2.5f);
        original.set (rr::ParamId::transpose, -5.0f);
        original.setBool (rr::ParamId::eqEnabled, true);
        original.setBool (rr::ParamId::reverbEnabled, false);
        original.set (rr::ParamId::hVoice1Custom, 4.0f);

        const auto text = juce::JSON::toString (original.toVar(), false);
        expect (text.isNotEmpty(), "JSON preset tidak boleh kosong");

        juce::var parsed;
        const auto parseResult = juce::JSON::parse (text, parsed);
        expect (parseResult.wasOk(), "JSON preset harus valid: " + parseResult.getErrorMessage());

        rr::ParamStore restored;
        juce::StringArray warnings;
        expect (restored.fromVar (parsed, &warnings), "fromVar harus menerima objek JSON");
        expect (warnings.isEmpty(), "Round-trip tanpa warning: " + warnings.joinIntoString ("; "));

        for (int i = 0; i < rr::ParamStore::numParams; ++i)
        {
            const auto id = (rr::ParamId) i;
            expectWithinAbsoluteError (restored.get (id), original.get (id), 1.0e-6f,
                                       "Parameter tidak sama setelah round-trip: "
                                           + juce::String (rr::ParamStore::info (id).key));
        }

        beginTest ("ParamStore memberi warning untuk data tidak valid");

        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty ("tidak.dikenal", 1.0f);
        object->setProperty ("vocal.hpfHz", "bukan angka");
        object->setProperty ("vocal.inputGain", 3.5f);

        juce::StringArray partialWarnings;
        expect (restored.fromVar (juce::var (object.get()), &partialWarnings),
                "Objek dengan sebagian nilai valid harus tetap diterapkan");
        expectEquals (partialWarnings.size(), 2, "Harus ada dua warning");
        expectWithinAbsoluteError (restored.get (rr::ParamId::micGainDb), 3.5f, 1.0e-6f);

        juce::StringArray rootWarnings;
        expect (! restored.fromVar (juce::var (42), &rootWarnings), "Root non-objek harus ditolak");
        expectEquals (rootWarnings.size(), 1);

        beginTest ("resetGroup hanya mengembalikan grup terpilih");

        rr::ParamStore groups;
        groups.set (rr::ParamId::micGainDb, -3.0f);
        groups.set (rr::ParamId::masterGainDb, -6.0f);
        groups.resetGroup (rr::ParamGroup::Vocal);

        expectWithinAbsoluteError (groups.get (rr::ParamId::micGainDb),
                                   rr::ParamStore::info (rr::ParamId::micGainDb).defaultValue, 1.0e-6f);
        expectWithinAbsoluteError (groups.get (rr::ParamId::masterGainDb), -6.0f, 1.0e-6f,
                                   "Grup lain tidak boleh ikut di-reset");
    }

    void runFactoryPresetMetadataTests()
    {
        beginTest ("Metadata factory preset konsisten");

        using Category = rr::PresetManager::Category;

        expectEquals (rr::PresetManager::categoryName (Category::Vocal), juce::String ("Vocal"));
        expectEquals (rr::PresetManager::categoryFolder (Category::Scene), juce::String ("Scenes"));
        expectEquals (rr::PresetManager::categoryFolder (Category::Master), juce::String ("Master"));

        const auto vocals = rr::PresetManager::getFactoryPresetNames (Category::Vocal);
        expectGreaterThan (vocals.size(), 4, "Harus ada beberapa preset vokal bawaan");
        expect (vocals.contains ("Live Karaoke"), "Preset Live Karaoke harus tersedia");

        expect (rr::PresetManager::getFactoryPresetNames (Category::Sampler).isEmpty(),
                "Sampler tidak memakai preset parameter factory");

        expect (rr::PresetManager::getFactoryPresetNames (Category::Scene).contains ("Dangdut"),
                "Scene Dangdut harus tersedia");
    }
};

PresetRoutingTests presetRoutingTests;
} // namespace
