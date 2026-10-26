#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <vector>

namespace rr
{
/** WASAPI loopback capture for the dedicated YouTube Music Bus.

    Captures whatever is rendered to the selected Windows playback endpoint.
    This is an endpoint capture: it cannot isolate a single application and it
    also captures any other sound playing on that endpoint.  The engine refuses
    to run it when the endpoint equals the master output device, because that
    would create an internal feedback loop.

    The capture thread converts the endpoint mix format to stereo float and
    resamples it to the audio engine sample rate.  Samples are handed to the
    audio thread through a lock-free AbstractFifo.
*/
class LoopbackCapture : private juce::Thread
{
public:
    struct EndpointInfo
    {
        juce::String id;
        juce::String name;
        bool isDefault = false;
    };

    LoopbackCapture();
    ~LoopbackCapture() override;

    static juce::Array<EndpointInfo> enumerateEndpoints();
    static juce::String getDefaultEndpointId();

    bool start (const juce::String& endpointId, double engineSampleRate);
    void stop();
    void restart();

    bool isRunning() const noexcept { return isThreadRunning(); }
    bool isSessionActive() const noexcept { return sessionActive.load (std::memory_order_relaxed); }
    bool hasSignal() const noexcept { return peakDb.load (std::memory_order_relaxed) > -90.0f; }

    int read (float* left, float* right, int maximumSamples) noexcept;

    float getPeakDb() const noexcept { return peakDb.load (std::memory_order_relaxed); }
    double getCaptureSampleRate() const noexcept { return captureSampleRate.load (std::memory_order_relaxed); }
    int getOverflowCount() const noexcept { return overflowCount.load (std::memory_order_relaxed); }
    int getAvailableSamples() const noexcept { return fifo.getNumReady(); }
    double getTargetSampleRate() const noexcept { return targetSampleRate.load (std::memory_order_relaxed); }

    juce::String getLastError() const;
    juce::String getEndpointName() const;
    juce::String getEndpointId() const;

private:
    void run() override;
    void setError (const juce::String& message);
    void clearError();
    bool processPacket (const void* data, int numFrames, bool isSilent,
                        int numChannels, int bytesPerSample, bool isFloat,
                        double sourceSampleRate);
    void appendResampled (const float* left, const float* right, int numSamples);
    void updatePeak (const float* left, const float* right, int numSamples) noexcept;

    juce::AbstractFifo fifo { 65536 };
    std::vector<float> fifoLeft;
    std::vector<float> fifoRight;

    std::atomic<bool> sessionActive { false };
    std::atomic<float> peakDb { -100.0f };
    std::atomic<double> captureSampleRate { 0.0 };
    std::atomic<int> overflowCount { 0 };
    std::atomic<void*> wakeEvent { nullptr };
    std::atomic<double> targetSampleRate { 0.0 };

    mutable juce::CriticalSection stateLock;
    juce::String lastError;
    juce::String currentEndpointId;
    juce::String currentEndpointName;
    double requestedSampleRate = 48000.0;
    int targetBacklogSamples = 1764;
    int maxBacklogSamples = 8820;
    std::atomic<bool> primed { false };

    // Capture-thread-only state
    std::vector<float> sourceLeft;
    std::vector<float> sourceRight;
    std::vector<float> resampledLeft;
    std::vector<float> resampledRight;
    double resamplePosition = 0.0;
    double driftCorrection = 1.0;
    float lastSourceL = 0.0f;
    float lastSourceR = 0.0f;
    bool haveLastSource = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopbackCapture)
};

} // namespace rr
