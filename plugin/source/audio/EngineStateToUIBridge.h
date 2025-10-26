#pragma once

#include <JuceHeader.h>
#include <atomic>

class EngineStateToUIBridge
{
public:
    struct EngineState
    {
        std::atomic<bool> isRecording { false };
        std::atomic<bool> isPlaying { false };
        std::atomic<int> activeTrackIndex { 0 };
        std::atomic<int> pendingTrackIndex { -1 };
        std::atomic<int> numTracks { 0 };
        std::atomic<int> stateVersion { 0 };
    };

    EngineStateToUIBridge() = default;

    // Called from AUDIO THREAD
    void updateFromAudioThread (bool recording, bool playing, int activeTrack, int pendingTrack, int numTracks)
    {
        state.isRecording.store (recording, std::memory_order_relaxed);
        state.isPlaying.store (playing, std::memory_order_relaxed);
        state.activeTrackIndex.store (activeTrack, std::memory_order_relaxed);
        state.pendingTrackIndex.store (pendingTrack, std::memory_order_relaxed);
        state.numTracks.store (numTracks, std::memory_order_relaxed);
        state.stateVersion.fetch_add (1, std::memory_order_release);
    }

    // Called from UI THREAD
    void getEngineState (bool& recording, bool& playing, int& activeTrack, int& pendingTrack, int& numTracks)
    {
        recording = state.isRecording.load (std::memory_order_relaxed);
        playing = state.isPlaying.load (std::memory_order_relaxed);
        activeTrack = state.activeTrackIndex.load (std::memory_order_relaxed);
        pendingTrack = state.pendingTrackIndex.load (std::memory_order_relaxed);
        numTracks = state.numTracks.load (std::memory_order_relaxed);
    }

    const EngineState& getState() const { return state; }

private:
    EngineState state;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineStateToUIBridge)
};
