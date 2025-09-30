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
    const int undoLayers = 1;
    // expected size round up to multiple of block size
    // 441 samples at 441 KHz for 120 sec duration, block size 512 -> 5292032 samples
    const int bufferSamples = 5292032;

    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_TRUE (track.isPrepared());
    EXPECT_DOUBLE_EQ (track.getSampleRate(), sr);
    EXPECT_EQ (track.getAudioBuffer().getNumChannels(), numChannels);
    EXPECT_EQ (track.getAudioBuffer().getNumSamples(), bufferSamples);

    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), bufferSamples);
    EXPECT_EQ (track.getUndoBuffer().getNumChannels(), numChannels);
    EXPECT_EQ (track.getUndoBuffer().getNumLayers(), (size_t) undoLayers);
}

TEST (LoopTrackPrepare, BuffersClearedToZero)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const auto buffer = track.getAudioBuffer();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* ptr = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (ptr[i], 0.0f);
        }
    }

    const auto& undoBuffer = track.getUndoBuffer();
    for (int ch = 0; ch < undoBuffer.getNumChannels(); ++ch)
    {
        auto undoBufferContents = undoBuffer.getBuffers()[0];
        auto* ptr = undoBufferContents.getReadPointer (ch);
        for (int i = 0; i < undoBuffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (ptr[i], 0.0f);
        }
    }
}

TEST (LoopTrackPrepare, StateReset)
{
    LoopTrack track;
    track.setLength (5000);

    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_EQ (track.getLength(), 0);
}

TEST (LoopTrackPrepare, ZeroMaxSecondsDoesNotAllocateNorPrepare)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 0;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_FALSE (track.isPrepared());
}

TEST (LoopTrackPrepare, FractionalSampleRateRoundsUp)
{
    LoopTrack track;

    const double sr = 48000.1;
    const int maxSeconds = 1;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_GT (track.getAudioBuffer().getNumSamples(), (int) sr * maxSeconds);
}

TEST (LoopTrackPrepare, LargeDurationDoesNotOverflow)
{
    LoopTrack track;

    const double sr = 44100.0;
    const int maxSeconds = 60 * 60;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_GT (track.getAudioBuffer().getNumSamples(), 0);
    EXPECT_LT (track.getAudioBuffer().getNumSamples(), INT_MAX);
}

TEST (LoopTrackPrepare, ReprepareWithLargerBlockGrowsBuffer)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int firstSize = track.getAudioBuffer().getNumSamples();

    // simulate host requesting a bigger block
    maxBlock = 1024;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int secondSize = track.getAudioBuffer().getNumSamples();

    EXPECT_GE (secondSize, firstSize);
}

TEST (LoopTrackPrepare, PrepareWithInvalidSampleRateDoesNotPrepare)
{
    LoopTrack track;
    double sr = 0.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_FALSE (track.isPrepared());

    sr = -10.0;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    ASSERT_FALSE (track.isPrepared());
}

TEST (LoopTrackPrepare, ReprepareWithSmallerBlockKeepsBufferSize)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int firstSize = track.getAudioBuffer().getNumSamples();

    // simulate host requesting a smaller block
    maxBlock = 256;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    int secondSize = track.getAudioBuffer().getNumSamples();

    EXPECT_EQ (secondSize, firstSize);
}

TEST (LoopTrackPrepare, UndoBufferMatchesMainBuffer)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 10;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_EQ (track.getUndoBuffer().getNumChannels(), track.getAudioBuffer().getNumChannels());
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), track.getAudioBuffer().getNumSamples());
}

juce::AudioBuffer<float> createSquareTestBuffer (int numChannels, int numSamples, double sr, float frequency)
{
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* writePtr = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            writePtr[i] = (std::fmod ((i / sr) * frequency, 1.0) < 0.5) ? 1.0f : -1.0f;
        }
    }
    return buffer;
}

TEST (LoopTrackRecord, ProcessFullBlockCopiesInput)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 10;
    const int maxBlock = 4;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int numSamples = 4;
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    EXPECT_EQ (track.getLength(), 0);

    // process another block and check it appends correctly
    track.processRecord (input, numSamples);
    track.finalizeLayer();
    loopPtr = loopBuffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i + numSamples], readPtr[i]);
    }

    EXPECT_EQ (track.getLength(), numSamples * 2);
}

