#pragma once
#include <JuceHeader.h>
#include <atomic>

class AudioToUIBridge
{
public:
    struct AudioState
    {
        std::atomic<size_t> loopLength { 0 };
        std::atomic<size_t> readPosition { 0 };
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
        // Initialize triple buffer
        for (int i = 0; i < 3; ++i)
        {
            snapshots[i] = std::make_unique<WaveformSnapshot>();
        }
    }

    // Called from AUDIO THREAD - must be lock-free and fast
    void updateFromAudioThread (const juce::AudioBuffer<float>* audioBuffer,
                                size_t length,
                                size_t readPos,
                                bool recording,
                                bool playing,
                                bool waveformChanged)
    {
        // Update lightweight state (always)
        state.loopLength.store (length, std::memory_order_relaxed);
        state.readPosition.store (readPos, std::memory_order_relaxed);
        state.isRecording.store (recording, std::memory_order_relaxed);
        state.isPlaying.store (playing, std::memory_order_relaxed);

        // Only update waveform snapshot when it actually changes
        if (waveformChanged && audioBuffer && length > 0)
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
    void getPlaybackState (size_t& length, size_t& readPos, bool& recording, bool& playing)
    {
        length = state.loopLength.load (std::memory_order_relaxed);
        readPos = state.readPosition.load (std::memory_order_relaxed);
        recording = state.isRecording.load (std::memory_order_relaxed);
        playing = state.isPlaying.load (std::memory_order_relaxed);
    }

    // Called from UI THREAD - get waveform snapshot if updated
    bool getWaveformSnapshot (WaveformSnapshot& destination)
    {
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
        return state;
    }

private:
    AudioState state;

    // Triple buffer indices
    std::atomic<int> writeIndex { 0 }; // Audio thread writes here
    std::atomic<int> readIndex { 1 };  // Retired buffer
    std::atomic<int> uiIndex { 2 };    // UI thread reads here

    std::unique_ptr<WaveformSnapshot> snapshots[3];
    int lastUIVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioToUIBridge)
};
