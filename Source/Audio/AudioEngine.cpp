#include "Audio/AudioEngine.h"

namespace rr
{
namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;

    float smoothMeterDb (float previous, float target) noexcept
    {
        if (target > previous)
            return target;

        return juce::jmax (target, previous - 0.6f);
    }

    float peakOf (const float* left, const float* right, int numSamples) noexcept
    {
        float peak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (left[i]), right != nullptr ? std::abs (right[i]) : 0.0f);

        return peak;
    }

    float rmsOf (const float* left, const float* right, int numSamples) noexcept
    {
        double sumSquares = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            sumSquares += (double) left[i] * left[i];

            if (right != nullptr)
                sumSquares += (double) right[i] * right[i];
        }

        const auto channels = right != nullptr ? 2 : 1;
        return (float) std::sqrt (sumSquares / (double) juce::jmax (1, numSamples * channels));
    }
}

AudioEngine::AudioEngine (ParamStore& parameters,
                          SamplerEngine& samplerEngine,
                          PluginHost& host,
                          DiagnosticsLog& diagnosticsLog)
    : params (parameters),
      sampler (samplerEngine),
      pluginHost (host),
      diagnostics (diagnosticsLog),
      vocalChain (parameters),
      musicBooster (parameters),
      smartBalance (parameters)
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::AlignmentDelay::prepare (double newSampleRate, double maximumDelaySeconds)
{
    auto required = (int) std::ceil (newSampleRate * maximumDelaySeconds) + 8;
    auto powerOfTwo = 1;

    while (powerOfTwo < required)
        powerOfTwo <<= 1;

    size = powerOfTwo;
    bufferL.assign ((size_t) size, 0.0f);
    bufferR.assign ((size_t) size, 0.0f);
    reset();
}

void AudioEngine::AlignmentDelay::reset() noexcept
{
    std::fill (bufferL.begin(), bufferL.end(), 0.0f);
    std::fill (bufferR.begin(), bufferR.end(), 0.0f);
    writeIndex = 0;
    currentDelay = 0.0;
}

void AudioEngine::AlignmentDelay::setTargetSamples (int newTarget) noexcept
{
    targetDelay = juce::jlimit (0, juce::jmax (0, size - 2), newTarget);
}

void AudioEngine::AlignmentDelay::process (float* left, float* right, int numSamples) noexcept
{
    if (size < 4 || left == nullptr || numSamples <= 0)
        return;

    const auto step = ((double) targetDelay - currentDelay) / (double) juce::jmax (1, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        bufferL[(size_t) writeIndex] = left[i];
        bufferR[(size_t) writeIndex] = right != nullptr ? right[i] : left[i];

        auto readPosition = (double) writeIndex - currentDelay;

        while (readPosition < 0.0)
            readPosition += (double) size;

        auto index0 = (int) readPosition;

        if (index0 >= size)
            index0 -= size;

        const auto index1 = index0 + 1 >= size ? 0 : index0 + 1;
        const auto fraction = (float) (readPosition - std::floor (readPosition));

        const auto outL = bufferL[(size_t) index0]
                        + fraction * (bufferL[(size_t) index1] - bufferL[(size_t) index0]);
        const auto outR = bufferR[(size_t) index0]
                        + fraction * (bufferR[(size_t) index1] - bufferR[(size_t) index0]);

        left[i] = outL;

        if (right != nullptr)
            right[i] = outR;

        currentDelay += step;

        if (currentDelay < 0.0)
            currentDelay = 0.0;

        writeIndex = (writeIndex + 1) & (size - 1);
    }

    if (std::abs ((double) targetDelay - currentDelay) < 0.01)
        currentDelay = (double) targetDelay;
}

