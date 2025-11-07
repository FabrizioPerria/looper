#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <queue>
#include <variant>
#include <vector>

/**
 * Unified message bus for UI <-> Engine communication (non-real-time paths)
 * 
 * Real-time audio state still uses dedicated bridges:
 * - AudioToUIBridge: waveform data & playback position
 * - EngineStateToUIBridge: recording/playing state
 */
class EngineMessageBus : juce::Timer
{
public:
    // ============================================================================
    // COMMAND SYSTEM (UI -> Engine)
    // ============================================================================

    enum class CommandType : uint8_t
    {
        TogglePlay,
        ToggleRecord,
        Stop,
        ToggleSyncTrack,
        ToggleSinglePlayMode,
        ToggleFreeze,

        Undo,
        Redo,
        Clear,

        NextTrack,
        PreviousTrack,
        SelectTrack,

        SetVolume,
        ToggleMute,
        ToggleSolo,
        ToggleVolumeNormalize,

        SetPlaybackSpeed,
        SetPlaybackPitch,
        TogglePitchLock,
        ToggleReverse,

        LoadAudioFile,

        SetExistingAudioGain,
        SetNewOverdubGain,

        SetMetronomeEnabled,
        SetMetronomeBPM,
        SetMetronomeTimeSignature,
        SetMetronomeStrongBeat,
        DisableMetronomeStrongBeat,
        SetMetronomeVolume,

        SetSubLoopRegion,
        ClearSubLoopRegion,

        SetOutputGain,
        SetInputGain
    };

    typedef std::
        variant<std::monostate, float, int, bool, juce::File, juce::AudioBuffer<float>, std::pair<int, int>, std::pair<float, float>>
            CommandPayload;

    struct Command
    {
        CommandType type;
        int trackIndex = -1;

        // Flexible payload using variant
        CommandPayload payload;
    };

    // ============================================================================
    // EVENT SYSTEM (Engine -> UI) - Listener Pattern
    // ============================================================================

    enum class EventType : uint8_t
    {
        NewOverdubGainLevels,
        OldOverdubGainLevels,
        NormalizeStateChanged,

        RecordingStateChanged,
        PlaybackStateChanged,

        ActiveTrackChanged,
        PendingTrackChanged,
        ActiveTrackCleared,

        TrackVolumeChanged,
        TrackMuteChanged,
        TrackSoloChanged,
        TrackSpeedChanged,
        TrackPitchChanged,
        TrackPitchLockChanged,
        TrackReverseDirection,

        MetronomeEnabledChanged,
        MetronomeBPMChanged,
        MetronomeTimeSignatureChanged,
        MetronomeStrongBeatChanged,
        MetronomeVolumeChanged,
        MetronomeBeatOccurred,

        TrackSyncChanged,
        SinglePlayModeChanged,
        FreezeStateChanged,
    };

    typedef std::variant<std::monostate, float, int, bool, std::pair<int, int>, std::pair<int, bool>, juce::String> EventData;
    struct Event
    {
        EventType type;
        int trackIndex = -1;

        EventData data;
    };

    // Listener interface - components implement this to receive events
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void handleEngineEvent (const Event& event) = 0;
    };

    // ============================================================================
    // PUBLIC API
    // ============================================================================

    EngineMessageBus() : commandFifo (FIFO_SIZE), eventFifo (FIFO_SIZE)
    {
        commandBuffer.resize (FIFO_SIZE);
        eventBuffer.resize (FIFO_SIZE);
        startTimerHz (120);
    }
    ~EngineMessageBus() override { stopTimer(); }

    // ============ COMMAND API (UI -> Engine) ============

    // UI Thread -> Send commands to engine
    void pushCommand (Command cmd)
    {
        auto writeIndex = commandFifo.write (1);
        if (writeIndex.blockSize1 > 0)
        {
            commandBuffer[writeIndex.startIndex1] = std::move (cmd);
            commandFifo.finishedWrite (1);
        }
    }

    // Audio Thread -> Process commands from UI
    bool popCommand (Command& outCmd)
    {
        auto readIndex = commandFifo.read (1);
        if (readIndex.blockSize1 > 0)
        {
            outCmd = std::move (commandBuffer[readIndex.startIndex1]);
            commandFifo.finishedRead (1);
            return true;
        }
        return false;
    }

    // Check if there are pending commands (useful for debugging)
    bool hasCommands() const { return commandFifo.getNumReady() > 0; }

    // ============ EVENT API (Engine -> UI) ============

    // UI Thread -> Register to receive events
    void addListener (Listener* listener)
    {
        const juce::SpinLock::ScopedLockType lock (listenerLock);
        listeners.addIfNotAlreadyThere (listener);
    }

    // UI Thread -> Unregister from events
    void removeListener (Listener* listener)
    {
        const juce::SpinLock::ScopedLockType lock (listenerLock);
        listeners.removeAllInstancesOf (listener);
    }

    // Audio Thread -> Broadcast event to all listeners
    void broadcastEvent (Event evt)
    {
        auto writeIndex = eventFifo.write (1);
        if (writeIndex.blockSize1 > 0)
        {
            eventBuffer[writeIndex.startIndex1] = std::move (evt);
            eventFifo.finishedWrite (1);
        }
    }

    // Message Thread -> Dispatch pending events to listeners
    void dispatchPendingEvents()
    {
        const int numReady = eventFifo.getNumReady();
        if (numReady == 0) return;

        auto readIndex = eventFifo.read (numReady);

        // Get snapshot of listeners under lock
        juce::Array<Listener*> listenerSnapshot;
        {
            const juce::SpinLock::ScopedLockType lock (listenerLock);
            listenerSnapshot = listeners;
        }

        // Process first block
        for (int i = 0; i < readIndex.blockSize1; ++i)
        {
            const auto& event = eventBuffer[readIndex.startIndex1 + i];
            for (auto* listener : listenerSnapshot)
                listener->handleEngineEvent (event);
        }

        // Process second block if it exists
        for (int i = 0; i < readIndex.blockSize2; ++i)
        {
            const auto& event = eventBuffer[readIndex.startIndex2 + i];
            for (auto* listener : listenerSnapshot)
                listener->handleEngineEvent (event);
        }

        eventFifo.finishedRead (numReady);
    }

    // Clear all pending messages (e.g., on shutdown)
    void clear()
    {
        while (commandFifo.getNumReady() > 0)
        {
            auto readIndex = commandFifo.read (1);
            if (readIndex.blockSize1 > 0) commandFifo.finishedRead (1);
        }

        while (eventFifo.getNumReady() > 0)
        {
            auto readIndex = eventFifo.read (1);
            if (readIndex.blockSize1 > 0) eventFifo.finishedRead (1);
        }
    }

private:
    void timerCallback() override { dispatchPendingEvents(); }

    static constexpr int FIFO_SIZE = 1024;

    // Command system (UI -> Engine)
    std::vector<Command> commandBuffer;
    juce::AbstractFifo commandFifo;

    // Event system (Engine -> UI)
    std::vector<Event> eventBuffer;
    juce::AbstractFifo eventFifo;

    juce::Array<Listener*> listeners;
    juce::SpinLock listenerLock; // Still need this one for listener management

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineMessageBus)
};
