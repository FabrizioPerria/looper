#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class LoopFifo
{
public:
    LoopFifo()
    {
        PERFETTO_FUNCTION();
        bufferSize = 0;
        musicalLength = 0;
        writePos = 0;
        readPos = 0;
        shouldWrapAround = true;
    }

    void prepareToPlay (int totalSize)
    {
        PERFETTO_FUNCTION();
        bufferSize = totalSize;
        musicalLength = totalSize;
        writePos = 0;
        readPos = 0;
    }

    void clear()
    {
        PERFETTO_FUNCTION();
        prepareToPlay (0);
    }

    void setMusicalLength (int length)
    {
        PERFETTO_FUNCTION();
        jassert (length <= bufferSize);
        musicalLength = length;
    }

    int getMusicalLength() const
    {
        PERFETTO_FUNCTION();
        return musicalLength;
    }

    void setWrapAround (bool shouldWrap)
    {
        PERFETTO_FUNCTION();
        shouldWrapAround = shouldWrap;
    }

    bool getWrapAround() const
    {
        PERFETTO_FUNCTION();
        return shouldWrapAround;
    }

    void prepareToWrite (int numToWrite, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        start1 = writePos;
        int remaining = musicalLength - writePos;

        size1 = std::min (numToWrite, remaining);
        start2 = 0;
        size2 = shouldWrapAround ? std::max (0, numToWrite - remaining) : 0;
    }

    void finishedWrite (int numWritten, bool overdub)
    {
        PERFETTO_FUNCTION();
        writePos = (writePos + numWritten) % musicalLength;
        if (overdub) writePos = readPos;
    }

    // Prepare to read N samples with wraparound
    void prepareToRead (int numToRead, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        start1 = readPos;
        int remaining = musicalLength - readPos;

        size1 = std::min (numToRead, remaining);
        start2 = 0;
        size2 = std::max (0, numToRead - remaining);
    }

    void finishedRead (int numRead, bool overdub)
    {
        PERFETTO_FUNCTION();
        readPos = (readPos + numRead) % musicalLength;
        if (overdub) writePos = readPos;
    }

    int getWritePos() const
    {
        PERFETTO_FUNCTION();
        return writePos;
    }
    int getReadPos() const
    {
        PERFETTO_FUNCTION();
        return readPos;
    }

private:
    int bufferSize;
    int musicalLength; // current loop length
    int writePos;
    int readPos;
    bool shouldWrapAround { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFifo)
};