bool AudioEngine::initialiseDevices (const juce::XmlElement* savedDeviceState, juce::String& errorMessage)
{
    if (deviceRunning.load (std::memory_order_relaxed))
        return true;

    deviceManager.addAudioCallback (this);
    deviceManager.addChangeListener (this);

    auto result = deviceManager.initialise (2, 2, savedDeviceState, true);

    if (result.isNotEmpty())
    {
        diagnostics.log ("Inisialisasi audio gagal: " + result);
        result = deviceManager.initialise (0, 2, savedDeviceState, true);
    }

    if (savedDeviceState == nullptr)
    {
        bool hasAsio = false;
        juce::String wasapiName;

        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            if (type == nullptr)
                continue;

            if (type->getTypeName().equalsIgnoreCase ("ASIO"))
                hasAsio = true;
            else if (type->getTypeName().containsIgnoreCase ("Windows Audio"))
                wasapiName = type->getTypeName();
        }

        if (hasAsio)
        {
            deviceManager.setCurrentAudioDeviceType ("ASIO", true);

            if (deviceManager.getCurrentAudioDevice() == nullptr)
            {
                diagnostics.log ("ASIO tidak dapat dibuka, kembali ke WASAPI.");

                if (wasapiName.isNotEmpty())
                    deviceManager.setCurrentAudioDeviceType (wasapiName, true);
            }
        }
    }

    if (deviceManager.getCurrentAudioDevice() == nullptr)
    {
        result = deviceManager.initialise (2, 2, nullptr, true);

        if (result.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
        {
            errorMessage = result.isNotEmpty() ? result
                                               : "Tidak ada perangkat audio output yang dapat dibuka.";
            diagnostics.log ("Audio tidak aktif: " + errorMessage);
            return false;
        }
    }

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
        currentSampleRate.store (device->getCurrentSampleRate());
        currentBlockSize.store (device->getCurrentBufferSizeSamples());
        deviceRunning.store (true);
        diagnostics.log ("Audio aktif: " + getDeviceDescription());
    }

    if (loopbackWanted.load (std::memory_order_relaxed))
        triggerAsyncUpdate();

    return true;
}

void AudioEngine::shutdown()
{
    loopbackWanted.store (false, std::memory_order_relaxed);
    loopback.stop();

    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
    pluginHost.releaseResources();

    deviceRunning.store (false, std::memory_order_relaxed);
    preparedBlockSize.store (0, std::memory_order_relaxed);
    cancelPendingUpdate();
}

juce::String AudioEngine::getDeviceDescription() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::String description;
        description << deviceManager.getCurrentAudioDeviceType() << " | " << device->getName()
                    << " | " << juce::String (device->getCurrentSampleRate(), 1) << " Hz"
                    << " | " << device->getCurrentBufferSizeSamples() << " samples"
                    << " | in " << device->getActiveInputChannels().countNumberOfSetBits()
                    << " / out " << device->getActiveOutputChannels().countNumberOfSetBits();
        return description;
    }

    return "Audio device belum aktif";
}

juce::String AudioEngine::getDeviceError() const
{
    const juce::ScopedLock lock (deviceErrorLock);
    return lastDeviceError;
}

double AudioEngine::getDeviceLatencyMs() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto sampleRate = device->getCurrentSampleRate();

        if (sampleRate > 0.0)
        {
            const auto samples = juce::jmax (0, device->getInputLatencyInSamples())
                               + juce::jmax (0, device->getOutputLatencyInSamples());
            return (double) samples * 1000.0 / sampleRate;
        }
    }

    return 0.0;
}

//==============================================================================
void AudioEngine::setLoopbackEndpointId (const juce::String& endpointId)
{
    {
        const juce::ScopedLock lock (loopbackStateLock);
        loopbackEndpointId = endpointId;
    }

    if (loopbackWanted.load (std::memory_order_relaxed))
    {
        juce::String error;
        startLoopback (error);
    }

    sendChangeMessage();
}

juce::String AudioEngine::getLoopbackEndpointId() const
{
    const juce::ScopedLock lock (loopbackStateLock);
    return loopbackEndpointId;
}

