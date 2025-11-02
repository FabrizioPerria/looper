#pragma once
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class LoopFifo
{
public:
    LoopFifo() : bufferSize (0), musicalLength (0), writePos (0), readPos (0.0), shouldWrapAround (true) /*, playbackRate (1.0)*/ {}

    void prepareToPlay (int totalSize)
    {
        PERFETTO_FUNCTION();

        bufferSize = musicalLength = totalSize;
        writePos = 0;
        readPos = 0.0;
    }

    void clear() { prepareToPlay (0); }

    void setMusicalLength (int length) { musicalLength = std::clamp (length, 0, bufferSize); }
    int getMusicalLength() const { return musicalLength; }

    void setWrapAround (bool shouldWrap) { shouldWrapAround = shouldWrap; }
    bool getWrapAround() const { return shouldWrapAround; }

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

    void finishedWrite (int numWritten, bool overdub, bool syncWithRead)
    {
        PERFETTO_FUNCTION();
        if (regionEnabled)
        {
            writePos += numWritten;
            if (writePos >= regionEnd) writePos = regionStart + (writePos - regionEnd);
        }
        else
        {
            writePos = (writePos + numWritten) % musicalLength;
        }

        if (overdub && syncWithRead) writePos = (int) readPos;
    }

    void setLoopRegion (int start, int end)
    {
        regionStart = start;
        regionEnd = end;
        regionEnabled = true;
    }

    void clearLoopRegion()
    {
        regionEnabled = false;
        regionStart = 0;
        regionEnd = 0;
    }

    void fromScratch()
    {
        PERFETTO_FUNCTION();
        if (regionEnabled)
        {
            writePos = regionStart;
            readPos = (double) regionStart;
        }
        else
        {
            writePos = 0;
            readPos = 0.0;
        }
    }

    void prepareToRead (int numToRead, int& start1, int& size1, int& start2, int& size2)
    {
        PERFETTO_FUNCTION();
        jassert (numToRead > 0);
        start1 = (int) readPos;

        int remaining = musicalLength - start1;

        size1 = std::min (numToRead, remaining);
        start2 = 0;
        size2 = std::max (0, numToRead - remaining);
    }

    int getReverseReadIndex (int offset) const
    {
        PERFETTO_FUNCTION();
        int idx = (int) readPos - offset;
        return ((idx % musicalLength) + musicalLength) % musicalLength; // Handles negative indices in one step
    }

    void finishedRead (int numRead, float playbackRate, bool overdub)
    {
        PERFETTO_FUNCTION();
        lastPlaybackRate = playbackRate;
        readPos += playbackRate * (float) numRead;

        if (regionEnabled)
        {
            if (readPos >= regionEnd)
                readPos = (double) regionStart + std::fmod (readPos - regionStart, regionEnd - regionStart);
            else if (readPos < regionStart)
                readPos = (double) regionEnd - std::fmod (regionEnd - readPos, regionEnd - regionStart);
        }
        else if (musicalLength > 0)
        {
            readPos = std::fmod (readPos, musicalLength);
            if (readPos < 0) readPos += musicalLength;
        }

        if (overdub) writePos = (int) readPos;
    }

    int getWritePos() const { return writePos; }
    int getReadPos() const { return (int) readPos; }

    double getExactReadPos() const { return readPos; }

    float getLastPlaybackRate() const { return lastPlaybackRate; }

private:
    float lastPlaybackRate = 1.0f;
    int bufferSize;
    int musicalLength;
    int writePos;
    double readPos;
    bool shouldWrapAround;

    bool regionEnabled = false;
    int regionStart = 0;
    int regionEnd = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopFifo)
};
