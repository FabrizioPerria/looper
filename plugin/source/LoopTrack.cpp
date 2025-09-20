#include "LoopTrack.h"
#include <algorithm>
#include <cassert>

LoopTrack::LoopTrack()
{
}

LoopTrack::~LoopTrack()
{
}

void LoopTrack::prepareToPlay (double sr, uint maxSeconds, uint maxBlockSize, uint numChannels)
{
    jassert (sr > 0);
    sampleRate = sr;
    uint totalSamples = std::max ((uint) sr * maxSeconds, 1u); // at least 1 block will be allocated
    uint bufferSamples = ((totalSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;
    if (bufferSamples > buffer.getNumSamples())
    {
        buffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }
    buffer.clear();

    if (bufferSamples > undoBuffer.getNumSamples())
    {
        undoBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }
    undoBuffer.clear();

    writePos = 0;
    length = 0;
}
