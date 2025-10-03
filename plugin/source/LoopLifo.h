#pragma once
#include <JuceHeader.h>

class LoopLifo
{
public:
    LoopLifo() = default;
    ~LoopLifo() = default;

    void prepareToPlay (int totalSize)
    {
        capacity = totalSize;
        clear();
    }

    void clear()
    {
        writePos = 0;
        activeLayers = 0;
    }

    // Prepare to push 1 layer
    void prepareToWrite (int numToWrite, int& start1, int& size1, int& start2, int& size2)
    {
        jassert (numToWrite == 1); // undo stack only pushes one layer at a time
        start1 = writePos;
        size1 = 1;
        start2 = 0;
        size2 = 0;
    }

    void finishedWrite (int numWritten, bool /*overdub*/)
    {
        jassert (numWritten == 1);
        writePos = (writePos + 1) % capacity;
        activeLayers = std::min (activeLayers + 1, capacity);
    }

    // Prepare to pop 1 layer
    void prepareToRead (int numToRead, int& start1, int& size1, int& start2, int& size2)
    {
        jassert (numToRead == 1); // only pop one layer at a time
        if (activeLayers == 0)
        {
            start1 = size1 = start2 = size2 = 0;
            return;
        }

        start1 = (writePos - 1 + capacity) % capacity;
        size1 = 1;
        start2 = 0;
        size2 = 0;
    }

    void finishedRead (int numRead, bool /*overdub*/)
    {
        jassert (numRead == 1);
        if (activeLayers > 0)
        {
            writePos = (writePos - 1 + capacity) % capacity;
            activeLayers--;
        }
    }

    int getWritePos() const
    {
        return writePos;
    }

    int getActiveLayers() const
    {
        return activeLayers;
    }
    int getCapacity() const
    {
        return capacity;
    }

private:
    int capacity = 0;     // total slots
    int writePos = 0;     // next slot to push
    int activeLayers = 0; // number of valid layers
                          //
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopLifo)
};