TEST (LoopTrackRecord, ProcessPartialBlockCopiesInput)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setCrossFadeLength (0);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int numSamples = 9; // less than block size
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
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
    track.finalizeLayer();
    loopPtr = loopBuffer.getReadPointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[numSamples + i], readPtr2[i]);
    }

    EXPECT_EQ (track.getLength(), numSamples * 2);
}

TEST (LoopTrackRecord, ProcessPartialBlockCopiesInputOverMaxBufferSize)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setCrossFadeLength (0);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int leaveSamples = 10; // leave some space at end of buffer
    const int numSamples = bufferSamples - leaveSamples;

    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    auto* readPtr = input.getReadPointer (0);

    track.processRecord (input, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    // process another partial block that will wrap around
    juce::AudioBuffer<float> input2 = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    auto* readPtr2 = input2.getReadPointer (0);

    track.processRecord (input2, numSamples);
    track.finalizeLayer();

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
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr[i]);
    }

    EXPECT_EQ (track.getLength(), bufferSamples);
}

TEST (LoopTrackRecord, ProcessMultipleChannels)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 3;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    const int numSamples = 12;
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    auto* readPtrCh0 = input.getReadPointer (0);
    auto* readPtrCh1 = input.getReadPointer (1);
    auto* readPtrCh2 = input.getReadPointer (2);

    track.processRecord (input, numSamples);
    track.finalizeLayer();

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

    EXPECT_EQ (track.getLength(), numSamples);
}

TEST (LoopTrackRecord, ZeroLengthInputDoesNothing)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    juce::AudioBuffer<float> input (numChannels, 0); // zero length buffer

    track.processRecord (input, 0);

    const auto& loopBuffer = track.getAudioBuffer();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* loopPtr = loopBuffer.getReadPointer (ch);
        for (int i = 0; i < loopBuffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (loopPtr[i], 0.0f);
        }
    }

    EXPECT_EQ (track.getLength(), 0);
}

TEST (LoopTrackRecord, OffsetOverdub)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 1;
    const int maxBlock = 4;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    juce::AudioBuffer<float> input1 = createSquareTestBuffer (numChannels, sr * maxSeconds, sr, 440.0f);
    track.processRecord (input1, sr * maxSeconds);
    track.finalizeLayer();

    const int numSamples = 2;
    track.processPlayback (input1, maxBlock); // advance readPosition position by one block
    juce::AudioBuffer<float> input2 = createSquareTestBuffer (numChannels, numSamples, sr, 880.0f);
    track.processRecord (input2, numSamples); // overdub at offset
    track.finalizeLayer();

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    auto* readPtr1 = input1.getReadPointer (0);
    auto* readPtr2 = input2.getReadPointer (0);
    int start = 0;
    int end = maxBlock;
    for (int i = start; i < end; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr1[i] * 0.5f);
    }
    start = end;
    end += numSamples;
    for (int i = start; i < end; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr1[i] + readPtr2[i - start]);
    }
    start = end;
    end = sr * maxSeconds;
    for (int i = start; i < end; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtr[i], readPtr1[i]);
    }

    EXPECT_EQ (track.getLength(), sr * maxSeconds);
}

