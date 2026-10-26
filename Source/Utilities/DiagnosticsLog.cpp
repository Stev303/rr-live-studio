#include "Utilities/DiagnosticsLog.h"

namespace rr
{
void DiagnosticsLog::initialise (const juce::File& folder)
{
    logFile = folder.getChildFile ("diagnostics.log");
    folder.createDirectory();

    const juce::ScopedLock guard (lock);
    lines.add ("==== Rr Live Studio diagnostics, "
               + juce::Time::getCurrentTime().toString (true, true, true) + " ====");
}

void DiagnosticsLog::log (const juce::String& message)
{
    const juce::ScopedLock guard (lock);
    lines.add (juce::Time::getCurrentTime().toString (false, true, true) + "  " + message);

    while (lines.size() > 1000)
        lines.remove (0);
}

void DiagnosticsLog::recordAudioBlock (int numSamples, double elapsedMs, bool clipped, bool overflowed) noexcept
{
    juce::ignoreUnused (numSamples);

    const auto micros = (juce::uint32) juce::jlimit (0.0, 1000000.0, elapsedMs * 1000.0);

    callbackCount.fetch_add (1, std::memory_order_relaxed);
    lastCallbackMicros.store (micros, std::memory_order_relaxed);

    auto previousMax = maxCallbackMicros.load (std::memory_order_relaxed);

    while (micros > previousMax
           && ! maxCallbackMicros.compare_exchange_weak (previousMax, micros,
                                                         std::memory_order_relaxed))
    {
    }

    totalCallbackMicros.fetch_add ((juce::uint64) micros, std::memory_order_relaxed);

    if (clipped)
        clippedFlag.store (true, std::memory_order_relaxed);

    if (overflowed)
        dropoutCount.fetch_add (1, std::memory_order_relaxed);
}

void DiagnosticsLog::resetAudioStats() noexcept
{
    callbackCount.store (0, std::memory_order_relaxed);
    dropoutCount.store (0, std::memory_order_relaxed);
    lastCallbackMicros.store (0, std::memory_order_relaxed);
    maxCallbackMicros.store (0, std::memory_order_relaxed);
    totalCallbackMicros.store (0, std::memory_order_relaxed);
    clippedFlag.store (false, std::memory_order_relaxed);
}

DiagnosticsLog::AudioStats DiagnosticsLog::getAudioStats() const noexcept
{
    AudioStats stats;
    stats.callbackCount = callbackCount.load (std::memory_order_relaxed);
    stats.dropoutCount = dropoutCount.load (std::memory_order_relaxed);
    stats.lastCallbackMs = (double) lastCallbackMicros.load (std::memory_order_relaxed) / 1000.0;
    stats.maxCallbackMs = (double) maxCallbackMicros.load (std::memory_order_relaxed) / 1000.0;
    stats.clipped = clippedFlag.load (std::memory_order_relaxed);

    const auto total = totalCallbackMicros.load (std::memory_order_relaxed);

    if (stats.callbackCount > 0)
        stats.averageCallbackMs = ((double) total / (double) stats.callbackCount) / 1000.0;

    return stats;
}

juce::StringArray DiagnosticsLog::getRecentLines (int maximumLines) const
{
    const juce::ScopedLock guard (lock);
    const auto first = juce::jmax (0, lines.size() - juce::jmax (0, maximumLines));
    juce::StringArray result;

    for (int i = first; i < lines.size(); ++i)
        result.add (lines[i]);

    return result;
}

juce::String DiagnosticsLog::getRecentText (int maximumLines) const
{
    return getRecentLines (maximumLines).joinIntoString ("\n");
}

juce::File DiagnosticsLog::getLogFile() const
{
    const juce::ScopedLock guard (lock);
    return logFile;
}

bool DiagnosticsLog::exportTo (const juce::File& destination) const
{
    const auto text = getRecentText (1000);

    if (text.isEmpty())
        return false;

    return destination.replaceWithText (text);
}

void DiagnosticsLog::clear()
{
    const juce::ScopedLock guard (lock);
    lines.clear();
}

} // namespace rr
