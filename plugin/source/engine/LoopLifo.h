#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class LoopLifo
{
public:
    LoopLifo() : capacity (0), slotToPush (0), activeLayers (0) {}

    void prepareToPlay (int totalSize)
    {
        capacity = totalSize;
        clear();
    }

    void clear() { slotToPush = activeLayers = 0; }

    // Prepare to push 1 layer
    void prepareToWrite (int /*numToWrite*/, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        jassert (numToWrite == 1); // undo stack only pushes one layer at a time
        start1 = slotToPush;
        size1 = 1;
        start2 = 0;
        size2 = 0;
    }

    void finishedWrite (int /*numWritten*/, bool /*overdub*/)
    {
        PERFETTO_FUNCTION();
        jassert (numWritten == 1);
        slotToPush = (slotToPush + 1) % capacity;
        activeLayers = std::min (activeLayers + 1, capacity);
    }

    // Prepare to pop 1 layer
    void prepareToRead (int /*numToRead*/, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        jassert (numToRead == 1); // only pop one layer at a time
        if (activeLayers == 0)
        {
            start1 = size1 = start2 = size2 = 0;
            return;
        }

        start1 = (slotToPush - 1 + capacity) % capacity;
        size1 = 1;
        start2 = 0;
        size2 = 0;
    }

    void finishedRead (int /*numRead*/, bool /*overdub*/)
    {
        PERFETTO_FUNCTION();
        jassert (numRead == 1);
        if (activeLayers > 0)
        {
            slotToPush = (slotToPush - 1 + capacity) % capacity;
            activeLayers--;
        }
    }

    int getSlotToPush() const { return slotToPush; }

    int getActiveLayers() const { return activeLayers; }

    int getCapacity() const { return capacity; }

    int getNextLayerIndex() const
    {
        if (activeLayers == 0) return -1;
        return (slotToPush - 1 + capacity) % capacity;
    }

private:
    int capacity;
    int slotToPush;
    int activeLayers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopLifo)
};