TEST (LoopTrackOverdub, IntermittentOverdubOnlyAffectsActiveRecordingPeriods)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 1;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    track.setCrossFadeLength (0);

    // Create initial loop - 1 second of 440Hz sine
    const int loopLength = 4410; // 0.1 seconds
    juce::AudioBuffer<float> initialLoop = createSquareTestBuffer (numChannels, loopLength, sr, 440.0f);
    track.processRecord (initialLoop, loopLength);
    track.finalizeLayer();

    // Save copy of original loop for comparison
    juce::AudioBuffer<float> originalLoop (numChannels, loopLength);
    originalLoop.copyFrom (0, 0, track.getAudioBuffer(), 0, 0, loopLength);

    auto compareBuffers = [] (const juce::AudioBuffer<float>& buf1, const juce::AudioBuffer<float>& buf2, int start, int length)
    {
        auto* ptr1 = buf1.getReadPointer (0, start);
        auto* ptr2 = buf2.getReadPointer (0, start);
        for (int i = 0; i < length; ++i)
        {
            if (ptr1[i] != ptr2[i]) return false;
        }
        return true;
    };

    // Do an initial playback to set read position to zero
    track.processPlayback (initialLoop, loopLength);

    EXPECT_TRUE (compareBuffers (track.getAudioBuffer(), originalLoop, 0, loopLength));

    // Verify:
    // First third should match original
    // Create overdub material - 880Hz sine (one octave higher)
    juce::AudioBuffer<float> overdubMaterial = createSquareTestBuffer (numChannels, loopLength, sr, 880.0f);

    // Do intermittent overdubs:
    // Overdub in the middle third of the loop
    const int thirdLength = loopLength / 3;

    // First third - just playback
    juce::AudioBuffer<float> playbackBuffer1 (numChannels, thirdLength);
    track.processPlayback (playbackBuffer1, thirdLength);

    // Middle third - overdub
    juce::AudioBuffer<float> overdubSection (numChannels, thirdLength);
    overdubSection.copyFrom (0, 0, overdubMaterial, 0, thirdLength, thirdLength);
    track.processRecord (overdubSection, thirdLength);
    track.finalizeLayer();

    // Last third - just playback
    juce::AudioBuffer<float> playbackBuffer2 (numChannels, thirdLength);
    track.processPlayback (playbackBuffer2, thirdLength);

    // Verify:
    // First third should match original
    EXPECT_TRUE (compareBuffers (track.getAudioBuffer(), originalLoop, 0, thirdLength));

    // Middle third should be sum of original and overdub
    auto* loopPtr = track.getAudioBuffer().getReadPointer (0);
    auto* originalPtr = originalLoop.getReadPointer (0);
    auto* overdubPtr = overdubMaterial.getReadPointer (0);
    for (int i = 0; i < thirdLength; ++i)
    {
        float expectedSum = originalPtr[thirdLength + i] + overdubPtr[thirdLength + i];
        EXPECT_FLOAT_EQ (loopPtr[thirdLength + i], expectedSum);
    }

    // Last third should match original
    EXPECT_TRUE (compareBuffers (track.getAudioBuffer(), originalLoop, 2 * thirdLength, thirdLength));
}

TEST (LoopTrackPlayback, ProcessFullBlockCopiesToOutput)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 1;
    const int maxBlock = 4;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int numSamples = 4;
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    juce::AudioBuffer<float> output (numChannels, numSamples);
    output.clear();

    track.processPlayback (output, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    auto* outPtr = output.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (outPtr[i], loopPtr[i]);
    }
}

TEST (LoopTrackPlayback, ProcessPartialBlockCopiesToOutput)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int numSamples = 9; // less than block size
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    juce::AudioBuffer<float> output (numChannels, numSamples);
    output.clear();

    track.processPlayback (output, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    auto* outPtr = output.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (outPtr[i], loopPtr[i]);
    }
}

TEST (LoopTrackPlayback, ProcessPartialBlockCopiesToOutputWrapAround)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int leaveSamples = 10; // leave some space at end of buffer
    const int numSamples = bufferSamples - leaveSamples;

    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    juce::AudioBuffer<float> output (numChannels, numSamples);
    output.clear();

    track.processPlayback (output, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    auto* outPtr = output.getReadPointer (0);
    // Check samples read before wrap
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (outPtr[i], loopPtr[i]);
    }

    // process another partial block that will wrap around
    juce::AudioBuffer<float> output2 (numChannels, numSamples);
    output2.clear();

    track.processPlayback (output2, numSamples);
    outPtr = output2.getReadPointer (0);

    // Check samples read after wrap.
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (outPtr[i], loopPtr[(numSamples + i) % numSamples]);
    }
}

TEST (LoopTrackPlayback, ProcessMultipleChannels)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 3;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int numSamples = 12;
    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    juce::AudioBuffer<float> output (numChannels, numSamples);
    output.clear();

    track.processPlayback (output, numSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* loopPtr = loopBuffer.getReadPointer (ch);
        auto* outPtr = output.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            EXPECT_FLOAT_EQ (outPtr[i], loopPtr[i]);
        }
    }
}

