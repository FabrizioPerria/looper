# 4-Track Looper Plugin

A real-time audio looper built with JUCE, featuring 4 independent tracks, granular freeze effects,
variable speed/pitch playback, comprehensive MIDI control, and professional undo/redo capabilities.

## Core Features

### Multi-Track Recording & Playback

- **4 independent stereo tracks** with individual controls
- **Recording modes**: Fresh recording or overdubbing on existing loops
- **Single/Multi-track playback**: Toggle between solo track or simultaneous multi-track playback
- **Track synchronization**: Automatic length quantization to master track when sync enabled
- **Audio file import**: Drag-and-drop support for WAV, MP3, AIFF, FLAC, M4A

### Playback Controls

- **Variable speed**: 0.5x to 2.0x with snap points at common values (0.5, 0.75, 1.0, 1.5, 2.0)
- **Pitch shifting**: ±2 semitones independent of speed
- **Pitch lock**: Maintain original pitch when changing playback speed using SoundTouch library
- **Reverse playback**: Play tracks backward
- **Sub-loop regions**: Define and loop within specific sections via shift+drag on waveform

### Track Mixing

- **Individual volume control**: 0.0x to 2.0x gain per track
- **Mute/Solo**: Standard mixing controls with visual feedback
- **Overdub gain controls**: Separate gain for existing audio and new overdubs
- **Global input/output gain**: Master level controls with LED metering

### Effects & Processing

- **Granular Freeze**: Real-time granular synthesis freeze effect with:
  - Adjustable grain parameters (size, density)
  - Multiple grain modulation with pitch and amplitude variation
  - Smooth tail fade-out when disabling
  - Configurable freeze intensity level

### Metronome

- **Click track**: Adjustable BPM (30-300), time signature, and volume
- **Strong beat accent**: Define which beat receives emphasis
- **Tap tempo**: Click metronome LED 2+ times to set BPM from timing
- **Visual beat indicator**: LED shows current beat number and flashes on beat

### Undo/Redo System

- **Multi-level undo**: Up to 5 undo states per track
- **Redo support**: Restore undone changes
- **Efficient memory**: Only stores deltas between layers using swap-based buffer management

### MIDI Control

- **Full MIDI mapping**: Map any CC or Note-On to any function
- **MIDI learn mode**: Click "Learn" on any control to assign MIDI input
- **Save/load mappings**: Persistent MIDI configuration per user
- **Default mappings**: Pre-configured CC/note assignments for common controllers

### User Interface

- **Waveform display**: Real-time visualization with playhead tracking
- **Track accent bars**: Visual indication of active/pending track
- **Tokyo Night theme**: Dark, modern color scheme with accent colors per control type
- **Time overlays**: Display current position, total length, and sub-loop boundaries

## Architecture Overview

### State Machine

The looper operates through a deterministic state machine defined in `LooperStateConfig.h` and
`LooperStateMachine.h`:

**States:**

- `Idle`: No content, ready to record
- `Stopped`: Loop exists but not playing
- `Playing`: Playback active
- `Recording`: Creating first loop layer
- `Overdubbing`: Adding audio to existing loop
- `PendingTrackChange`: Waiting for loop end before switching tracks
- `Transitioning`: Brief state during track switches

**State Transitions:** Each state defines valid transitions via compile-time bitfield masks. The
state machine validates transitions and executes `onEnter`/`onExit` handlers plus per-frame audio
processing callbacks.

**Example flow:**

```
Idle → Recording → Playing → Overdubbing → Playing → Stopped → Idle
```

### Message & Command System

#### Commands (UI → Engine)

The `EngineMessageBus` provides a lock-free FIFO queue for UI commands sent to the audio thread:

```cpp
// UI thread
messageBus->pushCommand({
    CommandType::SetPlaybackSpeed,
    trackIndex: 2,
    payload: 1.5f
});

// Audio thread (processBlock)
EngineMessageBus::Command cmd;
while (messageBus->popCommand(cmd)) {
    commandHandlers[cmd.type](cmd);
}
```

**Command Types:**

- Transport: `TogglePlay`, `ToggleRecord`, `Stop`
- Track selection: `NextTrack`, `PreviousTrack`, `SelectTrack`
- Playback: `SetPlaybackSpeed`, `SetPlaybackPitch`, `ToggleReverse`
- Mixing: `SetVolume`, `ToggleMute`, `ToggleSolo`
- Editing: `Undo`, `Redo`, `Clear`
- Effects: `ToggleFreeze`, `SetFreezeLevel`
- Metronome: `SetMetronomeBPM`, `SetMetronomeTimeSignature`
- Global: `SetInputGain`, `SetOutputGain`

#### Events (Engine → UI)

Complementary event system broadcasts state changes from audio thread to UI listeners:

```cpp
// Audio thread
messageBus->broadcastEvent({
    EventType::TrackSpeedChanged,
    trackIndex: 1,
    data: 1.25f
});

// UI thread (timer callback dispatches events)
void handleEngineEvent(const Event& event) {
    if (event.type == EventType::TrackSpeedChanged) {
        speedSlider.setValue(std::get<float>(event.data));
    }
}
```

Events are buffered in a lock-free FIFO and dispatched at ~120Hz via timer on the message thread.

### Audio Pipeline

#### BufferManager

Manages circular buffer storage and access via `LoopFifo`:

**Write path (Recording/Overdubbing):**

