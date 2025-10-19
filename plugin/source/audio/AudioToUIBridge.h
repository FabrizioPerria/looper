#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>
#include <atomic>

class AudioToUIBridge
{
public:
    struct AudioState
    {
        std::atomic<int> loopLength { 0 };
        std::atomic<int> readPosition { 0 };
        std::atomic<bool> isRecording { false };
        std::atomic<bool> isPlaying { false };
        std::atomic<int> stateVersion { 0 }; // Increment when waveform changes
    };

    // Triple buffer for safe waveform transfer
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
                buffer.setSize (source.getNumChannels(), sourceLength, false, false, true);
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
        // Initialize triple buffer
        for (int i = 0; i < 3; ++i)
        {
            snapshots[i] = std::make_unique<WaveformSnapshot>();
        }
    }

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
            snapshots[i]->buffer.setSize (0, 0); // Release memory
            snapshots[i]->length = 0;
            snapshots[i]->version = -1;
        }

        state.loopLength.store (0, std::memory_order_relaxed);
        state.readPosition.store (0, std::memory_order_relaxed);
        state.isRecording.store (false, std::memory_order_relaxed);
        state.isPlaying.store (false, std::memory_order_relaxed);
        state.stateVersion.fetch_add (1, std::memory_order_release);

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

    // Called from AUDIO THREAD - must be lock-free and fast
    void updateFromAudioThread (const juce::AudioBuffer<float>* audioBuffer, int length, int readPos, bool recording, bool playing)
    {
        PERFETTO_FUNCTION();
        // Update lightweight state (always)
        state.loopLength.store (length, std::memory_order_relaxed);
        state.readPosition.store (readPos, std::memory_order_relaxed);
        state.isRecording.store (recording, std::memory_order_relaxed);
        state.isPlaying.store (playing, std::memory_order_relaxed);

        // Only update waveform snapshot when it actually changes
        if (pendingUpdate.exchange (false, std::memory_order_acq_rel))
        {
            int newVersion = state.stateVersion.load (std::memory_order_relaxed) + 1;

            // Try to get write buffer without blocking
            int writeIdx = writeIndex.load (std::memory_order_relaxed);
            int readIdx = readIndex.load (std::memory_order_acquire);
            int uiIdx = uiIndex.load (std::memory_order_acquire);

            // Find a buffer that's not being read
            for (int i = 0; i < 3; ++i)
            {
                if (i != readIdx && i != uiIdx)
                {
                    writeIdx = i;
                    break;
                }
            }

            // Quick copy to snapshot (audio thread should do this)
            snapshots[writeIdx]->copyFrom (*audioBuffer, (int) length, newVersion);

            // Publish new snapshot
            writeIndex.store (writeIdx, std::memory_order_release);
            state.stateVersion.store (newVersion, std::memory_order_release);
        }
    }

    // Called from UI THREAD - get latest playback position
    void getPlaybackState (int& length, int& readPos, bool& recording, bool& playing)
    {
        PERFETTO_FUNCTION();
        length = state.loopLength.load (std::memory_order_relaxed);
        readPos = state.readPosition.load (std::memory_order_relaxed);
        recording = state.isRecording.load (std::memory_order_relaxed);
        playing = state.isPlaying.load (std::memory_order_relaxed);
    }

    // Called from UI THREAD - get waveform snapshot if updated
    bool getWaveformSnapshot (WaveformSnapshot& destination)
    {
        PERFETTO_FUNCTION();
        int currentVersion = state.stateVersion.load (std::memory_order_acquire);

        // Check if we already have this version
        if (currentVersion == lastUIVersion) return false; // No update needed

        int writeIdx = writeIndex.load (std::memory_order_acquire);

        // Swap to reading this buffer
        int prevUIIdx = uiIndex.exchange (writeIdx, std::memory_order_acq_rel);
        readIndex.store (prevUIIdx, std::memory_order_release);

        // Now safe to read from writeIdx
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

private:
    AudioState state;
    std::atomic<bool> pendingUpdate { false };
    int recordingFrameCounter = 0;

    // Triple buffer indices
    std::atomic<int> writeIndex { 0 }; // Audio thread writes here
    std::atomic<int> readIndex { 1 };  // Retired buffer
    std::atomic<int> uiIndex { 2 };    // UI thread reads here

    std::unique_ptr<WaveformSnapshot> snapshots[3];
    int lastUIVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioToUIBridge)
};