juce::String AudioEngine::getLoopbackEndpointName() const
{
    if (loopback.isRunning())
        return loopback.getEndpointName();

    const auto selected = getLoopbackEndpointId();

    if (selected.isEmpty())
        return "Belum dipilih";

    for (const auto& endpoint : getLoopbackEndpoints())
        if (endpoint.id == selected)
            return endpoint.name;

    return "Endpoint tidak tersedia";
}

juce::String AudioEngine::normaliseEndpointName (juce::String name)
{
    if (name.endsWithIgnoreCase (" (default)"))
        name = name.dropLastCharacters (10);

    return name.trim().toLowerCase();
}

bool AudioEngine::restartLoopbackForDevice()
{
    const auto target = loopback.getTargetSampleRate();
    const auto rate = currentSampleRate.load (std::memory_order_relaxed);

    if (! loopback.isRunning() || rate <= 0.0 || std::abs (target - rate) <= 1.0)
        return false;

    juce::String error;

    if (! startLoopback (error))
    {
        diagnostics.log ("Loopback tidak dapat mengikuti sample rate perangkat: " + error);
        return false;
    }

    diagnostics.log ("Loopback di-restart pada " + juce::String (rate, 1) + " Hz.");
    return true;
}

bool AudioEngine::checkLoopbackConflict (juce::String& explanation) const
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
    {
        explanation = "Output audio belum aktif, loopback tidak dapat dimulai.";
        return true;
    }

    const auto selectedId = getLoopbackEndpointId();
    const auto outputName = normaliseEndpointName (device->getName());

    for (const auto& endpoint : getLoopbackEndpoints())
    {
        if (! selectedId.isEmpty() && endpoint.id != selectedId)
            continue;

        if (normaliseEndpointName (endpoint.name) == outputName)
        {
            explanation = "Endpoint loopback sama dengan output master. Ini akan membuat umpan balik internal. "
                          "Pilih endpoint lain atau gunakan virtual audio device terpisah.";
            return true;
        }
    }

    return false;
}

bool AudioEngine::startLoopback (juce::String& errorMessage)
{
    if (! deviceRunning.load (std::memory_order_relaxed))
    {
        errorMessage = "Audio engine belum berjalan.";
        return false;
    }

    auto endpointId = getLoopbackEndpointId();

    if (endpointId.isEmpty())
    {
        endpointId = LoopbackCapture::getDefaultEndpointId();

        if (endpointId.isNotEmpty())
        {
            const juce::ScopedLock lock (loopbackStateLock);
            loopbackEndpointId = endpointId;
        }
    }

    if (endpointId.isEmpty())
    {
        errorMessage = "Tidak ada endpoint output Windows yang aktif untuk ditangkap.";
        {
            const juce::ScopedLock lock (loopbackStateLock);
            loopbackEndpointId.clear();
            lastLoopbackError = errorMessage;
        }

        loopbackWanted.store (false, std::memory_order_relaxed);
        sendChangeMessage();
        return false;
    }

    juce::String conflict;

    if (checkLoopbackConflict (conflict))
    {
        errorMessage = conflict;
        {
            const juce::ScopedLock lock (loopbackStateLock);
            lastLoopbackError = errorMessage;
        }
        loopbackWanted.store (false, std::memory_order_relaxed);
        sendChangeMessage();
        return false;
    }

    if (loopback.isRunning())
        loopback.stop();

    const auto started = loopback.start (endpointId, currentSampleRate.load (std::memory_order_relaxed));

    if (! started)
    {
        errorMessage = loopback.getLastError();

        if (errorMessage.isEmpty())
            errorMessage = "WASAPI loopback gagal dimulai.";

        {
            const juce::ScopedLock lock (loopbackStateLock);
            lastLoopbackError = errorMessage;
        }

        loopbackWanted.store (false, std::memory_order_relaxed);
        diagnostics.log ("Loopback gagal: " + errorMessage);
        sendChangeMessage();
        return false;
    }

    consecutiveLoopbackUnderruns = 0;
    loopbackWanted.store (true, std::memory_order_relaxed);

    {
        const juce::ScopedLock lock (loopbackStateLock);
        lastLoopbackError.clear();
    }

    diagnostics.log ("Loopback aktif: " + loopback.getEndpointName()
                     + " @ " + juce::String (loopback.getCaptureSampleRate(), 1) + " Hz");
    sendChangeMessage();
    return true;
}

