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
        std::atomic<int> stateVersion { 0 };
        juce::SpinLock fileLock;
    };

    UIToEngineBridge() = default;

    // Called from UI thread
    void updateAudioFile (const juce::File& newFile)
    {
        const juce::SpinLock::ScopedLockType lock (state.fileLock);
        state.audioFile = newFile;
        state.fileUpdated.store (true, std::memory_order_release);
        state.stateVersion.fetch_add (1, std::memory_order_release);
    }

    bool hasNewFile() const { return state.fileUpdated.load (std::memory_order_acquire); }

    // Called from audio thread
    juce::File getAudioFile()
    {
        const juce::SpinLock::ScopedLockType lock (state.fileLock);
        state.fileUpdated.store (false, std::memory_order_release);
        return state.audioFile;
    }

private:
    UIState state;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UIToEngineBridge)
};
