#include "Sampler/SamplerEngine.h"

#include <algorithm>
#include <cmath>

namespace rr
{
namespace
{
    constexpr int64_t maxSampleFrames = 48000LL * 300; // 5 minutes at the source rate
    constexpr size_t maxBufferBytes = 300u * 1024u * 1024u;
    constexpr juce::uint32 retentionMs = 10000;
}

SamplerEngine::SamplerEngine()
{
    formats.registerBasicFormats();

    for (int i = 0; i < numPads; ++i)
    {
        padNames.add ("Pad " + juce::String (i + 1));
        padPaths.add ({});
    }

    monoScratch.assign (4096, 0.0f);
}

SamplerEngine::~SamplerEngine()
{
    for (auto& pad : pads)
    {
        delete pad.buffer.exchange (nullptr, std::memory_order_acq_rel);
        pad.playing.store (false, std::memory_order_relaxed);
    }

    for (auto& item : retired)
        delete item.buffer;

    retired.clear();
}

void SamplerEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    maxBlockSize = juce::jmax (32, maximumBlockSize);
    monoScratch.assign ((size_t) juce::jmax (maxBlockSize, 4096), 0.0f);
    reset();
}

void SamplerEngine::reset() noexcept
{
    for (auto& pad : pads)
    {
        pad.playing.store (false, std::memory_order_relaxed);
        pad.triggerPending.store (false, std::memory_order_relaxed);
        pad.readPosition = 0.0;
    }

    publishedPeakDb.store (-100.0f, std::memory_order_relaxed);
}

bool SamplerEngine::loadPad (int padIndex, const juce::File& file, juce::String& errorMessage)
{
    if (! isValidPad (padIndex))
    {
        errorMessage = "Nomor pad tidak valid.";
        return false;
    }

    if (! file.existsAsFile())
    {
        errorMessage = "File tidak ditemukan: " + file.getFullPathName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));

    if (reader == nullptr)
    {
        errorMessage = "Format file tidak didukung atau file rusak: " + file.getFileName();
        return false;
    }

    auto frames = reader->lengthInSamples;

    if (frames <= 0)
    {
        errorMessage = "File audio kosong: " + file.getFileName();
        return false;
    }

    if (reader->sampleRate > 0.0)
        frames = juce::jmin (frames, (int64_t) std::llround (reader->sampleRate * 300.0));

    frames = juce::jmin (frames, maxSampleFrames);

    const auto bytesNeeded = (size_t) frames * 2u * sizeof (float);

    if (bytesNeeded > maxBufferBytes)
    {
        errorMessage = "Sampel terlalu panjang (maksimal sekitar 5 menit / 300 MB).";
        return false;
    }

    const auto channelsToRead = juce::jlimit (1, 2, (int) reader->numChannels);
    juce::AudioBuffer<float> tempBuffer (channelsToRead, (int) frames);

    if (! reader->read (&tempBuffer, 0, (int) frames, 0, true, channelsToRead > 1))
    {
        errorMessage = "Gagal membaca audio dari file: " + file.getFileName();
        return false;
    }

    auto buffer = std::make_unique<SampleBuffer>();
    buffer->sourceSampleRate = reader->sampleRate > 0.0 ? reader->sampleRate : sampleRate;
    buffer->left.resize ((size_t) frames);
    buffer->right.resize ((size_t) frames);

    const auto* left = tempBuffer.getReadPointer (0);
    const auto* right = channelsToRead > 1 ? tempBuffer.getReadPointer (1) : left;

    for (int64_t i = 0; i < frames; ++i)
    {
        float l = left[i];
        float r = right[i];

        buffer->left[(size_t) i] = std::isfinite (l) ? juce::jlimit (-8.0f, 8.0f, l) : 0.0f;
        buffer->right[(size_t) i] = std::isfinite (r) ? juce::jlimit (-8.0f, 8.0f, r) : 0.0f;
    }

    // readPosition belongs to the audio thread; request a reset through the
    // atomic flag instead of writing it from this thread.
    pads[(size_t) padIndex].playing.store (false, std::memory_order_relaxed);
    pads[(size_t) padIndex].triggerPending.store (true, std::memory_order_relaxed);

    auto* old = pads[(size_t) padIndex].buffer.exchange (buffer.release(), std::memory_order_acq_rel);
    retireBuffer (old);
    cleanupRetired();

    {
        const juce::ScopedLock lock (metadataLock);
        padNames.set (padIndex, file.getFileNameWithoutExtension());
        padPaths.set (padIndex, file.getFullPathName());
    }

    // Each load/clear gives the message thread a chance to reclaim old buffers.
    return true;
}

