#include "MidiCommandDispatcher.h"
#include "LooperEngine.h"

#include "LooperEngine.h" // NOW we can include it!
#include "MidiCommandDispatcher.h"

namespace CommandExecutors
{
static void executeToggleRecord (LooperEngine& engine, int) { engine.toggleRecord(); }

static void executeTogglePlay (LooperEngine& engine, int) { engine.togglePlay(); }

static void executeUndo (LooperEngine& engine, int trackIndex) { engine.undo (trackIndex); }

static void executeRedo (LooperEngine& engine, int trackIndex) { engine.redo (trackIndex); }

static void executeClear (LooperEngine& engine, int trackIndex) { engine.clear (trackIndex); }

static void executeNextTrack (LooperEngine& engine, int) { engine.selectNextTrack(); }

static void executePrevTrack (LooperEngine& engine, int) { engine.selectPreviousTrack(); }

static void executeToggleSolo (LooperEngine& engine, int trackIndex)
{
    auto* track = engine.getTrackByIndex (trackIndex);
    if (track) engine.setTrackSoloed (trackIndex, ! track->isSoloed());
}

static void executeToggleMute (LooperEngine& engine, int trackIndex)
{
    auto* track = engine.getTrackByIndex (trackIndex);
    if (track) engine.setTrackMuted (trackIndex, ! track->isMuted());
}

static void executeLoadFile (LooperEngine& engine, int trackIndex)
{
    juce::File defaultFile = juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("backing.wav");
    engine.loadWaveFileToTrack (defaultFile, trackIndex);
}

static void executeToggleReverse (LooperEngine& engine, int trackIndex)
{
    auto* track = engine.getTrackByIndex (trackIndex);
    if (track)
    {
        if (track->isPlaybackDirectionForward())
            track->setPlaybackDirectionBackward();
        else
            track->setPlaybackDirectionForward();
    }
}

static void executeToggleKeepPitch (LooperEngine& engine, int trackIndex)
{
    bool current = engine.getKeepPitchWhenChangingSpeed (trackIndex);
    engine.setKeepPitchWhenChangingSpeed (trackIndex, ! current);
}

static void executeNone (LooperEngine&, int)
{
    // No-op
}
} // namespace CommandExecutors

const CommandFunc COMMAND_DISPATCH_TABLE[(size_t) MidiCommandId::COUNT] = {
    [(size_t) MidiCommandId::None] = CommandExecutors::executeNone,
    [(size_t) MidiCommandId::ToggleRecord] = CommandExecutors::executeToggleRecord,
    [(size_t) MidiCommandId::TogglePlay] = CommandExecutors::executeTogglePlay,
    [(size_t) MidiCommandId::Undo] = CommandExecutors::executeUndo,
    [(size_t) MidiCommandId::Redo] = CommandExecutors::executeRedo,
    [(size_t) MidiCommandId::Clear] = CommandExecutors::executeClear,
    [(size_t) MidiCommandId::NextTrack] = CommandExecutors::executeNextTrack,
    [(size_t) MidiCommandId::PrevTrack] = CommandExecutors::executePrevTrack,
    [(size_t) MidiCommandId::ToggleSolo] = CommandExecutors::executeToggleSolo,
    [(size_t) MidiCommandId::ToggleMute] = CommandExecutors::executeToggleMute,
    [(size_t) MidiCommandId::LoadFile] = CommandExecutors::executeLoadFile,
    [(size_t) MidiCommandId::ToggleReverse] = CommandExecutors::executeToggleReverse,
    [(size_t) MidiCommandId::ToggleKeepPitch] = CommandExecutors::executeToggleKeepPitch
};

void MidiCommandDispatcher::dispatch (MidiCommandId commandId, LooperEngine& engine, int trackIndex)
{
    if (commandId == MidiCommandId::None || commandId >= MidiCommandId::COUNT) return;

    COMMAND_DISPATCH_TABLE[static_cast<size_t> (commandId)](engine, trackIndex);
}
