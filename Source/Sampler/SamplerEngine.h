#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <vector>

namespace rr
{
/** Ten-pad sampler.

    Decoding and file access happen on the message thread.  The audio thread
    only reads immutable sample buffers that were published atomically.  A
    replaced buffer is kept alive for a grace period before it is freed, so the
    audio callback can never touch freed memory.
*/
class SamplerEngine
{
public:
    static constexpr int numPads = 10;

    struct PadInfo
    {
        juce::String name;
        juce::String filePath;
        float gainDb = 0.0f;
        bool loop = false;
        bool playing = false;
        bool loaded = false;
    };

    SamplerEngine();
    ~SamplerEngine();

    void prepare (double newSampleRate, int maximumBlockSize);
    void reset() noexcept;

    bool loadPad (int padIndex, const juce::File& file, juce::String& errorMessage);
    void clearPad (int padIndex);

    void triggerPad (int padIndex);
    void stopPad (int padIndex);
    void stopAll() noexcept;

    void setPadGainDb (int padIndex, float gainDb);
    void setPadLoop (int padIndex, bool shouldLoop);
    void setPadName (int padIndex, const juce::String& name);

    PadInfo getPadInfo (int padIndex) const;
    int getActiveVoiceCount() const noexcept;
    float getPeakDb() const noexcept { return publishedPeakDb.load (std::memory_order_relaxed); }

    void process (float* left, float* right, int numSamples) noexcept;

    juce::var saveState() const;
    bool restoreState (const juce::var& state, juce::String& errorMessage);

private:
    struct SampleBuffer
    {
        std::vector<float> left;
        std::vector<float> right;
        double sourceSampleRate = 48000.0;
    };

    struct Pad
    {
        std::atomic<SampleBuffer*> buffer { nullptr };
        std::atomic<float> gainDb { 0.0f };
        std::atomic<bool> loop { false };
        std::atomic<bool> playing { false };
        std::atomic<bool> triggerPending { false };

        // Audio-thread-only playhead.
        double readPosition = 0.0;
    };

    struct RetiredBuffer
    {
        SampleBuffer* buffer = nullptr;
        juce::uint32 retiredAtMs = 0;
    };

    static bool isValidPad (int index) noexcept { return index >= 0 && index < numPads; }
    void retireBuffer (SampleBuffer* buffer);
    void cleanupRetired();

    std::array<Pad, numPads> pads;
    juce::StringArray padNames;
    juce::StringArray padPaths;
    mutable juce::CriticalSection metadataLock;

    juce::AudioFormatManager formats;
    double sampleRate = 48000.0;
    int maxBlockSize = 512;
    std::vector<float> monoScratch;
    std::vector<RetiredBuffer> retired;
    std::atomic<float> publishedPeakDb { -100.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SamplerEngine)
};

} // namespace rr