void SamplerEngine::clearPad (int padIndex)
{
    if (! isValidPad (padIndex))
        return;

    auto& pad = pads[(size_t) padIndex];
    pad.playing.store (false, std::memory_order_relaxed);
    pad.triggerPending.store (true, std::memory_order_relaxed);

    auto* old = pad.buffer.exchange (nullptr, std::memory_order_acq_rel);
    retireBuffer (old);
    cleanupRetired();

    const juce::ScopedLock lock (metadataLock);
    padNames.set (padIndex, "Pad " + juce::String (padIndex + 1));
    padPaths.set (padIndex, {});
}

void SamplerEngine::triggerPad (int padIndex)
{
    if (! isValidPad (padIndex))
        return;

    auto& pad = pads[(size_t) padIndex];

    if (pad.buffer.load (std::memory_order_acquire) == nullptr)
        return;

    pad.triggerPending.store (true, std::memory_order_release);
    pad.playing.store (true, std::memory_order_release);
}

void SamplerEngine::stopPad (int padIndex)
{
    if (! isValidPad (padIndex))
        return;

    pads[(size_t) padIndex].triggerPending.store (false, std::memory_order_relaxed);
    pads[(size_t) padIndex].playing.store (false, std::memory_order_release);
}

void SamplerEngine::stopAll() noexcept
{
    for (auto& pad : pads)
    {
        pad.triggerPending.store (false, std::memory_order_relaxed);
        pad.playing.store (false, std::memory_order_release);
    }
}

void SamplerEngine::setPadGainDb (int padIndex, float gainDb)
{
    if (isValidPad (padIndex))
        pads[(size_t) padIndex].gainDb.store (juce::jlimit (-48.0f, 12.0f, gainDb),
                                             std::memory_order_relaxed);
}

void SamplerEngine::setPadLoop (int padIndex, bool shouldLoop)
{
    if (isValidPad (padIndex))
        pads[(size_t) padIndex].loop.store (shouldLoop, std::memory_order_relaxed);
}

void SamplerEngine::setPadName (int padIndex, const juce::String& name)
{
    if (! isValidPad (padIndex))
        return;

    const juce::ScopedLock lock (metadataLock);
    padNames.set (padIndex, name.trim().isEmpty() ? "Pad " + juce::String (padIndex + 1) : name.trim());
}

SamplerEngine::PadInfo SamplerEngine::getPadInfo (int padIndex) const
{
    PadInfo info;

    if (! isValidPad (padIndex))
        return info;

    const auto& pad = pads[(size_t) padIndex];
    info.gainDb = pad.gainDb.load (std::memory_order_relaxed);
    info.loop = pad.loop.load (std::memory_order_relaxed);
    info.playing = pad.playing.load (std::memory_order_relaxed);
    info.loaded = pad.buffer.load (std::memory_order_acquire) != nullptr;

    const juce::ScopedLock lock (metadataLock);
    info.name = padNames[padIndex];
    info.filePath = padPaths[padIndex];

    return info;
}

int SamplerEngine::getActiveVoiceCount() const noexcept
{
    int count = 0;

    for (const auto& pad : pads)
        if (pad.playing.load (std::memory_order_relaxed))
            ++count;

    return count;
}

void SamplerEngine::retireBuffer (SampleBuffer* buffer)
{
    if (buffer == nullptr)
        return;

    retired.push_back ({ buffer, juce::Time::getMillisecondCounter() });
}

void SamplerEngine::cleanupRetired()
{
    const auto now = juce::Time::getMillisecondCounter();

    for (auto it = retired.begin(); it != retired.end();)
    {
        if (now - it->retiredAtMs >= retentionMs)
        {
            delete it->buffer;
            it = retired.erase (it);
        }
        else
        {
            ++it;
        }
    }
}

void SamplerEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || numSamples <= 0)
        return;

    juce::ScopedNoDenormals noDenormals;

    const bool monoOutput = right == nullptr;
    float* workRight = monoOutput ? monoScratch.data() : right;

    if (monoOutput && (size_t) numSamples > monoScratch.size())
        return;

    if (monoOutput)
        juce::FloatVectorOperations::clear (workRight, numSamples);

    float blockPeak = 0.0f;

    for (auto& pad : pads)
    {
        auto* buffer = pad.buffer.load (std::memory_order_acquire);

        if (buffer == nullptr || buffer->left.empty())
            continue;

        if (pad.triggerPending.exchange (false, std::memory_order_acq_rel))
            pad.readPosition = 0.0;

        if (! pad.playing.load (std::memory_order_relaxed))
            continue;

        const auto length = (int64_t) buffer->left.size();
        const auto step = buffer->sourceSampleRate / sampleRate;
        const auto gain = juce::Decibels::decibelsToGain (pad.gainDb.load (std::memory_order_relaxed), -100.0f);
        const bool looping = pad.loop.load (std::memory_order_relaxed);
        bool finished = false;

        for (int i = 0; i < numSamples; ++i)
        {
            if (pad.readPosition >= (double) (length - 1))
            {
                if (looping)
                {
                    pad.readPosition = std::fmod (pad.readPosition, (double) length);
                }
                else
                {
                    finished = true;
                    break;
                }
            }

            const auto index = (size_t) pad.readPosition;
            const auto nextIndex = looping ? (index + 1) % (size_t) length
                                           : juce::jmin (index + 1, (size_t) length - 1);
            const auto fraction = (float) (pad.readPosition - (double) index);

            const auto sampleL = buffer->left[index] * (1.0f - fraction) + buffer->left[nextIndex] * fraction;
            const auto sampleR = buffer->right[index] * (1.0f - fraction) + buffer->right[nextIndex] * fraction;

            left[i] += sampleL * gain;
            workRight[i] += sampleR * gain;
            blockPeak = juce::jmax (blockPeak, std::abs (sampleL * gain), std::abs (sampleR * gain));

            pad.readPosition += step;
        }

        if (finished)
        {
            pad.playing.store (false, std::memory_order_relaxed);
            pad.readPosition = 0.0;
        }
    }

    if (monoOutput)
    {
        for (int i = 0; i < numSamples; ++i)
            left[i] += workRight[i];
    }

    publishedPeakDb.store (juce::Decibels::gainToDecibels (blockPeak, -100.0f),
                           std::memory_order_relaxed);
}

juce::var SamplerEngine::saveState() const
{
    juce::Array<juce::var> padArray;

    {
        const juce::ScopedLock lock (metadataLock);

        for (int i = 0; i < numPads; ++i)
        {
            auto entry = juce::DynamicObject::Ptr (new juce::DynamicObject());
            entry->setProperty ("index", i);
            entry->setProperty ("name", padNames[i]);
            entry->setProperty ("path", padPaths[i]);
            entry->setProperty ("gainDb", pads[(size_t) i].gainDb.load (std::memory_order_relaxed));
            entry->setProperty ("loop", pads[(size_t) i].loop.load (std::memory_order_relaxed));
            entry->setProperty ("loaded", pads[(size_t) i].buffer.load (std::memory_order_acquire) != nullptr);
            padArray.add (juce::var (entry.get()));
        }
    }

    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("version", 1);
    root->setProperty ("pads", padArray);
    return juce::var (root.get());
}

bool SamplerEngine::restoreState (const juce::var& state, juce::String& errorMessage)
{
    auto* root = state.getDynamicObject();
    const auto* padArray = root != nullptr ? root->getProperty ("pads").getArray() : nullptr;

    if (padArray == nullptr)
    {
        errorMessage = "State sampler tidak valid.";
        return false;
    }

    juce::StringArray failures;

    for (const auto& item : *padArray)
    {
        auto* entry = item.getDynamicObject();

        if (entry == nullptr)
            continue;

        const auto index = (int) entry->getProperty ("index");

        if (! isValidPad (index))
            continue;

        setPadGainDb (index, (float) entry->getProperty ("gainDb"));
        setPadLoop (index, (bool) entry->getProperty ("loop"));

        const auto name = entry->getProperty ("name").toString();
        const auto path = entry->getProperty ("path").toString();

        if (name.isNotEmpty())
            setPadName (index, name);

        if (path.isNotEmpty())
        {
            juce::String loadError;
            const juce::File file (path);

            if (! loadPad (index, file, loadError))
                failures.add (loadError);
            else if (name.isNotEmpty())
                setPadName (index, name);
        }
        else
        {
            clearPad (index);

            if (name.isNotEmpty())
                setPadName (index, name);
        }
    }

    if (! failures.isEmpty())
    {
        errorMessage = failures.joinIntoString ("\n");
        return false;
    }

    return true;
}

} // namespace rr