TEST (LoopTrackPlayback, ZeroLengthOutputDoesNothing)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    juce::AudioBuffer<float> output (numChannels, 0); // zero length buffer

    track.processPlayback (output, 0);

    const auto& loopBuffer = track.getAudioBuffer();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* loopPtr = loopBuffer.getReadPointer (ch);
        for (int i = 0; i < loopBuffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (loopPtr[i], 0.0f);
        }
    }
}
TEST (LoopTrackPlayback, ProcessPlaybackManySmallBlocksWrapAround)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int leaveSamples = 10; // leave some space at end of buffer
    const int numSamples = bufferSamples - leaveSamples;

    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    int chunkSize = 8; // process in very small chunks
    int playbackPos = 0;
    while (playbackPos < numSamples)
    {
        juce::AudioBuffer<float> output (numChannels, chunkSize);
        output.clear();

        int thisChunk = std::min (chunkSize, numSamples - playbackPos);
        track.processPlayback (output, thisChunk);

        const auto& loopBuffer = track.getAudioBuffer();
        auto* loopPtr = loopBuffer.getReadPointer (0);
        auto* outPtr = output.getReadPointer (0);
        for (int j = 0; j < thisChunk; ++j)
        {
            int bufferIndex = (playbackPos + j) % bufferSamples;
            EXPECT_FLOAT_EQ (outPtr[j], loopPtr[bufferIndex]);
        }
        playbackPos += thisChunk;
    }
}

TEST (LoopTrackClear, ClearsBuffersAndResetsState)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int numSamples = 10;
    juce::AudioBuffer<float> input = createSquareTestBuffer (2, numSamples, 441.0, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    EXPECT_GT (track.getLength(), 0);

    track.clear();

    const auto buffer = track.getAudioBuffer();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* ptr = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (ptr[i], 0.0f);
        }
    }

    const auto& undoBuffer = track.getUndoBuffer();
    for (int ch = 0; ch < undoBuffer.getNumChannels(); ++ch)
    {
        auto undoBufferContents = undoBuffer.getBuffers()[0];
        auto* ptr = undoBufferContents.getReadPointer (ch);
        for (int i = 0; i < undoBuffer.getNumSamples(); ++i)
        {
            EXPECT_FLOAT_EQ (ptr[i], 0.0f);
        }
    }

    EXPECT_EQ (track.getLength(), 0);
}

TEST (LoopTrackUndo, RestoresPreviousState)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int numSamples = 10;
    juce::AudioBuffer<float> input = createSquareTestBuffer (1, numSamples, 441.0, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    const auto& loopBufferBefore = track.getAudioBuffer();
    auto* loopPtrBefore = loopBufferBefore.getReadPointer (0);

    // Modify the loop buffer again
    juce::AudioBuffer<float> input2 = createSquareTestBuffer (1, numSamples, 441.0, 880.0f);
    track.processRecord (input2, numSamples);
    track.finalizeLayer();

    // Undo the last change
    track.undo();

    const auto& loopBufferAfter = track.getAudioBuffer();
    auto* loopPtrAfter = loopBufferAfter.getReadPointer (0);

    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (loopPtrAfter[i], loopPtrBefore[i]);
    }
}

TEST (LoopTrackOverdubs, setOverdubGainLimits)
{
    LoopTrack track;
    track.setOverdubGains (0.0, 0.0);
    EXPECT_FLOAT_EQ (track.getOverdubOldGain(), 0.0);
    EXPECT_FLOAT_EQ (track.getOverdubNewGain(), 0.0);

    track.setOverdubGains (0.5, 0.5);
    EXPECT_FLOAT_EQ (track.getOverdubOldGain(), 0.5);
    EXPECT_FLOAT_EQ (track.getOverdubNewGain(), 0.5);

    track.setOverdubGains (-1.0, 1.5);
    EXPECT_FLOAT_EQ (track.getOverdubOldGain(), 0.0);
    EXPECT_FLOAT_EQ (track.getOverdubNewGain(), 1.5);

    track.setOverdubGains (1.0, 2.0);
    EXPECT_FLOAT_EQ (track.getOverdubOldGain(), 1.0);
    EXPECT_FLOAT_EQ (track.getOverdubNewGain(), 2.0);

    track.setOverdubGains (2.0, -1.0);
    EXPECT_FLOAT_EQ (track.getOverdubOldGain(), 2.0);
    EXPECT_FLOAT_EQ (track.getOverdubNewGain(), 0.0);

    track.setOverdubGains (-5.0, -5.0);
    EXPECT_FLOAT_EQ (track.getOverdubOldGain(), 0.0);
    EXPECT_FLOAT_EQ (track.getOverdubNewGain(), 0.0);

    track.setOverdubGains (5.0, 5.0);
    EXPECT_FLOAT_EQ (track.getOverdubOldGain(), 2.0);
    EXPECT_FLOAT_EQ (track.getOverdubNewGain(), 2.0);
}

