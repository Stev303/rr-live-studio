#pragma once

#include "Audio/BusRouter.h"
#include "Audio/LoopbackCapture.h"
#include "Audio/ParamStore.h"
#include "Audio/SmartBalance.h"
#include "DSP/Limiter.h"
#include "DSP/MusicBooster.h"
#include "DSP/PitchShifter.h"
#include "DSP/VocalChain.h"
#include "Sampler/SamplerEngine.h"
#include "Utilities/DiagnosticsLog.h"
#include "VST3Host/PluginHost.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <vector>

namespace rr
{
/** Owns the audio device, the processing graph and the WASAPI loopback capture.

    Graph:
      mic -> vocal chain -> vocal VST3 chain -> vocal bus
      loopback -> YouTube pitch shift -> music booster -> smart balance -> music bus
      sampler pads -> sampler bus
      vocal + music + sampler -> test tone -> master VST3 chain
      -> master gain -> master limiter -> hardware output

    The audio callback performs no allocation, no locking, no file or network I/O.
    Paths with different latency (phase vocoder, look-ahead limiters, plug-ins) are
    aligned with small delay lines so the streamed mix stays coherent.
*/
class AudioEngine : public juce::ChangeBroadcaster,
                    private juce::AudioIODeviceCallback,
                    private juce::AsyncUpdater,
                    private juce::ChangeListener
{
public:
    struct Meters
    {
        float micPeakDb = -100.0f;
        float vocalChainPeakDb = -100.0f;
        float vocalBusPeakDb = -100.0f;
        float musicBoosterPeakDb = -100.0f;
        float musicBusPeakDb = -100.0f;
        float samplerPeakDb = -100.0f;
        float masterPeakDb = -100.0f;
        float masterRmsDb = -100.0f;
        float loopbackPeakDb = -100.0f;

        float vocalCompressorGrDb = 0.0f;
        float vocalDeEsserGrDb = 0.0f;
        float musicCompressorGrDb = 0.0f;
        float musicLimiterGrDb = 0.0f;
        float smartBalanceGrDb = 0.0f;
        float masterLimiterGrDb = 0.0f;

        float pitchSemitones = 0.0f;
        float detectedMidiNote = 0.0f;
        float pitchConfidence = 0.0f;
        float autoBoostDb = 0.0f;

        bool pitchVoiced = false;
        bool gateOpen = false;
        bool clip = false;
        bool loopbackRunning = false;
        bool loopbackSignal = false;
        int autoBoostState = 0;
        int dropouts = 0;
        int loopbackOverflows = 0;

        double captureSampleRate = 0.0;
        double latencyMs = 0.0;
        double cpuPercent = 0.0;
    };

    AudioEngine (ParamStore& parameters,
                 SamplerEngine& samplerEngine,
                 PluginHost& pluginHost,
                 DiagnosticsLog& diagnostics);
    ~AudioEngine() override;

    bool initialiseDevices (const juce::XmlElement* savedDeviceState, juce::String& errorMessage);
    void shutdown();

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    juce::String getDeviceDescription() const;
    juce::String getDeviceError() const;
    bool isRunning() const noexcept { return deviceRunning.load (std::memory_order_relaxed); }
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load (std::memory_order_relaxed); }
    int getCurrentBlockSize() const noexcept { return currentBlockSize.load (std::memory_order_relaxed); }

    double getDeviceLatencyMs() const;
    double getEngineLatencyMs() const noexcept { return engineLatencyMs.load (std::memory_order_relaxed); }
    double getTotalLatencyMs() const { return getDeviceLatencyMs() + getEngineLatencyMs(); }

    juce::Array<LoopbackCapture::EndpointInfo> getLoopbackEndpoints() const
    {
        return LoopbackCapture::enumerateEndpoints();
    }

