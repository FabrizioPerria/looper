#pragma once
#include <JuceHeader.h>

class LoopFifo
{
public:
    LoopFifo() = default;
    ~LoopFifo() = default;

    void prepareToPlay (int totalSize)
    {
        bufferSize = totalSize;
        musicalLength = totalSize;
        writePos = 0;
        readPos = 0;
    }

    void clear()
    {
        prepareToPlay (0);
    }

    void setMusicalLength (int length)
    {
        jassert (length <= bufferSize);
        musicalLength = length;
    }

    int getMusicalLength() const
    {
        return musicalLength;
    }

    void prepareToWrite (int numToWrite, int& start1, int& size1, int& start2, int& size2)
    {
        start1 = writePos;
        int remaining = musicalLength - writePos;

        if (numToWrite <= remaining)
        {
            size1 = numToWrite;
            start2 = 0;
            size2 = 0;
        }
        else
        {
            size1 = remaining;
            size2 = numToWrite - remaining;
            start2 = 0;
        }
    }

    void finishedWrite (int numWritten, bool overdub)
    {
        writePos = (writePos + numWritten) % musicalLength;
        if (overdub) writePos = readPos;
    }

    // Prepare to read N samples with wraparound
    void prepareToRead (int numToRead, int& start1, int& size1, int& start2, int& size2)
    {
        start1 = readPos;
        int remaining = musicalLength - readPos;

        if (numToRead <= remaining)
        {
            size1 = numToRead;
            start2 = 0;
            size2 = 0;
        }
        else
        {
            size1 = remaining;
            size2 = numToRead - remaining;
            start2 = 0;
        }
    }

    void finishedRead (int numRead, bool overdub)
    {
        readPos = (readPos + numRead) % musicalLength;
        if (overdub) writePos = readPos;
    }

    int getWritePos() const
    {
        return writePos;
    }
    int getReadPos() const
    {
        return readPos;
    }

private:
    int bufferSize = 0;
    int musicalLength = 0; // current loop length
    int writePos = 0;
    int readPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFifo)
};