```cpp
bufferManager.writeToAudioBuffer(
    writeFunc,        // How to write (copy vs. mix)
    sourceBuffer,     // Input audio
    numSamples,
    isOverdub,
    syncWriteWithRead // Keep write head synced to read head
);
```

**Read path (Playback):**

```cpp
bufferManager.readFromAudioBuffer(
    readFunc,         // How to read (copy vs. add)
    destBuffer,
    numSamples,
    speedMultiplier,  // Includes direction (negative = reverse)
    isOverdub
);
```

The `LoopFifo` handles wrap-around, region looping, and position tracking with sub-sample accuracy
for smooth playback.

#### PlaybackEngine

Implements time-stretching and pitch-shifting using the SoundTouch library:

**Fast path** (1.0x speed, forward, no pitch shift):

- Direct buffer copy, no processing

**Time-stretch path** (any other speed/pitch):

- Linearizes circular buffer into temporary buffer
- Feeds audio through per-channel SoundTouch instances
- SoundTouch configured for low-latency (82ms sequence, 28ms seek window)
- Pitch lock: Speed controlled via tempo, pitch independent
- No pitch lock: Speed controlled via rate (pitch follows speed naturally)

#### VolumeProcessor

Applies per-track gain and mixing:

- Crossfade at loop boundaries (configurable length, default 10ms)
- Automatic normalization to -1dB target level
- Separate gain multipliers for existing audio vs. new overdubs
- Smooth gain ramping to avoid clicks

### Data Flow (Audio → UI)

#### AudioToUIBridge

Triple-buffer system for lock-free waveform transfer:

```
Audio Thread          UI Thread
    ↓                     ↓
[Write Buffer] ←→ [Swap] → [Read Buffer]
                    ↓
              [Retired Buffer]
```

**Update flow:**

1. Audio thread writes snapshot to write buffer
2. Atomically swaps write/retired buffer indices
3. UI thread reads from UI buffer at leisure
4. Only updates UI when version number changes

**Throttling:**

- Full updates every 100ms during recording
- Immediate updates when recording stops/starts
- Position updates every frame via VBlank callback

#### WaveformCache

Downsamples full-resolution audio to pixel-resolution min/max pairs:

```cpp
cache.updateFromBuffer(
    audioBuffer,    // Full resolution audio
    sourceLength,
    targetWidth     // Pixels in waveform view
);
```

Generated on background thread, rendered via `LinearRenderer` which draws:

- Min/max waveform envelope
- Center line
- Playhead with glow effect
- Sub-loop region highlighting
- Time overlays (position, length)

### Memory Management

#### Undo System

Uses LIFO (Last-In-First-Out) buffer stacks managed by `LoopLifo`:

**On overdub start:**

```cpp
undoManager.finalizeCopyAndPush(currentLength);
// Current buffer → Undo stack
```

**On undo:**

```cpp
undoManager.undo(currentBuffer);
// Current ↔ Undo top
// Current → Redo stack
```

Buffers are **swapped, not copied**, making undo/redo operations O(1) in time and zero-copy.

#### Buffer Allocation

All audio buffers pre-allocated at `prepareToPlay()`:

- Loop buffers: 10 minutes × sample rate × channels
- Undo stacks: 5 layers × buffer size per track
- Playback buffers: Block size × channels

Aligned to block boundaries for SIMD operations.

### MIDI System

#### MidiMappingManager

Maintains two compile-time lookup tables:

```cpp
std::array<CommandType, 128> noteOnMapping;  // MIDI note → Command
std::array<CommandType, 128> ccMapping;      // CC number → Command
```

**MIDI Learn:**

1. User clicks "Learn" button
2. `startMidiLearn(commandType)` sets learning mode
3. Next MIDI input assigns to that command
4. Previous mapping cleared automatically (one-to-one mapping)

**Persistence:** Mappings saved to JSON in platform-specific app data folder:

- Windows: `%APPDATA%/fp/AwesomeLooper/MidiMapping.json`
- macOS: `~/Library/Application Support/fp/AwesomeLooper/MidiMapping.json`
- Linux: `~/.config/fp/AwesomeLooper/MidiMapping.json`

### Technical Details

#### Thread Safety

- **Lock-free FIFOs** for command/event queues (no mutexes in audio thread)
- **Atomic operations** for simple state flags
- **Triple buffering** for bulk data transfer (waveforms)
- **Message thread dispatching** for UI updates (120Hz timer)

#### Real-Time Constraints

- No allocations in `processBlock()`
- No file I/O in audio thread
- Bounded processing time (state machine guarantees)
- Pre-allocated scratch buffers
- SIMD vector operations via JUCE's `FloatVectorOperations`

#### Performance Optimizations

- Fast path for 1.0x playback (zero processing overhead)
- Waveform caching at UI resolution (not full resolution)
- Background thread for waveform generation
- VBlank-synchronized UI updates (not timer-based polling)
- Conditional state updates (only repaint when changed)

## Building

Requires:

- JUCE 8.x
- C++17 compiler
- SoundTouch library (included via JUCE modules)
- nlohmann/json (for MIDI mapping persistence)

Build as:

- Standalone application (custom JUCE standalone wrapper)
- VST3/AU plugin (standard JUCE plugin formats)

## Run standalone

App is not signed so, to run mac standalone you need to open a terminal and run xattr -d
com.apple.quarantine /Applications/Looper.app
