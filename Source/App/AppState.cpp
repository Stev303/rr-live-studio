#include "App/AppState.h"

namespace rr
{
namespace
{
    void appendWarning (juce::String& destination, const juce::String& message)
    {
        if (message.isEmpty())
            return;

        if (destination.isNotEmpty())
            destination << "\n";

        destination << message;
    }
}

AppState::AppState()
    : engine (params, sampler, pluginHost, diagnostics),
      presets (params, sampler, pluginHost)
{
}

void AppState::initialise()
{
    dataFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                     .getChildFile ("Rr Live Studio");
    dataFolder.createDirectory();

    juce::PropertiesFile::Options options;
    options.applicationName = "Rr Live Studio";
    options.folderName = "Rr Live Studio";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    applicationProperties.setStorageParameters (options);

    diagnostics.initialise (dataFolder.getChildFile ("Logs"));
    presets.initialise();

    juce::String savedDeviceState;
    juce::String savedLoopbackEndpoint;
    bool loopbackWasActive = false;

    if (auto* properties = getProperties())
    {
        const auto parameters = properties->getValue ("parametersJson");

        if (parameters.isNotEmpty())
        {
            juce::StringArray warnings;

            if (! params.fromVar (juce::JSON::parse (parameters), &warnings))
                appendWarning (startupWarning, "Sebagian parameter lama tidak dapat dipulihkan.");

            for (const auto& warning : warnings)
                appendWarning (startupWarning, warning);
        }

        currentPage = juce::jlimit (0, 8, properties->getIntValue ("page", 0));
        savedDeviceState = properties->getValue ("audioDeviceState");
        savedLoopbackEndpoint = properties->getValue ("loopbackEndpointId");
        loopbackWasActive = properties->getBoolValue ("loopbackActive", false);
    }

    std::unique_ptr<juce::XmlElement> deviceXml;

    if (savedDeviceState.isNotEmpty())
        deviceXml = juce::XmlDocument::parse (savedDeviceState);

    juce::String deviceError;

    if (! engine.initialiseDevices (deviceXml.get(), deviceError))
    {
        deviceInitError = deviceError;
        appendWarning (startupWarning,
                       "Perangkat audio tidak dapat dibuka: " + deviceError
                       + "\nBuka halaman Settings untuk memilih perangkat.");
    }

    engine.setLoopbackEndpointId (savedLoopbackEndpoint);

    juce::String modeError;
    const auto savedMode = getMode();

    if (engine.isRunning())
    {
        applyLatencyMode (savedMode, modeError);

        if (modeError.isNotEmpty())
            appendWarning (startupWarning, "Mode buffer tidak dapat diterapkan: " + modeError);
    }

    restoreSession();

    if (loopbackWasActive && engine.isRunning())
    {
        juce::String loopbackError;

        if (! engine.startLoopback (loopbackError))
            appendWarning (startupWarning,
                           "Loopback tidak dimulai otomatis: " + loopbackError);
    }

    diagnostics.log ("Aplikasi dimulai. " + engine.getDeviceDescription());
}

juce::PropertiesFile* AppState::getProperties() const
{
    return applicationProperties.getUserSettings();
}

juce::StringArray AppState::getModeNames()
{
    return { "Latency Rendah", "Seimbang", "Kualitas" };
}

juce::String AppState::getMode() const
{
    if (auto* properties = getProperties())
    {
        const auto value = properties->getValue ("mode", "Seimbang");

        if (getModeNames().contains (value))
            return value;
    }

    return "Seimbang";
}

void AppState::setMode (const juce::String& newMode, juce::String& errorMessage)
{
    if (! getModeNames().contains (newMode))
    {
        errorMessage = "Mode tidak dikenal.";
        return;
    }

    if (newMode == getMode())
        return;

    if (engine.isRunning())
        applyLatencyMode (newMode, errorMessage);

    if (errorMessage.isNotEmpty())
        return;

    if (auto* properties = getProperties())
    {
        properties->setValue ("mode", newMode);
        properties->saveIfNeeded();
    }
}

void AppState::applyLatencyMode (const juce::String& modeName, juce::String& errorMessage)
{
    auto* device = engine.getDeviceManager().getCurrentAudioDevice();

    if (device == nullptr)
    {
        errorMessage = "Perangkat audio belum aktif.";
        return;
    }

    const auto target = modeName == "Latency Rendah" ? 128
                      : modeName == "Kualitas" ? 512
                      : 256;

    const auto available = device->getAvailableBufferSizes();

    if (available.isEmpty())
        return;

    int chosen = available[0];
    int bestDistance = std::abs (chosen - target);

    for (const auto size : available)
    {
        const auto distance = std::abs (size - target);

        if (distance < bestDistance || (distance == bestDistance && size < chosen))
        {
            chosen = size;
            bestDistance = distance;
        }
    }

    auto setup = engine.getDeviceManager().getAudioDeviceSetup();

    if (setup.bufferSize == chosen)
        return;

    setup.bufferSize = chosen;
    errorMessage = engine.getDeviceManager().setAudioDeviceSetup (setup, true);
}

int AppState::getCurrentPage() const
{
    return currentPage;
}

void AppState::setCurrentPage (int pageIndex)
{
    currentPage = juce::jlimit (0, 8, pageIndex);

    if (auto* properties = getProperties())
        properties->setValue ("page", currentPage);
}

void AppState::navigateTo (int pageIndex)
{
    if (onNavigate != nullptr)
        onNavigate (pageIndex);
}

void AppState::restoreSession()
{
    if (auto* properties = getProperties())
    {
        const auto samplerState = properties->getValue ("samplerState");

        if (samplerState.isNotEmpty())
        {
            juce::String error;

            if (! sampler.restoreState (juce::JSON::parse (samplerState), error))
                appendWarning (startupWarning, "Sampler tidak dipulihkan: " + error);
        }

        const auto vocalState = properties->getValue ("vocalChainState");

        if (vocalState.isNotEmpty())
        {
            juce::String error;

            if (! pluginHost.restoreChain (false, juce::JSON::parse (vocalState), error))
                appendWarning (startupWarning, "Vocal VST chain: " + error);
        }

        const auto masterState = properties->getValue ("masterChainState");

        if (masterState.isNotEmpty())
        {
            juce::String error;

            if (! pluginHost.restoreChain (true, juce::JSON::parse (masterState), error))
                appendWarning (startupWarning, "Master VST chain: " + error);
        }
    }
}

void AppState::saveSession()
{
    if (auto* properties = getProperties())
    {
        properties->setValue ("samplerState", juce::JSON::toString (sampler.saveState()));
        properties->setValue ("vocalChainState", juce::JSON::toString (pluginHost.saveChain (false)));
        properties->setValue ("masterChainState", juce::JSON::toString (pluginHost.saveChain (true)));
    }
}

void AppState::save()
{
    saveSession();

    if (auto* properties = getProperties())
    {
        properties->setValue ("parametersJson", juce::JSON::toString (params.toVar()));
        properties->setValue ("page", currentPage);
        properties->setValue ("mode", getMode());
        properties->setValue ("loopbackEndpointId", engine.getLoopbackEndpointId());
        properties->setValue ("loopbackActive", engine.isLoopbackActive());

        if (auto xml = engine.getDeviceManager().createStateXml())
            properties->setValue ("audioDeviceState", xml->toString (juce::XmlElement::TextFormat().singleLine()));

        properties->saveIfNeeded();
    }

    diagnostics.log ("Pengaturan disimpan.");
}

} // namespace rr