void AudioEngine::stopLoopback()
{
    loopbackWanted.store (false, std::memory_order_relaxed);
    loopback.stop();
    diagnostics.log ("Loopback dihentikan.");
    sendChangeMessage();
}

juce::String AudioEngine::getLoopbackError() const
{
    const juce::ScopedLock lock (loopbackStateLock);

    if (lastLoopbackError.isNotEmpty())
        return lastLoopbackError;

    return loopback.getLastError();
}

juce::String AudioEngine::getLoopbackWarning() const
{
    if (! loopback.isRunning())
        return {};

    return "WASAPI loopback menangkap seluruh audio sistem pada endpoint \""
         + loopback.getEndpointName()
         + "\", bukan hanya YouTube. Gunakan endpoint/virtual cable terpisah dan jangan "
           "memilih endpoint yang sama dengan output master.";
}

//==============================================================================
AudioEngine::Meters AudioEngine::getMeters() const
{
    Meters meters;

    meters.micPeakDb = meterMic.load (std::memory_order_relaxed);
    meters.vocalChainPeakDb = meterVocalChain.load (std::memory_order_relaxed);
    meters.vocalBusPeakDb = meterVocalBus.load (std::memory_order_relaxed);
    meters.musicBoosterPeakDb = meterMusicBooster.load (std::memory_order_relaxed);
    meters.musicBusPeakDb = meterMusicBus.load (std::memory_order_relaxed);
    meters.samplerPeakDb = meterSampler.load (std::memory_order_relaxed);
    meters.masterPeakDb = meterMaster.load (std::memory_order_relaxed);
    meters.masterRmsDb = meterMasterRms.load (std::memory_order_relaxed);
    meters.loopbackPeakDb = loopback.getPeakDb();

    meters.vocalCompressorGrDb = vocalChain.getCompressorGainReductionDb();
    meters.vocalDeEsserGrDb = vocalChain.getDeEsserGainReductionDb();
    meters.musicCompressorGrDb = musicBooster.getCompressorGainReductionDb();
    meters.musicLimiterGrDb = musicBooster.getLimiterGainReductionDb();
    meters.smartBalanceGrDb = smartBalance.getGainReductionDb();
    meters.masterLimiterGrDb = masterLimiter.getGainReductionDb();

    meters.pitchSemitones = params.get (ParamId::pitchBypass) ? 0.0f : params.get (ParamId::transpose);
    meters.detectedMidiNote = vocalChain.getDetectedMidiNote();
    meters.pitchConfidence = vocalChain.getPitchConfidence();
    meters.autoBoostDb = musicBooster.getAutoBoostAppliedDb();

    meters.pitchVoiced = vocalChain.isPitchVoiced();
    meters.gateOpen = vocalChain.getGateOpen();
    meters.clip = clipLatched.load (std::memory_order_relaxed)
               || vocalChain.getClipped()
               || musicBooster.getClipFlag();
    meters.loopbackRunning = loopback.isRunning();
    meters.loopbackSignal = loopback.hasSignal();
    meters.autoBoostState = musicBooster.getAutoBoostState();
    meters.loopbackOverflows = loopback.getOverflowCount();
    meters.captureSampleRate = loopback.getCaptureSampleRate();

    const auto stats = diagnostics.getAudioStats();
    meters.dropouts = (int) stats.dropoutCount;
    meters.latencyMs = getTotalLatencyMs();

    const auto blockSize = currentBlockSize.load (std::memory_order_relaxed);
    const auto sampleRate = currentSampleRate.load (std::memory_order_relaxed);

    if (blockSize > 0 && sampleRate > 0.0 && stats.callbackCount > 0)
    {
        const auto budgetMs = (double) blockSize * 1000.0 / sampleRate;
        meters.cpuPercent = juce::jlimit (0.0, 999.0, stats.averageCallbackMs / juce::jmax (0.001, budgetMs) * 100.0);
    }

    return meters;
}

