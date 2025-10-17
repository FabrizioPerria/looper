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
        readPos = 0.0; // Changed to double
        shouldWrapAround = true;
        playbackRate = 1.0; // Added
    }

    void prepareToPlay (int totalSize)
    {
        PERFETTO_FUNCTION();
        bufferSize = totalSize;
        musicalLength = totalSize;
        writePos = 0;
        readPos = 0.0;      // Changed to double
        playbackRate = 1.0; // Added
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

    // NEW: Set playback rate (positive = forward, negative = reverse)
    void setPlaybackRate (double rate)
    {
        PERFETTO_FUNCTION();
        playbackRate = rate;
    }

    double getPlaybackRate() const
    {
        PERFETTO_FUNCTION();
        return playbackRate;
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
        if (overdub) writePos = (int) readPos; // Cast to int
    }

    void prepareToRead (int numToRead, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        start1 = (int) readPos; // Cast to int

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
        // Advance by playback rate
        readPos += playbackRate * numRead;

        // Wrap around
        if (musicalLength > 0)
        {
            while (readPos < 0.0)
                readPos += musicalLength;
            while (readPos >= musicalLength)
                readPos -= musicalLength;
        }

        if (overdub) writePos = (int) readPos;
    }

    int getWritePos() const
    {
        PERFETTO_FUNCTION();
        return writePos;
    }

    int getReadPos() const
    {
        PERFETTO_FUNCTION();
        return (int) readPos; // Return int for UI
    }

    // NEW: Get exact fractional position
    double getExactReadPos() const
    {
        PERFETTO_FUNCTION();
        return readPos;
    }

private:
    int bufferSize;
    int musicalLength;
    int writePos;
    double readPos; // Changed from int
    bool shouldWrapAround { true };
    double playbackRate { 1.0 }; // NEW

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFifo)
};
