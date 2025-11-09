#pragma once
//**************************************************************
// General Constants
//**************************************************************
constexpr int NUM_TRACKS = 4;
constexpr int LEFT_CHANNEL = 0;
constexpr int RIGHT_CHANNEL = 1;

constexpr int SAVE_TRACK_BITS_PER_SAMPLE = 16;
constexpr int LOOP_MAX_SECONDS_HARD_LIMIT = 10 * 60; // 10 minutes
constexpr int MAX_UNDO_LAYERS = 5;

constexpr int DEFAULT_ACTIVE_TRACK_INDEX = -1;
constexpr float MIN_PLAYBACK_SPEED = 0.5f;
constexpr float MAX_PLAYBACK_SPEED = 2.0f;

constexpr float MIN_OVERDUB_GAIN = 0.0f;
constexpr float MAX_OVERDUB_GAIN = 2.0f;
constexpr float MIN_BASE_GAIN = 0.0f;
constexpr float MAX_BASE_GAIN = 2.0f;
constexpr float MIN_PLAYBACK_PITCH_SEMITONES = -2.0f;
constexpr float MAX_PLAYBACK_PITCH_SEMITONES = 2.0f;

constexpr int MAX_MIDI_NOTES = 128;
constexpr int MAX_CC_NUMBERS = 128;

constexpr float CROSSFADE_DEFAULT_LENGTH_SECONDS = 0.01f;
constexpr float MIN_TRACK_VOLUME = 0.0f;
constexpr float MAX_TRACK_VOLUME = 1.0f;
constexpr float NORMALIZE_TARGET_LEVEL = 0.9f;
constexpr float TRACK_DEFAULT_VOLUME = 1.0f;
constexpr float BASE_DEFAULT_GAIN = 1.0f;
constexpr float OVERDUB_DEFAULT_GAIN = 1.0f;
constexpr bool DEFAULT_SOLO_STATE = false;
constexpr bool DEFAULT_MUTE_STATE = false;

constexpr char COMPANY_NAME[] = "fp";
constexpr char PLUGIN_NAME[] = "AwesomeLooper";
constexpr char MIDI_MAPPING_FILE_NAME[] = "MidiMapping.json";

//**************************************************************
// Message Bus Constants
//**************************************************************
constexpr int MESSAGE_BUS_FIFO_SIZE = 1024;

//**************************************************************
// Granular Freeze Constants
//**************************************************************
constexpr float FREEZE_BUFFER_DURATION_SECONDS = 0.5f;
constexpr int MAX_GRAINS = 64;
constexpr int GRAIN_LENGTH = 16384;
constexpr int GRAIN_SPACING = 512;
constexpr int ENVELOPE_WINDOW_SIZE = 2048;

constexpr int MOD_TABLE_SIZE = 1024;
constexpr int MOD_TABLE_MASK = MOD_TABLE_SIZE - 1;

constexpr float MOD_RATE = 0.04f;
constexpr float PITCH_MOD_DEPTH = 0.005f;
constexpr float AMP_MOD_DEPTH = 0.01f;

constexpr float MIN_AMP_MOD = 0.7f;
constexpr float MAX_AMP_MOD = 1.0f;

constexpr float DEFAULT_FREEZE_AMPLITUDE = 0.8f;

//**************************************************************
// Metronome Constants
//**************************************************************
constexpr float METRONOME_MIN_BPM = 30;
constexpr float METRONOME_MAX_BPM = 300;
constexpr int METRONOME_DEFAULT_BPM = 120;
constexpr int METRONOME_TAP_RECENT_THRESHOLD_MS = 500; // Consider tap "recent" if within last 500ms
constexpr int METRONOME_TAP_TIMEOUT_MS = 3000;         // Reset after 3 seconds of no taps
constexpr int METRONOME_MIN_TAP_INTERVAL_MS = 100;     // Debounce - ignore taps closer than 100ms
constexpr float METRONOME_DEFAULT_VOLUME = 0.8f;
constexpr bool METRONOME_DEFAULT_ENABLED = false;

constexpr float METRONOME_STRONG_BEAT_CLICK_LENTH_SECONDS = 0.01f; // 10ms
constexpr float METRONOME_STRONG_BEAT_FREQUENCY = 1200.0f;         // 1200 Hz
constexpr float METRONOME_STRONG_BEAT_ENVELOPE_DECAY = 200.0f;     // Decay rate for strong beat click
constexpr float METRONOME_STRONG_BEAT_GAIN = 2.0f;
//
constexpr float METRONOME_WEAK_BEAT_CLICK_LENTH_SECONDS = 0.008f; // 8ms
constexpr float METRONOME_WEAK_BEAT_FREQUENCY = 800.0f;
constexpr float METRONOME_WEAK_BEAT_ENVELOPE_DECAY = 250.0f; // Decay rate for weak beat click
constexpr float METRONOME_WEAK_BEAT_GAIN = 1.5f;
constexpr int METRONOME_DEFAULT_TIME_SIGNATURE_NUMERATOR = 4;
constexpr int METRONOME_DEFAULT_TIME_SIGNATURE_DENOMINATOR = 4;

//**************************************************************
// Level Meter Constants
//**************************************************************
constexpr float DECAY_FACTOR = 0.99f; // Decay factor for smoothing
constexpr float DEFAULT_INPUT_GAIN = 1.0f;
constexpr float DEFAULT_OUTPUT_GAIN = 1.0f;
