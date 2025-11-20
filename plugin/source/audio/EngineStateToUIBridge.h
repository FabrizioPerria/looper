#pragma once

#include "engine/Constants.h"
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
        std::atomic<int> numChannels { 0 };

        std::unique_ptr<MeterContext> inputMeter;
        std::unique_ptr<MeterContext> outputMeter;

        std::atomic<int> stateVersion { 0 };
    };

    EngineStateToUIBridge() {}

    void setNumChannels (int newNumChannels)
    {
        numChannels = newNumChannels;
        state.inputMeter = std::make_unique<MeterContext> (newNumChannels);
        state.outputMeter = std::make_unique<MeterContext> (newNumChannels);
    }

    // Called from AUDIO THREAD
    void updateFromAudioThread (bool recording,
                                bool playing,
                                int activeTrack,
                                int pendingTrack,
                                int numTracks,
                                MeterContext& inputMeter,
                                MeterContext& outputMeter)
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
        // if (state.inputMeter == nullptr)
        // {
        //     peakInLeft = 0.0f;
        //     rmsInLeft = 0.0f;
        //     peakInRight = 0.0f;
        //     rmsInRight = 0.0f;
        //     return;
        // }
        peakInLeft = state.inputMeter->getChannel (LEFT_CHANNEL)->getPeakLevel();
        rmsInLeft = state.inputMeter->getChannel (LEFT_CHANNEL)->getRMSLevel();
        if (state.numChannels.load (std::memory_order_relaxed) < 2)
        {
            peakInRight = peakInLeft;
            rmsInRight = rmsInLeft;
            return;
        }
        else
        {
            peakInRight = state.inputMeter->getChannel (RIGHT_CHANNEL)->getPeakLevel();
            rmsInRight = state.inputMeter->getChannel (RIGHT_CHANNEL)->getRMSLevel();
            return;
        }
    }

    void getMeterOutputLevels (float& peakOutLeft, float& rmsOutLeft, float& peakOutRight, float& rmsOutRight)
    {
        // if (state.outputMeter == nullptr)
        // {
        //     peakOutLeft = 0.0f;
        //     rmsOutLeft = 0.0f;
        //     peakOutRight = 0.0f;
        //     rmsOutRight = 0.0f;
        //     return;
        // }
        peakOutLeft = state.outputMeter->getChannel (LEFT_CHANNEL)->getPeakLevel();
        rmsOutLeft = state.outputMeter->getChannel (LEFT_CHANNEL)->getRMSLevel();
        if (state.numChannels.load (std::memory_order_relaxed) < 2)
        {
            peakOutRight = peakOutLeft;
            rmsOutRight = rmsOutLeft;
            return;
        }
        else
        {
            peakOutRight = state.outputMeter->getChannel (RIGHT_CHANNEL)->getPeakLevel();
            rmsOutRight = state.outputMeter->getChannel (RIGHT_CHANNEL)->getRMSLevel();
            return;
        }
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
    int numChannels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineStateToUIBridge)
};
