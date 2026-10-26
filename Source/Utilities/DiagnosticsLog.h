#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

namespace rr
{
/** In-memory diagnostic log plus lock-free audio statistics.

    log() is for the message thread only.  recordAudioBlock() is safe to call
    from the audio callback: it performs atomic arithmetic and never allocates,
    locks or touches the disk.
*/
class DiagnosticsLog
{
public:
    struct AudioStats
    {
        juce::uint32 callbackCount = 0;
        juce::uint32 dropoutCount = 0;
        double lastCallbackMs = 0.0;
        double maxCallbackMs = 0.0;
        double averageCallbackMs = 0.0;
        bool clipped = false;
    };

    void initialise (const juce::File& folder);
    void log (const juce::String& message);
    void recordAudioBlock (int numSamples, double elapsedMs, bool clipped, bool overflowed) noexcept;
    void resetAudioStats() noexcept;

    AudioStats getAudioStats() const noexcept;
    juce::StringArray getRecentLines (int maximumLines = 200) const;
    juce::String getRecentText (int maximumLines = 200) const;
    juce::File getLogFile() const;
    bool exportTo (const juce::File& destination) const;
    void clear();

private:
    mutable juce::CriticalSection lock;
    juce::StringArray lines;
    juce::File logFile;

    std::atomic<juce::uint32> callbackCount { 0 };
    std::atomic<juce::uint32> dropoutCount { 0 };
    std::atomic<juce::uint32> lastCallbackMicros { 0 };
    std::atomic<juce::uint32> maxCallbackMicros { 0 };
    std::atomic<juce::uint64> totalCallbackMicros { 0 };
    std::atomic<bool> clippedFlag { false };
};

} // namespace rr
