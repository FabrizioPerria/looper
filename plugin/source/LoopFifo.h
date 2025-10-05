#pragma once
#include <JuceHeader.h>

class LoopFifo
{
public:
    LoopFifo()
    {
        bufferSize = 0;
        musicalLength = 0;
        writePos = 0;
        readPos = 0;
        shouldWrapAround = true;
    }

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

    void setWrapAround (bool shouldWrap)
    {
        shouldWrapAround = shouldWrap;
    }

    void prepareToWrite (int numToWrite, int& start1, int& size1, int& start2, int& size2)
    {
        start1 = writePos;
        int remaining = musicalLength - writePos;

        size1 = std::min (numToWrite, remaining);
        start2 = 0;
        size2 = shouldWrapAround ? std::max (0, numToWrite - remaining) : 0;
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

        size1 = std::min (numToRead, remaining);
        start2 = 0;
        size2 = shouldWrapAround ? std::max (0, numToRead - remaining) : 0;
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
    int bufferSize;
    int musicalLength; // current loop length
    int writePos;
    int readPos;
    bool shouldWrapAround { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFifo)
};
