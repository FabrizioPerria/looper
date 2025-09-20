#include <gtest/gtest.h>

#include "LoopTrack.h"

namespace audio_plugin_test
{
TEST (LoopTrackPrepare, PreallocatesCorrectSize)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 120;
    const int maxBlock = 512;
    const int numChannels = 4;
    // expected size round up to multiple of block size
    // 44100 samples at 44100 Hz for 120 sec duration, block size 512 -> 22579200 samples
    const int bufferSamples = 22579200;

    track.prepareToPlay (sr, maxBlock, maxSeconds, numChannels);

    EXPECT_EQ (track.getBuffer().getNumChannels(), numChannels);
    EXPECT_EQ (track.getBuffer().getNumSamples(), bufferSamples);

    EXPECT_EQ (track.getUndoBuffer().getNumChannels(), numChannels);
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), bufferSamples);
}

TEST (LoopTrackPrepare, BuffersClearedToZero)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 512, 10, 2);

    const auto buffer = track.getBuffer();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* ptr = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (ptr[i], 0.0f);
        }
    }

    const auto undoBuffer = track.getUndoBuffer();
    for (int ch = 0; ch < undoBuffer.getNumChannels(); ++ch)
    {
        auto* ptr = undoBuffer.getReadPointer (ch);
        for (int i = 0; i < undoBuffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (ptr[i], 0.0f);
        }
    }
}

TEST (LoopTrackPrepare, StateReset)
{
    LoopTrack track;
    track.setWritePos (1000);
    track.setLength (5000);
    track.prepareToPlay (44100.0, 512, 10, 2);

    EXPECT_EQ (track.getWritePos(), 0);
    EXPECT_EQ (track.getLength(), 0);
}

TEST (LoopTrackPrepare, ZeroMaxSecondsAllocatesAtLeastOneBlock)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxBlock = 512;

    track.prepareToPlay (sr, 0, maxBlock, 2); // zero seconds

    EXPECT_EQ (track.getBuffer().getNumSamples(), maxBlock);
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), maxBlock);
}

TEST (LoopTrackPrepare, FractionalSampleRateRoundsUp)
{
    LoopTrack track;
    double sr = 48000.1;
    int seconds = 1;
    int maxBlock = 512;

    track.prepareToPlay (sr, seconds, maxBlock, 2);

    EXPECT_GT (track.getBuffer().getNumSamples(), (int) sr * seconds);
}

TEST (LoopTrackPrepare, LargeDurationDoesNotOverflow)
{
    LoopTrack track;
    double sr = 44100.0;
    int seconds = 60 * 60; // one hour
    int maxBlock = 512;

    track.prepareToPlay (sr, seconds, maxBlock, 2);

    EXPECT_GT (track.getBuffer().getNumSamples(), 0);
    EXPECT_LT (track.getBuffer().getNumSamples(), INT_MAX);
}

TEST (LoopTrackPrepare, ReprepareWithLargerBlockGrowsBuffer)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 10, 512, 2);
    int firstSize = track.getBuffer().getNumSamples();

    // simulate host requesting a bigger block
    track.prepareToPlay (44100.0, 10, 1024, 2);
    int secondSize = track.getBuffer().getNumSamples();

    EXPECT_GE (secondSize, firstSize);
}

TEST (LoopTrackPrepare, ReprepareWithSmallerBlockKeepsBufferSize)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 10, 512, 2);
    int firstSize = track.getBuffer().getNumSamples();

    // simulate host requesting a smaller block
    track.prepareToPlay (44100.0, 10, 256, 2);
    int secondSize = track.getBuffer().getNumSamples();

    EXPECT_EQ (secondSize, firstSize);
}

TEST (LoopTrackPrepare, UndoBufferMatchesMainBuffer)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 10, 512, 10);

    EXPECT_EQ (track.getUndoBuffer().getNumChannels(), track.getBuffer().getNumChannels());
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), track.getBuffer().getNumSamples());
}

} // namespace audio_plugin_test
