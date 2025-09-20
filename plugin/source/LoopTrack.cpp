#include "LoopTrack.h"

LoopTrack::LoopTrack()
{
}

LoopTrack::~LoopTrack()
{
}

void LoopTrack::prepareToPlay (double sr, int maxSeconds, int maxBlockSize, int numChannels)
{
    sampleRate = sr;
    int totalSamples = (int) (sr * maxSeconds);
    int bufferSamples = ((totalSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;
    buffer.setSize (numChannels, bufferSamples, false, true, true);
    buffer.clear();

    undoBuffer.setSize (numChannels, bufferSamples, false, true, true);
    undoBuffer.clear();

    writePos = 0;
    length = 0;
}
