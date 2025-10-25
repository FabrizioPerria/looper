#pragma once
#include "MidiCommandConfig.h"

// Forward declare only
class LooperEngine;

using CommandFunc = void (*) (LooperEngine&, int trackIndex);

extern const CommandFunc COMMAND_EXECUTORS[static_cast<size_t> (MidiCommandId::COUNT)];

class MidiCommandDispatcher
{
public:
    static void dispatch (MidiCommandId commandId, LooperEngine& engine, int trackIndex);
};
