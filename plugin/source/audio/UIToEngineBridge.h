#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <queue>

class UIToEngineBridge
{
public:
    struct UIState
    {
        juce::File audioFile;
        std::atomic<bool> fileUpdated { false };
        std::atomic<int> trackIndex { -1 };
        std::atomic<int> stateVersion { 0 };
        juce::SpinLock fileLock;

        // Add MIDI message queue
        std::queue<juce::MidiBuffer> midiQueue;
        juce::SpinLock midiLock;
    };

    UIToEngineBridge() = default;

    // Called from UI thread
    void updateAudioFile (const juce::File& newFile, int trackIdx)
    {
        const juce::SpinLock::ScopedLockType lock (state.fileLock);
        state.audioFile = newFile;
        state.trackIndex.store (trackIdx, std::memory_order_release);
        state.fileUpdated.store (true, std::memory_order_release);
        state.stateVersion.fetch_add (1, std::memory_order_release);
    }

    // Called from UI thread
    void pushMidiMessage (const juce::MidiBuffer& buffer)
    {
        const juce::SpinLock::ScopedLockType lock (state.midiLock);
        state.midiQueue.push (buffer);
    }

    bool hasNewFile() const { return state.fileUpdated.load (std::memory_order_acquire); }

    // Called from audio thread
    void fetchAudioFileForTrack (juce::File& outFile, int& outTrackIdx)
    {
        const juce::SpinLock::ScopedLockType lock (state.fileLock);
        outFile = state.audioFile;
        outTrackIdx = state.trackIndex.load (std::memory_order_acquire);
        state.fileUpdated.store (false, std::memory_order_release);
    }

    // Called from audio thread
    bool fetchNextMidiBuffer (juce::MidiBuffer& outBuffer)
    {
        const juce::SpinLock::ScopedLockType lock (state.midiLock);
        if (state.midiQueue.empty()) return false;

        outBuffer.swapWith (state.midiQueue.front());
        state.midiQueue.pop();
        return true;
    }

private:
    UIState state;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UIToEngineBridge)
};