void AudioEngine::resetClipIndicators()
{
    clipLatched.store (false, std::memory_order_relaxed);
    vocalChain.clearClipFlag();
    musicBooster.clearClipFlag();
}

void AudioEngine::requestAutoBoost()
{
    musicBooster.requestAutoBoost();
}

void AudioEngine::resetAutoBoost()
{
    musicBooster.resetAutoGain();
}

//==============================================================================
void AudioEngine::prepare (double newSampleRate, int blockSize)
{
    const auto rate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    const auto samples = juce::jlimit (32, 16384, blockSize > 0 ? blockSize : 512);

    currentSampleRate.store (rate);
    preparedBlockSize.store (samples);

    micL.assign ((size_t) samples, 0.0f);
    micR.assign ((size_t) samples, 0.0f);
    vocalL.assign ((size_t) samples, 0.0f);
    vocalR.assign ((size_t) samples, 0.0f);
    musicL.assign ((size_t) samples, 0.0f);
    musicR.assign ((size_t) samples, 0.0f);
    musicShiftL.assign ((size_t) samples, 0.0f);
    musicShiftR.assign ((size_t) samples, 0.0f);
    samplerL.assign ((size_t) samples, 0.0f);
    samplerR.assign ((size_t) samples, 0.0f);
    masterL.assign ((size_t) samples, 0.0f);
    masterR.assign ((size_t) samples, 0.0f);

    vocalChain.prepare (rate, samples);
    musicPitch.prepare (rate, 10);
    musicBooster.prepare (rate, samples);
    smartBalance.prepare (rate);
    masterLimiter.prepare (rate, samples, true);
    vocalAlignment.prepare (rate, 0.2);
    musicAlignment.prepare (rate, 0.2);
    sampler.prepare (rate, samples);
    pluginHost.prepare (rate, samples);

    masterGainSmoother.reset (rate, 0.03);
    masterGainSmoother.setCurrentAndTargetValue (dbToGain (params.get (ParamId::masterGainDb)));

    testTonePhase = 0.0f;
    consecutiveLoopbackUnderruns = 0;
    clipLatched.store (false, std::memory_order_relaxed);

    meterMic.store (-100.0f);
    meterVocalChain.store (-100.0f);
    meterVocalBus.store (-100.0f);
    meterMusicBooster.store (-100.0f);
    meterMusicBus.store (-100.0f);
    meterSampler.store (-100.0f);
    meterMaster.store (-100.0f);
    meterMasterRms.store (-100.0f);

    updateDelayAlignment();
    diagnostics.log ("Engine disiapkan: " + juce::String (rate, 1) + " Hz / "
                     + juce::String (samples) + " samples.");
}

void AudioEngine::updateDelayAlignment() noexcept
{
    const auto musicLatency = (params.getBool (ParamId::pitchBypass) ? 0 : musicPitch.getLatencySamples())
                            + (params.getBool (ParamId::boosterBypass) ? 0 : musicBooster.getLatencySamples());
    const auto vocalLatency = vocalChain.getLatencySamples() + pluginHost.getChainLatencySamples (false);
    const auto alignedLatency = juce::jmax (musicLatency, vocalLatency);

    vocalAlignment.setTargetSamples (alignedLatency - vocalLatency);
    musicAlignment.setTargetSamples (alignedLatency - musicLatency);

    const auto masterLatency = pluginHost.getChainLatencySamples (true)
                             + (params.getBool (ParamId::masterLimiterEnabled)
                                    ? masterLimiter.getLatencySamples() : 0);

    const auto sampleRate = currentSampleRate.load (std::memory_order_relaxed);

    if (sampleRate > 0.0)
        engineLatencyMs.store ((double) (alignedLatency + masterLatency) * 1000.0 / sampleRate,
                               std::memory_order_relaxed);
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const auto rate = device->getCurrentSampleRate();
    const auto samples = device->getCurrentBufferSizeSamples();

    prepare (rate, samples);
    currentSampleRate.store (rate);
    currentBlockSize.store (samples);
    deviceRunning.store (true);

    triggerAsyncUpdate();
}

