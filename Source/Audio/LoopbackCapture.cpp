#include "Audio/LoopbackCapture.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

#if JUCE_WINDOWS
 #include <windows.h>
 #include <objbase.h>
 #include <mmdeviceapi.h>
 #include <audioclient.h>
 #include <ksmedia.h>
 #include <propsys.h>
 #include <propvarutil.h>
 #include <functiondiscoverykeys_devpkey.h>
 #include <wrl/client.h>
#endif

namespace rr
{
namespace
{
#if JUCE_WINDOWS
    struct ComApartment
    {
        ComApartment()
        {
            result = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
            initialised = SUCCEEDED (result);

            // JUCE (and WebView2) already initialise COM as STA on the message
            // thread, so requesting MTA returns RPC_E_CHANGED_MODE. COM is still
            // usable there; bailing out would hide every render endpoint.
            usable = initialised || result == RPC_E_CHANGED_MODE;
        }

        ~ComApartment()
        {
            if (initialised)
                CoUninitialize();
        }

        HRESULT result = E_FAIL;
        bool initialised = false;
        bool usable = false;
    };

    juce::String endpointFriendlyName (IMMDevice* device)
    {
        Microsoft::WRL::ComPtr<IPropertyStore> store;

        if (device == nullptr || FAILED (device->OpenPropertyStore (STGM_READ, store.GetAddressOf())))
            return {};

        PROPVARIANT value;
        PropVariantInit (&value);
        juce::String name;

        if (SUCCEEDED (store->GetValue (PKEY_Device_FriendlyName, &value))
            && value.vt == VT_LPWSTR
            && value.pwszVal != nullptr)
            name = juce::String (value.pwszVal);

        PropVariantClear (&value);
        return name;
    }

    juce::String hresultToString (HRESULT result)
    {
        return "0x" + juce::String::toHexString ((int) result).paddedLeft ('0', 8);
    }
#endif

    constexpr int fifoSize = 65536;
}

LoopbackCapture::LoopbackCapture()
    : juce::Thread ("WASAPI loopback")
    , fifo (fifoSize)
{
    fifoLeft.assign ((size_t) fifoSize, 0.0f);
    fifoRight.assign ((size_t) fifoSize, 0.0f);
    sourceLeft.assign (8192, 0.0f);
    sourceRight.assign (8192, 0.0f);
    resampledLeft.assign (16384, 0.0f);
    resampledRight.assign (16384, 0.0f);
}

LoopbackCapture::~LoopbackCapture()
{
    stop();
}

juce::Array<LoopbackCapture::EndpointInfo> LoopbackCapture::enumerateEndpoints()
{
    juce::Array<EndpointInfo> endpoints;

#if JUCE_WINDOWS
    ComApartment com;

    if (! com.usable)
        return endpoints;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;

    if (FAILED (CoCreateInstance (__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**> (enumerator.GetAddressOf()))))
        return endpoints;

    Microsoft::WRL::ComPtr<IMMDevice> defaultDevice;
    juce::String defaultId;

    if (SUCCEEDED (enumerator->GetDefaultAudioEndpoint (eRender, eConsole, defaultDevice.GetAddressOf())))
    {
        LPWSTR id = nullptr;

        if (SUCCEEDED (defaultDevice->GetId (&id)) && id != nullptr)
        {
            defaultId = juce::String (id);
            CoTaskMemFree (id);
        }
    }

    Microsoft::WRL::ComPtr<IMMDeviceCollection> collection;

    if (FAILED (enumerator->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE, collection.GetAddressOf())))
        return endpoints;

    UINT count = 0;

    if (FAILED (collection->GetCount (&count)))
        return endpoints;

    for (UINT i = 0; i < count; ++i)
    {
        Microsoft::WRL::ComPtr<IMMDevice> device;

        if (FAILED (collection->Item (i, device.GetAddressOf())) || device == nullptr)
            continue;

        LPWSTR rawId = nullptr;

        if (FAILED (device->GetId (&rawId)) || rawId == nullptr)
            continue;

        EndpointInfo info;
        info.id = juce::String (rawId);
        CoTaskMemFree (rawId);

        info.name = endpointFriendlyName (device.Get());

        if (info.name.isEmpty())
            info.name = "Output audio";

        info.isDefault = info.id == defaultId;
        info.name += info.isDefault ? " (default)" : "";

        endpoints.add (info);
    }
#endif

    return endpoints;
}

