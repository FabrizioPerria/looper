#pragma once
#include <array>
#include <cstdint>
#include <string_view>

enum class LooperState : uint8_t
{
    Idle = 0,
    Stopped,
    Playing,
    Recording,
    Overdubbing,
    PendingTrackChange,
    Transitioning,
    COUNT
};

namespace StateConfig
{
constexpr size_t NUM_STATES = static_cast<size_t>(LooperState::COUNT);

constexpr int TRANSITIONS[NUM_STATES] =
    {
        [static_cast<size_t>(LooperState::Idle)] =
            (1u << static_cast<uint32_t>(LooperState::Recording)) |
            (1u << static_cast<uint32_t>(LooperState::Playing)),

        [static_cast<size_t>(LooperState::Stopped)] =
            (1u << static_cast<uint32_t>(LooperState::Playing)) |
            (1u << static_cast<uint32_t>(LooperState::Recording)) |
            (1u << static_cast<uint32_t>(LooperState::Overdubbing)) |
            (1u << static_cast<uint32_t>(LooperState::Idle)),

        [static_cast<size_t>(LooperState::Playing)] =
            (1u << static_cast<uint32_t>(LooperState::Stopped)) |
            (1u << static_cast<uint32_t>(LooperState::Overdubbing)) |
            (1u << static_cast<uint32_t>(LooperState::PendingTrackChange)),

        [static_cast<size_t>(LooperState::Recording)] =
            (1u << static_cast<uint32_t>(LooperState::Playing)) |
            (1u << static_cast<uint32_t>(LooperState::Stopped)) |
            (1u << static_cast<uint32_t>(LooperState::Idle)) |
            (1u << static_cast<uint32_t>(LooperState::Overdubbing)),

        [static_cast<size_t>(LooperState::Overdubbing)] =
            (1u << static_cast<uint32_t>(LooperState::Playing)) |
            (1u << static_cast<uint32_t>(LooperState::Stopped)),

        [static_cast<size_t>(LooperState::PendingTrackChange)] =
            (1u << static_cast<uint32_t>(LooperState::Transitioning)) |
            (1u << static_cast<uint32_t>(LooperState::Playing)) |
            (1u << static_cast<uint32_t>(LooperState::Stopped)),

        [static_cast<size_t>(LooperState::Transitioning)] =
            (1u << static_cast<uint32_t>(LooperState::Playing)) |
            (1u << static_cast<uint32_t>(LooperState::Stopped)) |
            (1u << static_cast<uint32_t>(LooperState::Idle))};

// State properties as bitfields
struct StateFlags
{
    uint8_t isRecording : 1;
    uint8_t isPlaying : 1;
    uint8_t needsContent : 1;
    uint8_t allowsUndo : 1;
    uint8_t processesAudio : 1;
};

constexpr StateFlags FLAGS[NUM_STATES] =
    {
        [static_cast<size_t>(LooperState::Idle)] = StateFlags{0, 0, 0, 1, 0},
        [static_cast<size_t>(LooperState::Stopped)] = StateFlags{0, 0, 1, 1, 1},
        [static_cast<size_t>(LooperState::Playing)] = StateFlags{0, 1, 1, 0, 1},
        [static_cast<size_t>(LooperState::Recording)] = StateFlags{1, 0, 0, 0, 1},
        [static_cast<size_t>(LooperState::Overdubbing)] = StateFlags{1, 1, 1, 0, 1},
        [static_cast<size_t>(LooperState::PendingTrackChange)] = StateFlags{0, 1, 1, 1, 1},
        [static_cast<size_t>(LooperState::Transitioning)] = StateFlags{0, 0, 0, 0, 1}};

constexpr const char* NAMES[NUM_STATES] =
    {
        [static_cast<size_t>(LooperState::Idle)] = "Idle",
        [static_cast<size_t>(LooperState::Stopped)] = "Stopped",
        [static_cast<size_t>(LooperState::Playing)] = "Playing",
        [static_cast<size_t>(LooperState::Recording)] = "Recording",
        [static_cast<size_t>(LooperState::Overdubbing)] = "Overdubbing",
        [static_cast<size_t>(LooperState::PendingTrackChange)] = "PendingTrackChange",
        [static_cast<size_t>(LooperState::Transitioning)] = "Transitioning"};

constexpr bool canTransition(LooperState from, LooperState to)
{
    const uint32_t mask = TRANSITIONS[static_cast<size_t>(from)];
    const uint32_t bit = 1u << static_cast<uint32_t>(to);
    return (mask & bit) != 0;
}

constexpr bool isRecording(LooperState s)
{
    return FLAGS[static_cast<size_t>(s)].isRecording;
}

constexpr bool isPlaying(LooperState s)
{
    return FLAGS[static_cast<size_t>(s)].isPlaying;
}

constexpr bool needsContent(LooperState s)
{
    return FLAGS[static_cast<size_t>(s)].needsContent;
}

constexpr bool allowsUndo(LooperState s)
{
    return FLAGS[static_cast<size_t>(s)].allowsUndo;
}

constexpr bool processesAudio(LooperState s)
{
    return FLAGS[static_cast<size_t>(s)].processesAudio;
}

constexpr const char* name(LooperState s)
{
    return NAMES[static_cast<size_t>(s)];
}
} // namespace StateConfig