TEST (LoopTrackUndo, MultilayerUndo)
{
    LoopTrack track;
    const double sr = 100.0;
    const int maxSeconds = 10;
    const int maxBlock = 20;
    const int numChannels = 1;
    const int undoLayers = 3;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    juce::AudioBuffer<float> mainLoopSine = createSquareTestBuffer (numChannels, maxBlock, sr, 5.0f);

    juce::AudioBuffer<float> mainLoopCopy (numChannels, maxBlock);
    mainLoopCopy.clear();
    mainLoopCopy.copyFrom (0, 0, mainLoopSine, 0, 0, maxBlock);
    auto* mainLoopCopyPtr = mainLoopCopy.getReadPointer (0);

    auto* scope = mainLoopSine.getReadPointer (0);
    std::cout << "MAIN(5hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << scope[i] << " ";
    }
    std::cout << std::endl;

    track.processRecord (mainLoopSine, maxBlock);
    track.finalizeLayer();

    const auto& audioBuffer = track.getAudioBuffer();
    auto* audioBufferPtr = audioBuffer.getReadPointer (0);

    juce::AudioBuffer<float> firstOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 10.0f);
    auto* firstOverdubPtr = firstOverdubSine.getReadPointer (0);
    std::cout << "FIRST(10hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << firstOverdubPtr[i] << " ";
    }
    std::cout << std::endl;
    track.processRecord (firstOverdubSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> secondOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 2.5f);
    auto* secondOverdubPtr = secondOverdubSine.getReadPointer (0);
    std::cout << "SECOND(2.5hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << secondOverdubPtr[i] << " ";
    }
    std::cout << std::endl;
    track.processRecord (secondOverdubSine, maxBlock);
    track.finalizeLayer();

    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    juce::AudioBuffer<float> thirdOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 25.0f);
    auto* thirdOverdubPtr = thirdOverdubSine.getReadPointer (0);
    std::cout << "THIRD(25hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << scope[i] << " ";
    }
    std::cout << std::endl;
    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i] + thirdOverdubPtr[i]);
    }

    track.undo();

    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    track.undo();
    const auto& secondUndo = track.getAudioBuffer();
    auto* secondUndoPtr = secondUndo.getReadPointer (0);

    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.undo();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    // Further undo should have no effect
    track.undo();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    auto* thirdOverdubSinePtr = thirdOverdubSine.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + thirdOverdubSinePtr[i]);
    }

    track.undo();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }
}

