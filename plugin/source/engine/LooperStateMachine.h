#pragma once
#include "LooperStateConfig.h"
#include "audio/AudioToUIBridge.h"
#include "engine/LoopTrack.h"
#include <JuceHeader.h>
#include <array>

// Context passed to state actions
struct StateContext
{
    LoopTrack* track;
    AudioToUIBridge* bridge;
    const juce::AudioBuffer<float>* inputBuffer;
    juce::AudioBuffer<float>* outputBuffer; // For playback
    int numSamples;
    double sampleRate;
    int trackIndex;
    bool bridgeInitialized;
};

// Function pointer types for state actions
using ProcessAudioFunc = void (*) (StateContext&);
using OnEnterFunc = void (*) (StateContext&);
using OnExitFunc = void (*) (StateContext&);

// State action table entry
struct StateActions
{
    ProcessAudioFunc processAudio;
    OnEnterFunc onEnter;
    OnExitFunc onExit;
};

// Concrete state action implementations
namespace StateHandlers
{
// Idle state
inline void idleProcessAudio (StateContext&) { /* No processing */ }
inline void idleOnEnter (StateContext&) {}
inline void idleOnExit (StateContext&) {}

// Stopped state
inline void stoppedProcessAudio (StateContext&) { /* No processing */ }
inline void stoppedOnEnter (StateContext&) {}
inline void stoppedOnExit (StateContext&) {}

// Playing state
inline void playingProcessAudio (StateContext& ctx)
{
    if (ctx.track && ctx.outputBuffer)
    {
        ctx.track->processPlayback (*ctx.outputBuffer, ctx.numSamples, false);
    }
}
inline void playingOnEnter (StateContext&) {}
inline void playingOnExit (StateContext&) {}

// Recording state
inline void recordingProcessAudio (StateContext& ctx)
{
    if (ctx.track && ctx.inputBuffer)
    {
        ctx.track->processRecord (*ctx.inputBuffer, ctx.numSamples, false);
    }
}
inline void recordingOnEnter (StateContext&) {}
inline void recordingOnExit (StateContext& ctx)
{
    // This is the ONLY place where finalizeLayer should be called for Recording
    if (ctx.track)
    {
        ctx.track->finalizeLayer (false);
        if (ctx.bridge)
        {
            ctx.bridge->signalWaveformChanged();
        }
    }
}

// Overdubbing state
inline void overdubbingProcessAudio (StateContext& ctx)
{
    if (ctx.track && ctx.inputBuffer && ctx.outputBuffer)
    {
        ctx.track->processRecord (*ctx.inputBuffer, ctx.numSamples, true);
        ctx.track->processPlayback (*ctx.outputBuffer, ctx.numSamples, true);
    }
}

inline void overdubbingOnEnter (StateContext& ctx) { ctx.track->initializeForNewOverdubSession(); }

inline void overdubbingOnExit (StateContext& ctx)
{
    // CRITICAL: Ensure recording is finalized when leaving Overdubbing state
    // This is the ONLY place where finalizeLayer should be called for Overdubbing
    if (ctx.track)
    {
        ctx.track->finalizeLayer (true);
        if (ctx.bridge)
        {
            ctx.bridge->signalWaveformChanged();
        }
    }
}

// PendingTrackChange state
inline void pendingProcessAudio (StateContext& ctx)
{
    // Same as playing
    playingProcessAudio (ctx);
}
inline void pendingOnEnter (StateContext&) {}
inline void pendingOnExit (StateContext&) {}

// Transitioning state
inline void transitioningProcessAudio (StateContext& ctx)
{
    // Same as playing
    playingProcessAudio (ctx);
}
inline void transitioningOnEnter (StateContext&) {}
inline void transitioningOnExit (StateContext&) {}
} // namespace StateHandlers

// State action table - maps each state to its handlers
constexpr std::array<StateActions, StateConfig::NUM_STATES> STATE_ACTION_TABLE = {
    StateActions { StateHandlers::idleProcessAudio, StateHandlers::idleOnEnter, StateHandlers::idleOnExit },
    StateActions { StateHandlers::stoppedProcessAudio, StateHandlers::stoppedOnEnter, StateHandlers::stoppedOnExit },
    StateActions { StateHandlers::playingProcessAudio, StateHandlers::playingOnEnter, StateHandlers::playingOnExit },
    StateActions { StateHandlers::recordingProcessAudio, StateHandlers::recordingOnEnter, StateHandlers::recordingOnExit },
    StateActions { StateHandlers::overdubbingProcessAudio, StateHandlers::overdubbingOnEnter, StateHandlers::overdubbingOnExit },
    StateActions { StateHandlers::pendingProcessAudio, StateHandlers::pendingOnEnter, StateHandlers::pendingOnExit },
    StateActions { StateHandlers::transitioningProcessAudio, StateHandlers::transitioningOnEnter, StateHandlers::transitioningOnExit }
};

// State machine class
class LooperStateMachine
{
public:
    LooperStateMachine() = default;

    bool transition (LooperState& current, LooperState target, StateContext& ctx)
    {
        if (! StateConfig::canTransition (current, target))
        {
            DBG ("Invalid transition: " << StateConfig::name (current) << " -> " << StateConfig::name (target));
            return false;
        }

        // Call exit handler for current state
        // CRITICAL: This is where recording finalization happens
        const auto& currentActions = STATE_ACTION_TABLE[static_cast<size_t> (current)];
        currentActions.onExit (ctx);

        // Transition
        current = target;

        // Call enter handler for new state
        const auto& targetActions = STATE_ACTION_TABLE[static_cast<size_t> (target)];
        targetActions.onEnter (ctx);

        return true;
    }

    void processAudio (LooperState current, StateContext& ctx)
    {
        const auto& actions = STATE_ACTION_TABLE[static_cast<size_t> (current)];
        actions.processAudio (ctx);
    }
};
