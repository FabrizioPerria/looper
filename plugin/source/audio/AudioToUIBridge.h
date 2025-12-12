#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>
#include <atomic>
#include <thread>

class AudioToUIBridge
{
public:
    struct AudioState
    {
        std::atomic<int> loopLength { 0 };
        std::atomic<int> readPosition { 0 };
        std::atomic<bool> isRecording { false };
        std::atomic<bool> isPlaying { false };
        std::atomic<double> sampleRate { 44100.0 };
        std::atomic<int> stateVersion { 0 };
    };

    struct WaveformSnapshot
    {
        juce::AudioBuffer<float> buffer;
        int length = 0;
        int version = 0;

        void copyFrom (const juce::AudioBuffer<float>& source, int sourceLength, int ver)
        {
            PERFETTO_FUNCTION();
            if (buffer.getNumChannels() != source.getNumChannels() || buffer.getNumSamples() < sourceLength)
            {
                buffer.setSize (source.getNumChannels(), sourceLength, false, true, true);
            }

            for (int ch = 0; ch < source.getNumChannels(); ++ch)
            {
                buffer.copyFrom (ch, 0, source, ch, 0, sourceLength);
            }
            length = sourceLength;
            version = ver;
        }
    };

    AudioToUIBridge()
    {
        PERFETTO_FUNCTION();
        for (int i = 0; i < 3; ++i)
        {
            snapshots[i] = std::make_unique<WaveformSnapshot>();
        }

        startCopyThread();
    }

    ~AudioToUIBridge() { stopCopyThread(); }

    void signalWaveformChanged()
    {
        PERFETTO_FUNCTION();
        pendingUpdate.store (true, std::memory_order_release);
    }

    void clear()
    {
        PERFETTO_FUNCTION();
        for (int i = 0; i < 3; ++i)
        {
            snapshots[i]->buffer.setSize (0, 0);
            snapshots[i]->length = 0;
            snapshots[i]->version = -1;
        }

        state.loopLength.store (0, std::memory_order_relaxed);
        state.readPosition.store (0, std::memory_order_relaxed);
        state.isRecording.store (false, std::memory_order_relaxed);
        state.isPlaying.store (false, std::memory_order_relaxed);
        state.stateVersion.fetch_add (1, std::memory_order_release);
        state.sampleRate.store (44100.0, std::memory_order_relaxed);

        pendingUpdate.store (false, std::memory_order_relaxed);
        lastUIVersion = -1;
    }

    bool shouldUpdateWhileRecording (int samplesPerBlock, double sampleRate)
    {
        PERFETTO_FUNCTION();
        recordingFrameCounter++;
        int framesPerUpdate = (int) (sampleRate * 0.1 / samplesPerBlock); // 100ms

        if (recordingFrameCounter >= framesPerUpdate)
        {
            recordingFrameCounter = 0;
            return true;
        }
        return false;
    }

    void resetRecordingCounter()
    {
        PERFETTO_FUNCTION();
        recordingFrameCounter = 0;
    }

    // Called from AUDIO THREAD - just store the pointer, don't copy
    void updateFromAudioThread (const juce::AudioBuffer<float>* audioBuffer,
                                int length,
                                int readPos,
                                bool recording,
                                bool playing,
                                double sampleRate)
    {
        PERFETTO_FUNCTION();
        int prevPos = state.readPosition.load (std::memory_order_relaxed);
        if (readPos != prevPos)
        {
            playbackPositionChanged.store (true, std::memory_order_release);
        }

        state.readPosition.store (readPos, std::memory_order_relaxed);
        state.loopLength.store (length, std::memory_order_relaxed);
        state.isRecording.store (recording, std::memory_order_relaxed);
        state.isPlaying.store (playing, std::memory_order_relaxed);
        state.sampleRate.store (sampleRate, std::memory_order_relaxed);

        // Just signal that there's work to do - don't copy here
        if (pendingUpdate.exchange (false, std::memory_order_acq_rel))
        {
            // Store pointer and length for background thread to copy
            pendingBufferPtr.store (audioBuffer, std::memory_order_release);
            pendingBufferLength.store (length, std::memory_order_release);
            copySignal.signal();
        }
    }