TEST (LoopTrackUndo, MultilayerUndoMoreThanAvailableLayers)
{
    LoopTrack track;
    const double sr = 100.0;
    const int maxSeconds = 10;
    const int maxBlock = 20;
    const int numChannels = 1;
    const int undoLayers = 2;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    juce::AudioBuffer<float> mainLoopSine = createSquareTestBuffer (numChannels, maxBlock, sr, 5.0f);

    juce::AudioBuffer<float> mainLoopCopy (numChannels, maxBlock);
    mainLoopCopy.clear();
    mainLoopCopy.copyFrom (0, 0, mainLoopSine, 0, 0, maxBlock);
    auto* mainLoopCopyPtr = mainLoopCopy.getReadPointer (0);

    auto* scope = mainLoopSine.getReadPointer (0);
    std::cout << "MAIN(5hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << scope[i] << " ";
    }
    std::cout << std::endl;

    track.processRecord (mainLoopSine, maxBlock);
    track.finalizeLayer();

    const auto& audioBuffer = track.getAudioBuffer();
    auto* audioBufferPtr = audioBuffer.getReadPointer (0);

    juce::AudioBuffer<float> firstOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 10.0f);
    auto* firstOverdubPtr = firstOverdubSine.getReadPointer (0);
    std::cout << "FIRST(10hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << firstOverdubPtr[i] << " ";
    }
    std::cout << std::endl;
    track.processRecord (firstOverdubSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> secondOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 2.5f);
    auto* secondOverdubPtr = secondOverdubSine.getReadPointer (0);
    std::cout << "SECOND(2.5hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << secondOverdubPtr[i] << " ";
    }
    std::cout << std::endl;
    track.processRecord (secondOverdubSine, maxBlock);
    track.finalizeLayer();

    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    juce::AudioBuffer<float> thirdOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 25.0f);
    auto* thirdOverdubPtr = thirdOverdubSine.getReadPointer (0);
    std::cout << "THIRD(25hz): " << std::endl;
    for (auto i = 0; i < maxBlock; ++i)
    {
        std::cout << scope[i] << " ";
    }
    std::cout << std::endl;
    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i] + thirdOverdubPtr[i]);
    }

    track.undo();

    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    track.undo();
    const auto& secondUndo = track.getAudioBuffer();
    auto* secondUndoPtr = secondUndo.getReadPointer (0);

    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.undo();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    // Further undo should have no effect
    track.undo();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    auto* thirdOverdubSinePtr = thirdOverdubSine.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + thirdOverdubSinePtr[i]);
    }

    track.undo();
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }
}

TEST (LoopTrackRelease, ReleasesResources)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    EXPECT_TRUE (track.isPrepared());
    EXPECT_NO_THROW (track.releaseResources());
    EXPECT_EQ (track.getAudioBuffer().getNumSamples(), 0);
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), 0);
    EXPECT_DOUBLE_EQ (track.getSampleRate(), 0.0);
}

TEST (LoopTrackRelease, ReleaseUnprepareResourcesDoesNothing)
{
    LoopTrack track;

    EXPECT_EQ (track.getAudioBuffer().getNumSamples(), 0);
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), 0);
    EXPECT_DOUBLE_EQ (track.getSampleRate(), 0.0);
    EXPECT_NO_THROW (track.releaseResources());
    EXPECT_EQ (track.getAudioBuffer().getNumSamples(), 0);
    EXPECT_EQ (track.getUndoBuffer().getNumSamples(), 0);
    EXPECT_DOUBLE_EQ (track.getSampleRate(), 0.0);
}

TEST (LoopTrackPlayback, PlaybackWithoutRecordingProducesSilence)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int numSamples = 9; // less than block size
    juce::AudioBuffer<float> output (numChannels, numSamples);
    output.clear();

    track.processPlayback (output, numSamples);

    auto* outPtr = output.getReadPointer (0);
    for (int i = 0; i < numSamples; ++i)
    {
        EXPECT_FLOAT_EQ (outPtr[i], 0.0f);
    }
}

TEST (LoopTrackPlayback, PlaybackTwiceWillWrapCorrectly)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 1; // Reduce buffer size to force wrap-around
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);

    const int bufferSamples = track.getAudioBuffer().getNumSamples();
    const int leaveSamples = 10; // leave some space at end of buffer
    const int numSamples = bufferSamples - leaveSamples;

    juce::AudioBuffer<float> input = createSquareTestBuffer (numChannels, numSamples, sr, 440.0f);
    track.processRecord (input, numSamples);
    track.finalizeLayer();

    int requestedSamples = bufferSamples + 71;
    juce::AudioBuffer<float> output1 (numChannels, requestedSamples);
    output1.clear();

    track.processPlayback (output1, requestedSamples);

    const auto& loopBuffer = track.getAudioBuffer();
    auto* loopPtr = loopBuffer.getReadPointer (0);
    auto* outPtr1 = output1.getReadPointer (0);
    for (int i = 0; i < requestedSamples; ++i)
    {
        auto index = i % numSamples;
        EXPECT_FLOAT_EQ (outPtr1[index], loopPtr[index]);
    }
}

} // namespace audio_plugin_test
