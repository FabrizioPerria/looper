#pragma once

#include "engine/LevelMeter.h"
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

        std::unique_ptr<StereoMeterContext> inputMeter = std::make_unique<StereoMeterContext>();
        std::unique_ptr<StereoMeterContext> outputMeter = std::make_unique<StereoMeterContext>();

        std::atomic<int> stateVersion { 0 };
    };

    EngineStateToUIBridge() = default;

    // Called from AUDIO THREAD
    void updateFromAudioThread (bool recording,
                                bool playing,
                                int activeTrack,
                                int pendingTrack,
                                int numTracks,
                                StereoMeterContext& inputMeter,
                                StereoMeterContext& outputMeter)
    {
        state.inputMeter->update (inputMeter);
        state.outputMeter->update (outputMeter);

        state.isRecording.store (recording, std::memory_order_relaxed);
        state.isPlaying.store (playing, std::memory_order_relaxed);
        state.activeTrackIndex.store (activeTrack, std::memory_order_relaxed);
        state.pendingTrackIndex.store (pendingTrack, std::memory_order_relaxed);
        state.numTracks.store (numTracks, std::memory_order_relaxed);
        state.stateVersion.fetch_add (1, std::memory_order_release);
    }

    void getMeterInputLevels (float& peakInLeft, float& rmsInLeft, float& peakInRight, float& rmsInRight)
    {
        peakInLeft = state.inputMeter->getLeftChannel()->getPeakLevel();
        rmsInLeft = state.inputMeter->getLeftChannel()->getRMSLevel();
        peakInRight = state.inputMeter->getRightChannel()->getPeakLevel();
        rmsInRight = state.inputMeter->getRightChannel()->getRMSLevel();
    }

    void getMeterOutputLevels (float& peakOutLeft, float& rmsOutLeft, float& peakOutRight, float& rmsOutRight)
    {
        peakOutLeft = state.outputMeter->getLeftChannel()->getPeakLevel();
        rmsOutLeft = state.outputMeter->getLeftChannel()->getRMSLevel();
        peakOutRight = state.outputMeter->getRightChannel()->getPeakLevel();
        rmsOutRight = state.outputMeter->getRightChannel()->getRMSLevel();
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