    void getPlaybackState (int& length, int& readPos, bool& recording, bool& playing, double& sampleRate)
    {
        PERFETTO_FUNCTION();
        length = state.loopLength.load (std::memory_order_relaxed);
        readPos = state.readPosition.load (std::memory_order_relaxed);
        recording = state.isRecording.load (std::memory_order_relaxed);
        playing = state.isPlaying.load (std::memory_order_relaxed);
        sampleRate = state.sampleRate.load (std::memory_order_relaxed);
    }

    void getIsPendingUpdate (bool& pending)
    {
        PERFETTO_FUNCTION();
        pending = pendingUpdate.load (std::memory_order_relaxed);
    }

    bool getWaveformSnapshot (WaveformSnapshot& destination)
    {
        PERFETTO_FUNCTION();
        int currentVersion = state.stateVersion.load (std::memory_order_acquire);

        if (currentVersion == lastUIVersion) return false;

        int writeIdx = writeIndex.load (std::memory_order_acquire);
        int prevUIIdx = uiIndex.exchange (writeIdx, std::memory_order_acq_rel);
        readIndex.store (prevUIIdx, std::memory_order_release);

        auto& snapshot = snapshots[writeIdx];

        if (snapshot->version == currentVersion)
        {
            destination.copyFrom (snapshot->buffer, snapshot->length, snapshot->version);
            lastUIVersion = currentVersion;
            return true;
        }

        return false;
    }

    const AudioState& getState() const
    {
        PERFETTO_FUNCTION();
        return state;
    }

    std::atomic<bool> playbackPositionChanged { false };

private:
    AudioState state;
    std::atomic<bool> pendingUpdate { false };
    int recordingFrameCounter = 0;

    std::atomic<int> writeIndex { 0 };
    std::atomic<int> readIndex { 1 };
    std::atomic<int> uiIndex { 2 };

    std::unique_ptr<WaveformSnapshot> snapshots[3];
    int lastUIVersion = -1;

    // Background copy thread
    std::atomic<const juce::AudioBuffer<float>*> pendingBufferPtr { nullptr };
    std::atomic<int> pendingBufferLength { 0 };
    juce::WaitableEvent copySignal;
    std::atomic<bool> shouldStop { false };
    std::thread copyThread;

    void startCopyThread()
    {
        copyThread = std::thread (
            [this]()
            {
                juce::Thread::setCurrentThreadName ("Waveform Copy Thread");

                while (! shouldStop.load())
                {
                    copySignal.wait (100);

                    auto* bufferPtr = pendingBufferPtr.exchange (nullptr, std::memory_order_acquire);
                    if (bufferPtr)
                    {
                        int length = pendingBufferLength.load (std::memory_order_acquire);
                        int newVersion = state.stateVersion.load (std::memory_order_relaxed) + 1;

                        int writeIdx = findFreeWriteBuffer();
                        snapshots[writeIdx]->copyFrom (*bufferPtr, length, newVersion);

                        writeIndex.store (writeIdx, std::memory_order_release);
                        state.stateVersion.store (newVersion, std::memory_order_release);
                    }
                }
            });
    }

    void stopCopyThread()
    {
        shouldStop.store (true);
        copySignal.signal();
        if (copyThread.joinable())
        {
            copyThread.join();
        }
    }

    int findFreeWriteBuffer()
    {
        int readIdx = readIndex.load (std::memory_order_acquire);
        int uiIdx = uiIndex.load (std::memory_order_acquire);

        for (int i = 0; i < 3; ++i)
        {
            if (i != readIdx && i != uiIdx) return i;
        }
        return 0;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioToUIBridge)
};