void AudioEngine::audioDeviceStopped()
{
    deviceRunning.store (false);
    triggerAsyncUpdate();
}

void AudioEngine::audioDeviceError (const juce::String& errorMessage)
{
    {
        const juce::ScopedTryLock lock (deviceErrorLock);

        if (lock.isLocked())
            lastDeviceError = errorMessage;
    }

    triggerAsyncUpdate();
}

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
        triggerAsyncUpdate();
}

void AudioEngine::handleAsyncUpdate()
{
    if (! deviceRunning.load (std::memory_order_relaxed))
    {
        if (loopback.isRunning())
            loopback.stop();
    }
    else if (loopbackWanted.load (std::memory_order_relaxed) && ! loopback.isRunning())
    {
        juce::String error;

        if (! startLoopback (error))
            diagnostics.log ("Loopback tidak dapat dimulai ulang: " + error);
    }
    else
    {
        restartLoopbackForDevice();
    }

    juce::String deviceError;
    {
        const juce::ScopedTryLock lock (deviceErrorLock);

        if (lock.isLocked() && lastDeviceError.isNotEmpty())
        {
            deviceError = lastDeviceError;
            lastDeviceError.clear();
        }
    }

    if (deviceError.isNotEmpty())
        diagnostics.log ("Error perangkat audio: " + deviceError);

    sendChangeMessage();
}

//==============================================================================
void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (context);

    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[channel], numSamples);

    const auto prepared = preparedBlockSize.load (std::memory_order_relaxed);

    if (! deviceRunning.load (std::memory_order_relaxed)
        || prepared <= 0
        || numSamples <= 0
        || numOutputChannels <= 0
        || outputChannelData == nullptr)
    {
        return;
    }

    const auto startTicks = juce::Time::getMillisecondCounterHiRes();
    int offset = 0;
    bool clipped = false;

    while (offset < numSamples)
    {
        const auto chunk = juce::jmin (prepared, numSamples - offset);
        processChunk (inputChannelData, numInputChannels, outputChannelData, numOutputChannels,
                      offset, chunk);
        offset += chunk;
    }

    clipped = clipLatched.load (std::memory_order_relaxed)
           || vocalChain.getClipped()
           || musicBooster.getClipFlag();

    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTicks;
    const auto budgetMs = currentSampleRate.load (std::memory_order_relaxed) > 0.0
                            ? (double) numSamples * 1000.0 / currentSampleRate.load (std::memory_order_relaxed)
                            : 0.0;
    const auto overloaded = budgetMs > 0.0 && elapsed > budgetMs * 0.95;

    diagnostics.recordAudioBlock (numSamples, elapsed, clipped, overloaded);
}