juce::String LoopbackCapture::getDefaultEndpointId()
{
#if JUCE_WINDOWS
    ComApartment com;

    if (! com.usable)
        return {};

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;

    if (FAILED (CoCreateInstance (__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**> (enumerator.GetAddressOf()))))
        return {};

    Microsoft::WRL::ComPtr<IMMDevice> device;

    if (FAILED (enumerator->GetDefaultAudioEndpoint (eRender, eConsole, device.GetAddressOf())))
        return {};

    LPWSTR id = nullptr;
    juce::String result;

    if (SUCCEEDED (device->GetId (&id)) && id != nullptr)
    {
        result = juce::String (id);
        CoTaskMemFree (id);
    }

    return result;
#else
    return {};
#endif
}

bool LoopbackCapture::start (const juce::String& endpointId, double engineSampleRate)
{
    if (isThreadRunning())
        stop();

#if ! JUCE_WINDOWS
    juce::ignoreUnused (endpointId, engineSampleRate);
    setError ("Loopback capture hanya tersedia di Windows.");
    return false;
#else
    requestedSampleRate = engineSampleRate > 0.0 ? engineSampleRate : 48000.0;

    // Keep roughly 40 ms of music buffered so normal packet jitter does not
    // produce gaps, and only drop data when the backlog grows beyond 200 ms.
    targetBacklogSamples = juce::jlimit (512, 4096, (int) (requestedSampleRate * 0.04));
    maxBacklogSamples = juce::jlimit (2048, 32768, (int) (requestedSampleRate * 0.20));
    primed.store (false, std::memory_order_relaxed);

    auto requestedId = endpointId;

    if (requestedId.isEmpty())
        requestedId = getDefaultEndpointId();

    if (requestedId.isEmpty())
    {
        setError ("Tidak ada endpoint output Windows yang aktif untuk ditangkap.");
        return false;
    }

    juce::String name = "Output audio";

    for (const auto& endpoint : enumerateEndpoints())
    {
        if (endpoint.id == requestedId)
        {
            name = endpoint.name;
            break;
        }
    }

    {
        const juce::ScopedLock lock (stateLock);
        currentEndpointId = requestedId;
        currentEndpointName = name;
        lastError.clear();
    }

    fifo.reset();
    peakDb.store (-100.0f, std::memory_order_relaxed);
    captureSampleRate.store (0.0, std::memory_order_relaxed);
    overflowCount.store (0, std::memory_order_relaxed);
    targetSampleRate.store (requestedSampleRate, std::memory_order_relaxed);
    sessionActive.store (false, std::memory_order_relaxed);
    resamplePosition = 0.0;
    driftCorrection = 1.0;
    haveLastSource = false;
    lastSourceL = lastSourceR = 0.0f;

    return startThread (juce::Thread::Priority::high);
#endif
}

void LoopbackCapture::stop()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();

        if (auto* handle = static_cast<HANDLE> (wakeEvent.load (std::memory_order_relaxed)))
            SetEvent (handle);

        stopThread (3000);
    }

    sessionActive.store (false, std::memory_order_relaxed);
    peakDb.store (-100.0f, std::memory_order_relaxed);
}

void LoopbackCapture::restart()
{
    const auto id = getEndpointId();
    const auto rate = requestedSampleRate;
    stop();
    start (id, rate);
}

void LoopbackCapture::setError (const juce::String& message)
{
    const juce::ScopedLock lock (stateLock);
    lastError = message;
}

void LoopbackCapture::clearError()
{
    const juce::ScopedLock lock (stateLock);
    lastError.clear();
}

juce::String LoopbackCapture::getLastError() const
{
    const juce::ScopedLock lock (stateLock);
    return lastError;
}

juce::String LoopbackCapture::getEndpointName() const
{
    const juce::ScopedLock lock (stateLock);
    return currentEndpointName;
}

juce::String LoopbackCapture::getEndpointId() const
{
    const juce::ScopedLock lock (stateLock);
    return currentEndpointId;
}

void LoopbackCapture::updatePeak (const float* left, const float* right, int numSamples) noexcept
{
    float peak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        peak = juce::jmax (peak, std::abs (left[i]), std::abs (right[i]));
    }

    const auto newDb = juce::Decibels::gainToDecibels (peak, -100.0f);
    const auto oldDb = peakDb.load (std::memory_order_relaxed);
    peakDb.store (juce::jmax (newDb, oldDb - 3.0f), std::memory_order_relaxed);
}

