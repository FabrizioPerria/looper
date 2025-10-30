#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <queue>
#include <variant>

/**
 * Unified message bus for UI <-> Engine communication (non-real-time paths)
 * 
 * Real-time audio state still uses dedicated bridges:
 * - AudioToUIBridge: waveform data & playback position
 * - EngineStateToUIBridge: recording/playing state
 */
class EngineMessageBus
{
public:
    // ============================================================================
    // COMMAND SYSTEM (UI -> Engine)
    // ============================================================================

    enum class CommandType : uint8_t
    {
        TogglePlay,
        ToggleRecord,

        Undo,
        Redo,
        Clear,

        NextTrack,
        PreviousTrack,
        SelectTrack,

        SetVolume,
        SetMute,
        SetSolo,
        VolumeNormalizeToggle,

        SetPlaybackSpeed,
        SetPlaybackPitch,
        SetKeepPitch,
        SetPlaybackDirection,

        // File operations
        LoadAudioFile,

        // Track editing
        SetOverdubGains,

        // MIDI pass-through (for buttons/notes)
        MidiMessage,

        // Future: Menu items, preferences, etc.
        SetMetronomeEnabled,
        SetMetronomeBPM,
        SaveProject,
        LoadProject
    };

    struct Command
    {
        CommandType type;
        int trackIndex = -1;

        // Flexible payload using variant
        std::variant<std::monostate,           // Empty
                     float,                    // Single float (volume, speed, pitch)
                     bool,                     // Boolean (mute, solo, keepPitch)
                     juce::File,               // File path
                     juce::MidiBuffer,         // MIDI data
                     juce::AudioBuffer<float>, // Audio buffer (for backing tracks)
                     std::pair<float, float>   // Two floats (overdub gains)
                     >
            payload;

        // Convenience constructors
        static Command setVolume (int track, float volume) { return { CommandType::SetVolume, track, volume }; }

        static Command setMute (int track, bool muted) { return { CommandType::SetMute, track, muted }; }

        static Command setSolo (int track, bool soloed) { return { CommandType::SetSolo, track, soloed }; }

        static Command setPlaybackSpeed (int track, float speed) { return { CommandType::SetPlaybackSpeed, track, speed }; }

        static Command setPlaybackPitch (int track, float pitch) { return { CommandType::SetPlaybackPitch, track, pitch }; }

        static Command setKeepPitch (int track, bool keep) { return { CommandType::SetKeepPitch, track, keep }; }

        static Command setPlaybackDirection (int track, bool forward) { return { CommandType::SetPlaybackDirection, track, forward }; }

        static Command loadAudioFile (int track, const juce::File& file) { return { CommandType::LoadAudioFile, track, file }; }

        static Command loadBackingTrack (int track, const juce::AudioBuffer<float>& buffer)
        {
            Command cmd;
            cmd.type = CommandType::LoadBackingTrack;
            cmd.trackIndex = track;
            cmd.payload = buffer;
            return cmd;
        }

        static Command setOverdubGains (int track, float oldGain, float newGain)
        {
            return { CommandType::SetOverdubGains, track, std::make_pair (oldGain, newGain) };
        }

        static Command midiMessage (const juce::MidiBuffer& buffer) { return { CommandType::MidiMessage, -1, buffer }; }

        static Command setMetronomeEnabled (bool enabled) { return { CommandType::SetMetronomeEnabled, -1, enabled }; }

        static Command setMetronomeBPM (float bpm) { return { CommandType::SetMetronomeBPM, -1, bpm }; }
    };

    // ============================================================================
    // EVENT SYSTEM (Engine -> UI) - Listener Pattern
    // ============================================================================

    enum class EventType : uint8_t
    {
        // Track state changes (non-real-time, complementary to EngineStateToUIBridge)
        TrackVolumeChanged,
        TrackMuteChanged,
        TrackSoloChanged,
        TrackSpeedChanged,
        TrackPitchChanged,

        // File operation results
        FileLoadSucceeded,
        FileLoadFailed,

        // Project operations
        ProjectSaved,
        ProjectSaveFailed,

        // Errors/warnings
        ErrorOccurred,
        WarningOccurred
    };

    struct Event
    {
        EventType type;
        int trackIndex = -1;

        std::variant<std::monostate,
                     float,
                     bool,
                     juce::String // Error messages, file paths, etc.
                     >
            data;

        static Event trackVolumeChanged (int track, float volume) { return { EventType::TrackVolumeChanged, track, volume }; }

        static Event trackMuteChanged (int track, bool muted) { return { EventType::TrackMuteChanged, track, muted }; }

        static Event fileLoadSucceeded (int track, const juce::String& filePath)
        {
            return { EventType::FileLoadSucceeded, track, filePath };
        }

        static Event fileLoadFailed (int track, const juce::String& errorMsg) { return { EventType::FileLoadFailed, track, errorMsg }; }

        static Event errorOccurred (const juce::String& message) { return { EventType::ErrorOccurred, -1, message }; }
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

    EngineMessageBus() = default;

    // ============ COMMAND API (UI -> Engine) ============

    // UI Thread -> Send commands to engine
    void pushCommand (Command cmd)
    {
        const juce::SpinLock::ScopedLockType lock (commandLock);
        commandQueue.push (std::move (cmd));
    }

    // Audio Thread -> Process commands from UI
    bool popCommand (Command& outCmd)
    {
        const juce::SpinLock::ScopedLockType lock (commandLock);
        if (commandQueue.empty()) return false;

        outCmd = std::move (commandQueue.front());
        commandQueue.pop();
        return true;
    }

    // Check if there are pending commands (useful for debugging)
    bool hasCommands() const
    {
        const juce::SpinLock::ScopedLockType lock (commandLock);
        return ! commandQueue.empty();
    }

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
    // This pushes to a queue for async delivery on message thread
    void broadcastEvent (Event evt)
    {
        const juce::SpinLock::ScopedLockType lock (eventLock);
        pendingEvents.push (std::move (evt));
    }

    // Message Thread -> Dispatch pending events to listeners
    // Call this from a timer or message manager callback
    void dispatchPendingEvents()
    {
        // Move events to a local queue to minimize lock time
        std::queue<Event> eventsToDispatch;
        {
            const juce::SpinLock::ScopedLockType lock (eventLock);
            eventsToDispatch.swap (pendingEvents);
        }

        // Dispatch events to all listeners (without holding locks)
        while (! eventsToDispatch.empty())
        {
            const auto& event = eventsToDispatch.front();

            // Get snapshot of listeners under lock
            juce::Array<Listener*> listenerSnapshot;
            {
                const juce::SpinLock::ScopedLockType lock (listenerLock);
                listenerSnapshot = listeners;
            }

            // Call listeners without holding lock
            for (auto* listener : listenerSnapshot)
                listener->handleEngineEvent (event);

            eventsToDispatch.pop();
        }
    }

    // Clear all pending messages (e.g., on shutdown)
    void clear()
    {
        {
            const juce::SpinLock::ScopedLockType lock (commandLock);
            while (! commandQueue.empty())
                commandQueue.pop();
        }
        {
            const juce::SpinLock::ScopedLockType lock (eventLock);
            while (! pendingEvents.empty())
                pendingEvents.pop();
        }
    }

private:
    // Command queue (UI -> Engine)
    std::queue<Command> commandQueue;
    mutable juce::SpinLock commandLock;

    // Event system (Engine -> UI via listeners)
    std::queue<Event> pendingEvents; // Events waiting to be dispatched
    juce::SpinLock eventLock;

    juce::Array<Listener*> listeners;
    juce::SpinLock listenerLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineMessageBus)
};
