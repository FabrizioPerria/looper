#pragma once
#include "LooperStateConfig.h"
#include "engine/Constants.h"
#include "engine/LoopTrack.h"
#include <JuceHeader.h>
#include <array>

// Context passed to state actions
//
struct StateContext
{
    LoopTrack* track;
    const juce::AudioBuffer<float>* inputBuffer;
    juce::AudioBuffer<float>* outputBuffer; // For playback
    int numSamples;
    double sampleRate;
    int trackIndex;
    bool wasRecording;
    bool isSinglePlayMode;
    int syncMasterLength;
    int syncMasterTrackIndex;
    std::array<std::unique_ptr<LoopTrack>, NUM_TRACKS>* allTracks;
    std::array<bool, NUM_TRACKS>* tracksToPlay;
};

// Function pointer types for state actions
using ProcessAudioFunc = void (*) (StateContext&, const LooperState&);
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
inline void idleProcessAudio (StateContext&, const LooperState&) { /* No processing */ }
inline void idleOnEnter (StateContext&) {}
inline void idleOnExit (StateContext&) {}

// Stopped state
inline void stoppedProcessAudio (StateContext&, const LooperState&) { /* No processing */ }
inline void stoppedOnEnter (StateContext&) {}
inline void stoppedOnExit (StateContext&) {}

// Playing state
inline void playingProcessAudio (StateContext& ctx, const LooperState& currentState)
{
    if (ctx.outputBuffer)
    {
        for (int i = 0; i < NUM_TRACKS; ++i)
        {
            if (! ctx.tracksToPlay->at ((size_t) i)) continue;
            ctx.allTracks->at ((size_t) i)->processPlayback (*ctx.outputBuffer, ctx.numSamples, false, currentState);
        }
    }
}
inline void playingOnEnter (StateContext&) {}
inline void playingOnExit (StateContext&) {}

// Recording state
inline void recordingProcessAudio (StateContext& ctx, const LooperState& currentState)
{
    if (ctx.track && ctx.inputBuffer)
    {
        ctx.track->processRecord (*ctx.inputBuffer, ctx.numSamples, false, currentState);
    }
    playingProcessAudio (ctx, currentState);
}

inline void recordingOnEnter (StateContext& ctx)
{
    if (ctx.track->isSynced() && ! ctx.isSinglePlayMode && ctx.syncMasterLength > 0)
    {
        auto* masterTrack = ctx.allTracks->at ((size_t) ctx.syncMasterTrackIndex).get();
        int masterStart = masterTrack->getCurrentReadPosition();
        ctx.track->setWritePosition (masterStart);
    }
}

static bool shouldSyncRecording (StateContext& ctx)
{
    return (ctx.syncMasterLength <= 0) || (ctx.track->isSynced() && ! ctx.isSinglePlayMode);
}

static int quantizeLengthToMaster (int recordedLength, int masterLength)
{
    if (masterLength <= 0) return recordedLength;

    // Quantize to nearest multiple of master length
    const auto multiples = (recordedLength / masterLength) + 1;
    return multiples * masterLength;
}

static int calculateFinalLength (StateContext& ctx, int recordedLength)
{
    if (shouldSyncRecording (ctx)) return quantizeLengthToMaster (recordedLength, ctx.syncMasterLength);
    return recordedLength;
}

inline void recordingOnExit (StateContext& ctx)
{
    if (ctx.track)
    {
        int recordedLength = ctx.track->getCurrentWritePosition();
        int finalLength = calculateFinalLength (ctx, recordedLength);

        ctx.track->finalizeLayer (false, finalLength);
    }
    if (ctx.track->isSynced() && ! ctx.isSinglePlayMode && ctx.syncMasterLength > 0 && ctx.trackIndex != ctx.syncMasterTrackIndex)
    {
        auto* masterTrack = ctx.allTracks->at ((size_t) ctx.syncMasterTrackIndex).get();
        int masterStart = masterTrack->getCurrentReadPosition();
        ctx.track->setReadPosition (masterStart);
    }
}

// Overdubbing state
inline void overdubbingProcessAudio (StateContext& ctx, const LooperState& currentState)
{
    if (ctx.track && ctx.inputBuffer && ctx.outputBuffer)
    {
        ctx.track->processRecord (*ctx.inputBuffer, ctx.numSamples, true, currentState);
    }
    playingProcessAudio (ctx, currentState);
}

inline void overdubbingOnEnter (StateContext& ctx) { ctx.track->initializeForNewOverdubSession(); }

inline void overdubbingOnExit (StateContext& ctx)
{
    // CRITICAL: Ensure recording is finalized when leaving Overdubbing state
    // This is the ONLY place where finalizeLayer should be called for Overdubbing
    if (ctx.track)
    {
        ctx.track->finalizeLayer (true, ctx.track->isSynced() ? ctx.syncMasterLength : ctx.track->getCurrentWritePosition());
    }
}

// PendingTrackChange state
inline void pendingProcessAudio (StateContext& ctx, const LooperState& currentState)
{
    // Same as playing
    playingProcessAudio (ctx, currentState);
}
inline void pendingOnEnter (StateContext&) {}
inline void pendingOnExit (StateContext&) {}

// Transitioning state
inline void transitioningProcessAudio (StateContext& ctx, const LooperState& currentState)
{
    // Same as playing
    playingProcessAudio (ctx, currentState);
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

        const auto& currentActions = STATE_ACTION_TABLE[static_cast<size_t> (current)];
        currentActions.onExit (ctx);

        current = target;

        const auto& targetActions = STATE_ACTION_TABLE[static_cast<size_t> (target)];
        targetActions.onEnter (ctx);

        return true;
    }

    void processAudio (const LooperState& current, StateContext& ctx)
    {
        const auto& actions = STATE_ACTION_TABLE[static_cast<size_t> (current)];
        actions.processAudio (ctx, current);
    }
};
