#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class LoopFifo
{
public:
    LoopFifo() : bufferSize (0), musicalLength (0), writePos (0), readPos (0.0), shouldWrapAround (true), playbackRate (1.0) {}

    void prepareToPlay (int totalSize)
    {
        PERFETTO_FUNCTION();
        jassert (totalSize > 0);

        bufferSize = musicalLength = totalSize;
        writePos = 0;
        readPos = 0.0;
        playbackRate = 1.0;
    }

    void clear() { prepareToPlay (0); }

    void setMusicalLength (int length) { musicalLength = std::clamp (length, 0, bufferSize); }
    int getMusicalLength() const { return musicalLength; }

    void setWrapAround (bool shouldWrap) { shouldWrapAround = shouldWrap; }
    bool getWrapAround() const { return shouldWrapAround; }

    void setPlaybackRate (double speed, int direction) { playbackRate = speed * direction; }
    double getPlaybackRate() const { return playbackRate; }

    void prepareToWrite (int numToWrite, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        jassert (numToWrite > 0);
        start1 = writePos;
        int remaining = musicalLength - writePos;

        size1 = std::min (numToWrite, remaining);
        start2 = 0;
        size2 = shouldWrapAround ? std::max (0, numToWrite - remaining) : 0;
    }

    void finishedWrite (int numWritten, bool overdub)
    {
        PERFETTO_FUNCTION();
        jassert (numWritten > 0);
        writePos = (writePos + numWritten) % musicalLength;
        if (overdub) writePos = (int) readPos;
    }

    void prepareToRead (int numToRead, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        jassert (numToRead > 0);
        start1 = (int) readPos;

        int samplesToTraverse = numToRead;
        if (std::abs (playbackRate - 1.0) > 0.001) // Not normal speed
        {
            // Calculate samples we might traverse (for variable speed)
            samplesToTraverse = (int) (numToRead * std::abs (playbackRate)) + 10;
        }

        int remaining = musicalLength - start1;

        size1 = std::min (samplesToTraverse, remaining);
        start2 = 0;
        size2 = std::max (0, samplesToTraverse - remaining);
    }

    void finishedRead (int numRead, bool overdub)
    {
        PERFETTO_FUNCTION();
        jassert (numRead > 0);
        readPos += playbackRate * numRead;

        if (musicalLength > 0)
        {
            readPos = std::fmod (readPos, musicalLength);
            if (readPos < 0) readPos += musicalLength;
        }

        if (overdub) writePos = (int) readPos;
    }

    int getWritePos() const { return writePos; }
    int getReadPos() const { return (int) readPos; }

    double getExactReadPos() const { return readPos; }

private:
    int bufferSize;
    int musicalLength;
    int writePos;
    double readPos;
    bool shouldWrapAround;
    double playbackRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFifo)
};
