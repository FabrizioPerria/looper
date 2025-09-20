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

    EXPECT_EQ (track.getAudioBuffer().getNumChannels(), numChannels);
    EXPECT_EQ (track.getAudioBuffer().getNumSamples(), bufferSamples);

    EXPECT_EQ (track.getUndoBuffer().getNumChannels(), numChannels);
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), bufferSamples);
}

TEST (LoopTrackPrepare, BuffersClearedToZero)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 512, 10, 2);

    const auto buffer = track.getAudioBuffer();
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

    EXPECT_EQ (track.getAudioBuffer().getNumSamples(), maxBlock);
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), maxBlock);
}

TEST (LoopTrackPrepare, FractionalSampleRateRoundsUp)
{
    LoopTrack track;
    double sr = 48000.1;
    int seconds = 1;
    int maxBlock = 512;

    track.prepareToPlay (sr, seconds, maxBlock, 2);

    EXPECT_GT (track.getAudioBuffer().getNumSamples(), (int) sr * seconds);
}

TEST (LoopTrackPrepare, LargeDurationDoesNotOverflow)
{
    LoopTrack track;
    double sr = 44100.0;
    int seconds = 60 * 60; // one hour
    int maxBlock = 512;

    track.prepareToPlay (sr, seconds, maxBlock, 2);

    EXPECT_GT (track.getAudioBuffer().getNumSamples(), 0);
    EXPECT_LT (track.getAudioBuffer().getNumSamples(), INT_MAX);
}

TEST (LoopTrackPrepare, ReprepareWithLargerBlockGrowsBuffer)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 10, 512, 2);
    int firstSize = track.getAudioBuffer().getNumSamples();

    // simulate host requesting a bigger block
    track.prepareToPlay (44100.0, 10, 1024, 2);
    int secondSize = track.getAudioBuffer().getNumSamples();

    EXPECT_GE (secondSize, firstSize);
}

TEST (LoopTrackPrepare, ReprepareWithSmallerBlockKeepsBufferSize)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 10, 512, 2);
    int firstSize = track.getAudioBuffer().getNumSamples();

    // simulate host requesting a smaller block
    track.prepareToPlay (44100.0, 10, 256, 2);
    int secondSize = track.getAudioBuffer().getNumSamples();

    EXPECT_EQ (secondSize, firstSize);
}

TEST (LoopTrackPrepare, UndoBufferMatchesMainBuffer)
{
    LoopTrack track;
    track.prepareToPlay (44100.0, 10, 512, 10);

    EXPECT_EQ (track.getUndoBuffer().getNumChannels(), track.getAudioBuffer().getNumChannels());
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), track.getAudioBuffer().getNumSamples());
}

juce::AudioBuffer<float> createSineTestBuffer (int numChannels, int numSamples, double sr)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            writePtr[i] = std::sin (2.0 * M_PI * 440.0 * i / sr); // 440 Hz sine wave
        }
    }
    return buffer;
}

TEST (LoopTrackRecord, ProcessFullBlockCopiesInput)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 1;
    const int maxBlock = 4;
    const int numChannels = 1;

    track.prepareToPlay (sr, maxBlock, maxSeconds, numChannels);

    const int numSamples = 4;
    juce::AudioBuffer<float> input = createSineTestBuffer (numChannels, numSamples, sr);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    EXPECT_EQ (track.getWritePos(), numSamples);
    EXPECT_EQ (track.getLength(), numSamples);

    // process another block and check it appends correctly
    track.processRecord (input, numSamples);
    loopPtr = loopBuffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i + numSamples], readPtr[i]);
    }

    EXPECT_EQ (track.getWritePos(), numSamples * 2);
    EXPECT_EQ (track.getLength(), numSamples * 2);
}

TEST (LoopTrackRecord, ProcessPartialBlockCopiesInput)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 512;
    const int numChannels = 1;

    track.prepareToPlay (sr, maxBlock, maxSeconds, numChannels);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int numSamples = 200; // less than block size
    juce::AudioBuffer<float> input = createSineTestBuffer (numChannels, numSamples, sr);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    auto* readPtr2 = input.getReadPointer (0);

    track.processRecord (input, numSamples);
    loopPtr = loopBuffer.getReadPointer (0);

    // Check samples written before wrap
    for (int i = 0; i < 100; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[numSamples + i], readPtr2[i]);
    }

    EXPECT_EQ (track.getWritePos(), numSamples * 2);
    EXPECT_EQ (track.getLength(), numSamples * 2);
}

TEST (LoopTrackRecord, ProcessPartialBlockCopiesInputWrapAround)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 512;
    const int numChannels = 1;

    track.prepareToPlay (sr, maxBlock, maxSeconds, numChannels);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int leaveSamples = 100; // leave some space at end of buffer
    const int numSamples = bufferSamples - leaveSamples;

    juce::AudioBuffer<float> input = createSineTestBuffer (numChannels, numSamples, sr);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    EXPECT_EQ (track.getWritePos(), numSamples);
    EXPECT_EQ (track.getLength(), numSamples);

    // process another partial block that will wrap around
    juce::AudioBuffer<float> input2 = createSineTestBuffer (numChannels, numSamples, sr);
    auto* readPtr2 = input2.getReadPointer (0);

    track.processRecord (input2, numSamples);
    loopPtr = loopBuffer.getReadPointer (0);

    // Check samples written before wrap
    for (int i = 0; i < leaveSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[numSamples + i], readPtr2[i]);
    }

    readPtr = input.getReadPointer (0);
    // Check samples written after wrap. overdub of end and start of buffer
    for (int i = 0; i < leaveSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i] + readPtr2[leaveSamples + i]);
    }

    EXPECT_EQ (track.getWritePos(), numSamples - leaveSamples);
    EXPECT_EQ (track.getLength(), bufferSamples);
}

TEST (LoopTrackRecord, ProcessMultipleChannels)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 3;

    track.prepareToPlay (sr, maxBlock, maxSeconds, numChannels);

    const int numSamples = 512;
    juce::AudioBuffer<float> input = createSineTestBuffer (numChannels, numSamples, sr);
    auto* readPtrCh0 = input.getReadPointer (0);
    auto* readPtrCh1 = input.getReadPointer (1);
    auto* readPtrCh2 = input.getReadPointer (2);

    track.processRecord (input, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtrCh0 = loopBuffer.getReadPointer (0);
    auto* loopPtrCh1 = loopBuffer.getReadPointer (1);
    auto* loopPtrCh2 = loopBuffer.getReadPointer (2);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtrCh0[i], readPtrCh0[i]);
        EXPECT_FLOAT_EQ (loopPtrCh1[i], readPtrCh1[i]);
        EXPECT_FLOAT_EQ (loopPtrCh2[i], readPtrCh2[i]);
    }

    EXPECT_EQ (track.getWritePos(), numSamples);
    EXPECT_EQ (track.getLength(), numSamples);
}

} // namespace audio_plugin_test