void AudioEngine::processChunk (const float* const* inputChannelData, int numInputChannels,
                                float* const* outputChannelData, int numOutputChannels,
                                int sampleOffset, int numSamples) noexcept
{
    // --- microphone -----------------------------------------------------------
    if (numInputChannels > 0 && inputChannelData != nullptr && inputChannelData[0] != nullptr)
    {
        juce::FloatVectorOperations::copy (micL.data(), inputChannelData[0] + sampleOffset, numSamples);

        if (numInputChannels > 1 && inputChannelData[1] != nullptr)
            juce::FloatVectorOperations::copy (micR.data(), inputChannelData[1] + sampleOffset, numSamples);
        else
            juce::FloatVectorOperations::copy (micR.data(), micL.data(), numSamples);
    }
    else
    {
        juce::FloatVectorOperations::clear (micL.data(), numSamples);
        juce::FloatVectorOperations::clear (micR.data(), numSamples);
    }

    vocalChain.process (micL.data(), micR.data(), vocalL.data(), vocalR.data(), numSamples);
    pluginHost.processVocal (vocalL.data(), vocalR.data(), numSamples);

    // --- YouTube music bus (WASAPI loopback) ---------------------------------
    const auto captured = loopback.read (musicL.data(), musicR.data(), numSamples);

    if (captured < numSamples)
    {
        juce::FloatVectorOperations::clear (musicL.data() + captured, numSamples - captured);
        juce::FloatVectorOperations::clear (musicR.data() + captured, numSamples - captured);

        if (loopback.isRunning())
            ++consecutiveLoopbackUnderruns;
    }
    else
    {
        consecutiveLoopbackUnderruns = 0;
    }

    if (params.getBool (ParamId::pitchBypass))
    {
        juce::FloatVectorOperations::copy (musicShiftL.data(), musicL.data(), numSamples);
        juce::FloatVectorOperations::copy (musicShiftR.data(), musicR.data(), numSamples);
    }
    else
    {
        musicPitch.setTransposeSemitones (params.get (ParamId::transpose));
        musicPitch.processStereo (musicL.data(), musicR.data(),
                                  musicShiftL.data(), musicShiftR.data(), numSamples);
    }

    musicBooster.process (musicShiftL.data(), musicShiftR.data(), numSamples);

    // --- latency alignment ----------------------------------------------------
    updateDelayAlignment();
    vocalAlignment.process (vocalL.data(), vocalR.data(), numSamples);
    musicAlignment.process (musicShiftL.data(), musicShiftR.data(), numSamples);

    smartBalance.process (musicShiftL.data(), musicShiftR.data(), vocalL.data(), numSamples);

    // --- sampler --------------------------------------------------------------
    juce::FloatVectorOperations::clear (samplerL.data(), numSamples);
    juce::FloatVectorOperations::clear (samplerR.data(), numSamples);
    sampler.process (samplerL.data(), samplerR.data(), numSamples);

    // --- bus faders, mute and solo -------------------------------------------
    const auto anySolo = params.getBool (ParamId::micSolo)
                      || params.getBool (ParamId::musicSolo)
                      || params.getBool (ParamId::samplerSolo);

    const BusRouter::BusState micBus { params.get (ParamId::micMixDb),
                                       params.getBool (ParamId::micMute),
                                       params.getBool (ParamId::micSolo) };
    const BusRouter::BusState musicBus { params.get (ParamId::musicMixDb),
                                         params.getBool (ParamId::musicMute),
                                         params.getBool (ParamId::musicSolo) };
    const BusRouter::BusState samplerBus { params.get (ParamId::samplerMixDb),
                                           params.getBool (ParamId::samplerMute),
                                           params.getBool (ParamId::samplerSolo) };

    const auto micGain = BusRouter::gainForBus (micBus, anySolo);
    const auto musicGain = BusRouter::gainForBus (musicBus, anySolo);
    const auto samplerGain = BusRouter::gainForBus (samplerBus, anySolo);

    juce::FloatVectorOperations::clear (masterL.data(), numSamples);
    juce::FloatVectorOperations::clear (masterR.data(), numSamples);

    BusRouter::sumInto (masterL.data(), masterR.data(), vocalL.data(), vocalR.data(), numSamples, micGain);
    BusRouter::sumInto (masterL.data(), masterR.data(), musicShiftL.data(), musicShiftR.data(), numSamples, musicGain);
    BusRouter::sumInto (masterL.data(), masterR.data(), samplerL.data(), samplerR.data(), numSamples, samplerGain);

    if (params.getBool (ParamId::testToneEnabled))
    {
        const auto amplitude = dbToGain (params.get (ParamId::testToneDb));
        const auto increment = kTwoPi * 1000.0f / (float) currentSampleRate.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto sample = std::sin (testTonePhase) * amplitude;
            masterL[(size_t) i] += sample;
            masterR[(size_t) i] += sample;
            testTonePhase += increment;

            if (testTonePhase >= kTwoPi)
                testTonePhase -= kTwoPi;
        }
    }

    pluginHost.processMaster (masterL.data(), masterR.data(), numSamples);

    masterGainSmoother.setTargetValue (dbToGain (params.get (ParamId::masterGainDb)));

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = masterGainSmoother.getNextValue();
        masterL[(size_t) i] *= gain;
        masterR[(size_t) i] *= gain;
    }

    if (params.getBool (ParamId::masterLimiterEnabled))
    {
        masterLimiter.setCeilingDb (params.get (ParamId::masterLimiterCeilingDb));
        masterLimiter.setReleaseMs (120.0f);
        masterLimiter.process (masterL.data(), masterR.data(), numSamples);
    }

    bool clipped = false;
    float masterPeak = 0.0f;
    double masterSumSquares = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        auto left = masterL[(size_t) i];
        auto right = masterR[(size_t) i];

        if (! std::isfinite (left))
        {
            left = 0.0f;
            clipped = true;
        }

        if (! std::isfinite (right))
        {
            right = 0.0f;
            clipped = true;
        }

        if (std::abs (left) > 1.0f || std::abs (right) > 1.0f)
            clipped = true;

        left = juce::jlimit (-1.0f, 1.0f, left);
        right = juce::jlimit (-1.0f, 1.0f, right);

        masterL[(size_t) i] = left;
        masterR[(size_t) i] = right;
        masterPeak = juce::jmax (masterPeak, std::abs (left), std::abs (right));
        masterSumSquares += (double) left * left + (double) right * right;
    }

    if (numOutputChannels > 0 && outputChannelData[0] != nullptr)
        juce::FloatVectorOperations::copy (outputChannelData[0] + sampleOffset, masterL.data(), numSamples);

    if (numOutputChannels > 1 && outputChannelData[1] != nullptr)
        juce::FloatVectorOperations::copy (outputChannelData[1] + sampleOffset, masterR.data(), numSamples);

    if (clipped)
        clipLatched.store (true, std::memory_order_relaxed);

    const auto micPeakDb = vocalChain.getInputPeakDb();
    const auto vocalBusPeak = (micGain > 0.0f ? peakOf (vocalL.data(), vocalR.data(), numSamples) * micGain : 0.0f)
                            * (params.getBool (ParamId::micMute) ? 0.0f : 1.0f);
    const auto musicBusPeak = peakOf (musicShiftL.data(), musicShiftR.data(), numSamples) * musicGain;
    const auto samplerBusPeak = peakOf (samplerL.data(), samplerR.data(), numSamples) * samplerGain;

    updateMetersAfterBlock (micPeakDb, vocalBusPeak, musicBusPeak, samplerBusPeak,
                            masterPeak, masterSumSquares, numSamples, clipped);
}

void AudioEngine::updateMetersAfterBlock (float micPeakDb,
                                          float vocalBusPeak,
                                          float musicBusPeak,
                                          float samplerBusPeak,
                                          float masterPeak,
                                          double masterSumSquares,
                                          int numSamples,
                                          bool clipped) noexcept
{
    auto update = [] (std::atomic<float>& meter, float target)
    {
        const auto previous = meter.load (std::memory_order_relaxed);
        meter.store (smoothMeterDb (previous, target), std::memory_order_relaxed);
    };

    update (meterMic, micPeakDb);
    update (meterVocalChain, vocalChain.getOutputPeakDb());
    update (meterVocalBus, gainToDb (vocalBusPeak));
    update (meterMusicBooster, musicBooster.getPeakDb());
    update (meterMusicBus, gainToDb (musicBusPeak));
    update (meterSampler, gainToDb (samplerBusPeak));
    update (meterMaster, gainToDb (masterPeak));

    const auto rms = (float) std::sqrt (masterSumSquares / (double) juce::jmax (1, numSamples * 2));
    meterMasterRms.store (gainToDb (rms), std::memory_order_relaxed);

    if (clipped)
        clipLatched.store (true, std::memory_order_relaxed);
}

} // namespace rr
