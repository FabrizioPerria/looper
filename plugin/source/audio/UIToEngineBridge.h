#pragma once

#include <JuceHeader.h>
#include <atomic>

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

    bool hasNewFile() const { return state.fileUpdated.load (std::memory_order_acquire); }

    // Called from audio thread
    void fetchAudioFileForTrack (juce::File& outFile, int& outTrackIdx)
    {
        const juce::SpinLock::ScopedLockType lock (state.fileLock);
        outFile = state.audioFile;
        outTrackIdx = state.trackIndex.load (std::memory_order_acquire);
        state.fileUpdated.store (false, std::memory_order_release);
    }

private:
    UIState state;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UIToEngineBridge)
};