    void setLoopbackEndpointId (const juce::String& endpointId);
    juce::String getLoopbackEndpointId() const;
    juce::String getLoopbackEndpointName() const;
    bool startLoopback (juce::String& errorMessage);
    void stopLoopback();
    bool isLoopbackActive() const { return loopbackWanted.load (std::memory_order_relaxed); }
    bool isLoopbackCapturing() const { return loopback.isRunning(); }
    juce::String getLoopbackError() const;
    juce::String getLoopbackWarning() const;

    Meters getMeters() const;
    void resetClipIndicators();
    void requestAutoBoost();
    void resetAutoBoost();

    VocalChain& getVocalChain() noexcept { return vocalChain; }
    MusicBooster& getMusicBooster() noexcept { return musicBooster; }
    LoopbackCapture& getLoopbackCapture() noexcept { return loopback; }

private:
    class AlignmentDelay
    {
    public:
        AlignmentDelay() = default;

        void prepare (double sampleRate, double maximumDelaySeconds = 0.2);
        void reset() noexcept;
        void setTargetSamples (int newTarget) noexcept;
        void process (float* left, float* right, int numSamples) noexcept;

    private:
        std::vector<float> bufferL;
        std::vector<float> bufferR;
        int size = 0;
        int writeIndex = 0;
        double currentDelay = 0.0;
        int targetDelay = 0;

        JUCE_DECLARE_NON_COPYABLE (AlignmentDelay)
    };

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& errorMessage) override;

    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    void prepare (double newSampleRate, int blockSize);
    void processChunk (const float* const* inputChannelData, int numInputChannels,
                       float* const* outputChannelData, int numOutputChannels,
                       int sampleOffset, int numSamples) noexcept;
    void updateDelayAlignment() noexcept;
    void updateMetersAfterBlock (float micPeak, float vocalBusPeak, float musicBusPeak,
                                 float samplerBusPeak, float masterPeak, double masterSumSquares,
                                 int numSamples, bool clipped) noexcept;
    bool restartLoopbackForDevice();
    bool checkLoopbackConflict (juce::String& explanation) const;
    static juce::String normaliseEndpointName (juce::String name);

    ParamStore& params;
    SamplerEngine& sampler;
    PluginHost& pluginHost;
    DiagnosticsLog& diagnostics;

    juce::AudioDeviceManager deviceManager;
    LoopbackCapture loopback;
    VocalChain vocalChain;
    PhaseVocoderPitchShifter musicPitch;
    MusicBooster musicBooster;
    SmartBalance smartBalance;
    dsp::Limiter masterLimiter;
    AlignmentDelay vocalAlignment;
    AlignmentDelay musicAlignment;

    std::atomic<bool> deviceRunning { false };
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> currentBlockSize { 512 };
    std::atomic<int> preparedBlockSize { 0 };
    std::atomic<double> engineLatencyMs { 0.0 };
    std::atomic<bool> clipLatched { false };

    juce::SmoothedValue<float> masterGainSmoother { 1.0f };
    float testTonePhase = 0.0f;
    int consecutiveLoopbackUnderruns = 0;

    std::vector<float> micL, micR;
    std::vector<float> vocalL, vocalR;
    std::vector<float> musicL, musicR;
    std::vector<float> musicShiftL, musicShiftR;
    std::vector<float> samplerL, samplerR;
    std::vector<float> masterL, masterR;

    std::atomic<float> meterMic { -100.0f };
    std::atomic<float> meterVocalChain { -100.0f };
    std::atomic<float> meterVocalBus { -100.0f };
    std::atomic<float> meterMusicBooster { -100.0f };
    std::atomic<float> meterMusicBus { -100.0f };
    std::atomic<float> meterSampler { -100.0f };
    std::atomic<float> meterMaster { -100.0f };
    std::atomic<float> meterMasterRms { -100.0f };

    std::atomic<bool> loopbackWanted { false };
    std::atomic<bool> loopbackRestartPending { false };
    mutable juce::CriticalSection loopbackStateLock;
    juce::String loopbackEndpointId;
    juce::String lastLoopbackError;

    mutable juce::CriticalSection deviceErrorLock;
    juce::String lastDeviceError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace rr