void LoopbackCapture::appendResampled (const float* left, const float* right, int numSamples)
{
    if (numSamples <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

    if (size1 > 0)
    {
        std::copy (left, left + size1, fifoLeft.begin() + start1);
        std::copy (right, right + size1, fifoRight.begin() + start1);
    }

    if (size2 > 0)
    {
        std::copy (left + size1, left + size1 + size2, fifoLeft.begin() + start2);
        std::copy (right + size1, right + size1 + size2, fifoRight.begin() + start2);
    }

    const auto written = size1 + size2;
    fifo.finishedWrite (written);

    if (written < numSamples)
        overflowCount.fetch_add (numSamples - written, std::memory_order_relaxed);
    updatePeak (left, right, numSamples);
}

bool LoopbackCapture::processPacket (const void* data, int numFrames, bool isSilent,
                                     int numChannels, int bytesPerSample, bool isFloat,
                                     double sourceSampleRate)
{
    if (numFrames <= 0 || numChannels <= 0)
        return true;

    const auto requiredSourceSize = (size_t) numFrames;

    if (sourceLeft.size() < requiredSourceSize)
    {
        sourceLeft.resize (requiredSourceSize);
        sourceRight.resize (requiredSourceSize);
    }

    if (isSilent || data == nullptr)
    {
        std::fill (sourceLeft.begin(), sourceLeft.begin() + numFrames, 0.0f);
        std::fill (sourceRight.begin(), sourceRight.begin() + numFrames, 0.0f);
    }
    else
    {
        for (int frame = 0; frame < numFrames; ++frame)
        {
            float l = 0.0f;
            float r = 0.0f;

            if (isFloat && bytesPerSample == 4)
            {
                const auto* samples = static_cast<const float*> (data);
                const auto* frameData = samples + (size_t) frame * (size_t) numChannels;
                l = frameData[0];
                r = numChannels > 1 ? frameData[1] : l;
            }
            else if (! isFloat && bytesPerSample == 2)
            {
                const auto* samples = static_cast<const juce::int16*> (data);
                const auto* frameData = samples + (size_t) frame * (size_t) numChannels;
                l = (float) frameData[0] / 32768.0f;
                r = numChannels > 1 ? (float) frameData[1] / 32768.0f : l;
            }
            else if (! isFloat && bytesPerSample == 4)
            {
                const auto* samples = static_cast<const juce::int32*> (data);
                const auto* frameData = samples + (size_t) frame * (size_t) numChannels;
                l = (float) ((double) frameData[0] / 2147483648.0);
                r = numChannels > 1 ? (float) ((double) frameData[1] / 2147483648.0) : l;
            }
            else
            {
                setError ("Format mix endpoint tidak didukung (hanya float32, PCM16, atau PCM32).");
                return false;
            }

            sourceLeft[(size_t) frame] = std::isfinite (l) ? l : 0.0f;
            sourceRight[(size_t) frame] = std::isfinite (r) ? r : 0.0f;
        }
    }

    const auto nominalStep = sourceSampleRate / juce::jmax (8000.0, requestedSampleRate);

    // Capture and playback endpoints run on independent clocks.  With a fixed
    // step the FIFO backlog ramps until it reaches the drop threshold and the
    // music bus gets an audible gap.  A slow proportional feedback trims the
    // resample ratio by at most +/-0.4% (~7 cents) and is slewed over about a
    // second so the pitch correction is inaudible.  The gain is deliberately
    // low: a faster loop reacts to block jitter and makes the backlog swing.
    if (primed.load (std::memory_order_relaxed) && targetBacklogSamples > 0)
    {
        const auto backlogErrorMs = ((double) fifo.getNumReady() - (double) targetBacklogSamples)
                                      * 1000.0 / juce::jmax (8000.0, requestedSampleRate);
        const auto requestedCorrection = 1.0 + juce::jlimit (-0.004, 0.004, backlogErrorMs * 0.0001);
        driftCorrection += (requestedCorrection - driftCorrection) * 0.01;
    }

    const auto step = nominalStep * driftCorrection;
    auto maximumOutput = (int) std::ceil ((double) numFrames / juce::jmax (0.1, step)) + 4;

    if (maximumOutput <= 0)
        return true;

    if ((int) resampledLeft.size() < maximumOutput)
    {
        resampledLeft.resize ((size_t) maximumOutput);
        resampledRight.resize ((size_t) maximumOutput);
    }

    int outputCount = 0;

    auto emit = [&] (float l, float r)
    {
        if (outputCount < maximumOutput)
        {
            resampledLeft[(size_t) outputCount] = l;
            resampledRight[(size_t) outputCount] = r;
            ++outputCount;
        }
    };

    if (haveLastSource && resamplePosition < 0.0)
    {
        const auto t = (float) juce::jlimit (0.0, 1.0, resamplePosition + 1.0);
        emit (lastSourceL * (1.0f - t) + sourceLeft[0] * t,
              lastSourceR * (1.0f - t) + sourceRight[0] * t);
        resamplePosition += step;

        // With a step just under 1 the carry can land a hair below zero.  The
        // main loop rejects negative bases, so leaving it there would push the
        // position further negative on every packet and resample everything
        // away.  The next valid position is the current packet's first sample.
        if (resamplePosition < 0.0)
            resamplePosition = 0.0;
    }

    while (resamplePosition < (double) numFrames - 1.0)
    {
        const auto base = (int) std::floor (resamplePosition);
        const auto fraction = (float) (resamplePosition - (double) base);

        if (base < 0)
            break;

        if (base + 1 >= numFrames)
            break;

        emit (sourceLeft[(size_t) base] * (1.0f - fraction) + sourceLeft[(size_t) base + 1] * fraction,
              sourceRight[(size_t) base] * (1.0f - fraction) + sourceRight[(size_t) base + 1] * fraction);

        resamplePosition += step;
    }

    resamplePosition -= (double) numFrames;
    lastSourceL = sourceLeft[(size_t) numFrames - 1];
    lastSourceR = sourceRight[(size_t) numFrames - 1];
    haveLastSource = true;

    appendResampled (resampledLeft.data(), resampledRight.data(), outputCount);
    return true;
}

void LoopbackCapture::run()
{
#if ! JUCE_WINDOWS
    setError ("Loopback capture hanya tersedia di Windows.");
#else
    ComApartment com;

    if (! com.usable)
    {
        setError ("Gagal menginisialisasi COM untuk loopback capture. Kode " + hresultToString (com.result));
        return;
    }

    const auto endpointId = getEndpointId();
    bool formatErrorReported = false;

    while (! threadShouldExit())
    {
        Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
        Microsoft::WRL::ComPtr<IMMDevice> device;
        Microsoft::WRL::ComPtr<IAudioClient> client;
        Microsoft::WRL::ComPtr<IAudioCaptureClient> capture;

        HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       __uuidof(IMMDeviceEnumerator),
                                       reinterpret_cast<void**> (enumerator.GetAddressOf()));

        if (SUCCEEDED (hr))
            hr = enumerator->GetDevice (endpointId.toUTF16().getAddress(), device.GetAddressOf());

        if (SUCCEEDED (hr) && device != nullptr)
            hr = device->Activate (__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                   reinterpret_cast<void**> (client.GetAddressOf()));

        WAVEFORMATEX* mixFormat = nullptr;

        if (SUCCEEDED (hr) && client != nullptr)
            hr = client->GetMixFormat (&mixFormat);

        if (SUCCEEDED (hr) && client != nullptr && mixFormat != nullptr)
        {
            const auto flags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
            hr = client->Initialize (AUDCLNT_SHAREMODE_SHARED, flags, 0, 0, mixFormat, nullptr);
        }

        HANDLE eventHandle = CreateEventW (nullptr, FALSE, FALSE, nullptr);

        if (SUCCEEDED (hr) && client != nullptr && eventHandle != nullptr)
            hr = client->SetEventHandle (eventHandle);

        if (SUCCEEDED (hr) && client != nullptr)
            hr = client->GetService (__uuidof(IAudioCaptureClient),
                                     reinterpret_cast<void**> (capture.GetAddressOf()));

        if (SUCCEEDED (hr) && client != nullptr)
            hr = client->Start();

        if (FAILED (hr) || client == nullptr || capture == nullptr || mixFormat == nullptr || eventHandle == nullptr)
        {
            setError ("Gagal membuka loopback capture: " + hresultToString (hr)
                      + ". Pastikan endpoint output masih tersedia.");
            sessionActive.store (false, std::memory_order_relaxed);
            wakeEvent.store (nullptr, std::memory_order_relaxed);

            if (eventHandle != nullptr)
                CloseHandle (eventHandle);

            if (mixFormat != nullptr)
                CoTaskMemFree (mixFormat);

            client.Reset();
            capture.Reset();
            device.Reset();
            enumerator.Reset();

            for (int i = 0; i < 12 && ! threadShouldExit(); ++i)
                juce::Thread::sleep (100);

            continue;
        }

        wakeEvent.store (eventHandle, std::memory_order_relaxed);

        const auto numChannels = (int) mixFormat->nChannels;
        auto bytesPerSample = (int) (mixFormat->wBitsPerSample / 8);
        bool isFloat = mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;

        if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            const auto* extension = reinterpret_cast<const WAVEFORMATEXTENSIBLE*> (mixFormat);

            if (IsEqualGUID (extension->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
                isFloat = true;
            else if (IsEqualGUID (extension->SubFormat, KSDATAFORMAT_SUBTYPE_PCM))
                isFloat = false;
        }

        captureSampleRate.store ((double) mixFormat->nSamplesPerSec, std::memory_order_relaxed);
        resamplePosition = 0.0;
        driftCorrection = 1.0;
        haveLastSource = false;
        lastSourceL = lastSourceR = 0.0f;
        sessionActive.store (true, std::memory_order_relaxed);
        clearError();
        formatErrorReported = false;

        bool sessionFailed = false;

        while (! threadShouldExit())
        {
            const auto waitResult = WaitForSingleObject (eventHandle, 250);

            if (threadShouldExit())
                break;

            if (waitResult == WAIT_TIMEOUT)
                continue;

            if (waitResult != WAIT_OBJECT_0)
            {
                setError ("Loopback capture berhenti karena event WASAPI gagal.");
                sessionFailed = true;
                break;
            }

            UINT32 packetSize = 0;

            while (SUCCEEDED (capture->GetNextPacketSize (&packetSize)) && packetSize > 0)
            {
                BYTE* packetData = nullptr;
                UINT32 frames = 0;
                DWORD packetFlags = 0;
                UINT64 devicePosition = 0;
                UINT64 qpcPosition = 0;

                const auto bufferResult = capture->GetBuffer (&packetData, &frames, &packetFlags,
                                                              &devicePosition, &qpcPosition);

                if (FAILED (bufferResult))
                {
                    setError ("Loopback capture gagal membaca buffer: " + hresultToString (bufferResult)
                              + ". Endpoint mungkin dicabut atau berubah.");
                    sessionFailed = true;
                    break;
                }

                const auto silent = (packetFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
                const auto ok = processPacket (packetData, (int) frames, silent,
                                               numChannels, bytesPerSample, isFloat,
                                               (double) mixFormat->nSamplesPerSec);

                capture->ReleaseBuffer (frames);

                if (! ok)
                {
                    formatErrorReported = true;
                    sessionFailed = true;
                    break;
                }
            }

            if (sessionFailed)
                break;
        }

        sessionActive.store (false, std::memory_order_relaxed);
        client->Stop();

        wakeEvent.store (nullptr, std::memory_order_relaxed);
        CloseHandle (eventHandle);

        if (mixFormat != nullptr)
            CoTaskMemFree (mixFormat);

        capture.Reset();
        client.Reset();
        device.Reset();
        enumerator.Reset();

        if (! threadShouldExit() && ! formatErrorReported)
        {
            setError (getLastError().isEmpty()
                          ? "Loopback capture terputus. Mencoba menyambung ulang..."
                          : getLastError() + " Mencoba menyambung ulang...");

            for (int i = 0; i < 10 && ! threadShouldExit(); ++i)
                juce::Thread::sleep (100);
        }

        if (formatErrorReported)
            break;
    }
#endif
}

int LoopbackCapture::read (float* left, float* right, int maximumSamples) noexcept
{
    if (left == nullptr || right == nullptr || maximumSamples <= 0)
        return 0;

    // Wait until a small backlog exists before the music bus starts consuming,
    // so packet delivery jitter does not immediately cause gaps.
    if (! primed.load (std::memory_order_relaxed))
    {
        if (fifo.getNumReady() < targetBacklogSamples)
            return 0;

        primed.store (true, std::memory_order_relaxed);
    }

    auto ready = fifo.getNumReady();

    if (ready > maxBacklogSamples)
    {
        const auto toDiscard = ready - targetBacklogSamples;
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (toDiscard, start1, size1, start2, size2);
        fifo.finishedRead (size1 + size2);
        overflowCount.fetch_add (size1 + size2, std::memory_order_relaxed);
        ready = fifo.getNumReady();
    }

    if (ready == 0)
    {
        primed.store (false, std::memory_order_relaxed);
        return 0;
    }

    const auto toRead = juce::jmin (ready, maximumSamples);

    if (toRead <= 0)
        return 0;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToRead (toRead, start1, size1, start2, size2);

    if (size1 > 0)
    {
        std::copy (fifoLeft.begin() + start1, fifoLeft.begin() + start1 + size1, left);
        std::copy (fifoRight.begin() + start1, fifoRight.begin() + start1 + size1, right);
    }

    if (size2 > 0)
    {
        std::copy (fifoLeft.begin() + start2, fifoLeft.begin() + start2 + size2, left + size1);
        std::copy (fifoRight.begin() + start2, fifoRight.begin() + start2 + size2, right + size1);
    }

    fifo.finishedRead (size1 + size2);
    return size1 + size2;
}

} // namespace rr
