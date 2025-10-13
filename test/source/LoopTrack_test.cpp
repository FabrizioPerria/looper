#include <gtest/gtest.h>

#include "engine/LoopTrack.h"

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
        auto& undoBufferContents = undoBuffer.getBuffers()[0];
        auto* ptr = undoBufferContents->getReadPointer (ch);
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
    track.setCrossFadeLength (0);
    track.setOverdubGains (1.0f, 1.0f);

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
    track.setOverdubGains (1.0f, 1.0f);

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
    track.setOverdubGains (1.0f, 1.0f);

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
    track.setOverdubGains (1.0f, 1.0f);

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

TEST (LoopTrackOverdub, IntermittentOverdubOnlyAffectsActiveRecordingPeriods)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 1;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setOverdubGains (1.0f, 1.0f);

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
        auto& undoBufferContents = undoBuffer.getBuffers()[0];
        auto* ptr = undoBufferContents->getReadPointer (ch);
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
    track.setOverdubGains (1.0f, 1.0f);

    juce::AudioBuffer<float> mainLoopSine = createSquareTestBuffer (numChannels, maxBlock, sr, 5.0f);

    juce::AudioBuffer<float> mainLoopCopy (numChannels, maxBlock);
    mainLoopCopy.clear();
    mainLoopCopy.copyFrom (0, 0, mainLoopSine, 0, 0, maxBlock);
    auto* mainLoopCopyPtr = mainLoopCopy.getReadPointer (0);

    track.processRecord (mainLoopSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> firstOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 10.0f);
    auto* firstOverdubPtr = firstOverdubSine.getReadPointer (0);
    track.processRecord (firstOverdubSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> secondOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 2.5f);
    auto* secondOverdubPtr = secondOverdubSine.getReadPointer (0);
    track.processRecord (secondOverdubSine, maxBlock);
    track.finalizeLayer();

    auto* audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    juce::AudioBuffer<float> thirdOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 25.0f);
    auto* thirdOverdubPtr = thirdOverdubSine.getReadPointer (0);
    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i] + thirdOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    const auto& secondUndo = track.getAudioBuffer();
    auto* secondUndoPtr = secondUndo.getReadPointer (0);

    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    // Further undo should have no effect
    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    auto* thirdOverdubSinePtr = thirdOverdubSine.getReadPointer (0);
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + thirdOverdubSinePtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }
}
TEST (LoopTrackUndo, MultilayerUndoWithRedo)
{
    LoopTrack track;
    const double sr = 100.0;
    const int maxSeconds = 10;
    const int maxBlock = 20;
    const int numChannels = 1;
    const int undoLayers = 3;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.setOverdubGains (1.0f, 1.0f);

    juce::AudioBuffer<float> mainLoopSine = createSquareTestBuffer (numChannels, maxBlock, sr, 5.0f);

    juce::AudioBuffer<float> mainLoopCopy (numChannels, maxBlock);
    mainLoopCopy.clear();
    mainLoopCopy.copyFrom (0, 0, mainLoopSine, 0, 0, maxBlock);
    auto* mainLoopCopyPtr = mainLoopCopy.getReadPointer (0);

    auto* scope = mainLoopSine.getReadPointer (0);

    track.processRecord (mainLoopSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> firstOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 10.0f);
    auto* firstOverdubPtr = firstOverdubSine.getReadPointer (0);
    track.processRecord (firstOverdubSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> secondOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 2.5f);
    auto* secondOverdubPtr = secondOverdubSine.getReadPointer (0);
    track.processRecord (secondOverdubSine, maxBlock);
    track.finalizeLayer();

    auto* audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    juce::AudioBuffer<float> thirdOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 25.0f);
    auto* thirdOverdubPtr = thirdOverdubSine.getReadPointer (0);
    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i] + thirdOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    track.undo();
    const auto& secondUndo = track.getAudioBuffer();
    auto* secondUndoPtr = secondUndo.getReadPointer (0);

    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    track.redo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.redo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    track.redo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i] + thirdOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    // Further undo should have no effect
    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    auto* thirdOverdubSinePtr = thirdOverdubSine.getReadPointer (0);
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + thirdOverdubSinePtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i]);
    }

    track.redo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + thirdOverdubSinePtr[i]);
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
    track.setOverdubGains (1.0f, 1.0f);

    juce::AudioBuffer<float> mainLoopSine = createSquareTestBuffer (numChannels, maxBlock, sr, 5.0f);

    juce::AudioBuffer<float> mainLoopCopy (numChannels, maxBlock);
    mainLoopCopy.clear();
    mainLoopCopy.copyFrom (0, 0, mainLoopSine, 0, 0, maxBlock);
    auto* mainLoopCopyPtr = mainLoopCopy.getReadPointer (0);

    auto* scope = mainLoopSine.getReadPointer (0);
    track.processRecord (mainLoopSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> firstOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 10.0f);
    auto* firstOverdubPtr = firstOverdubSine.getReadPointer (0);
    track.processRecord (firstOverdubSine, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> secondOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 2.5f);
    auto* secondOverdubPtr = secondOverdubSine.getReadPointer (0);
    track.processRecord (secondOverdubSine, maxBlock);
    track.finalizeLayer();

    auto* audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    juce::AudioBuffer<float> thirdOverdubSine = createSquareTestBuffer (numChannels, maxBlock, sr, 25.0f);
    auto* thirdOverdubPtr = thirdOverdubSine.getReadPointer (0);
    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i] + thirdOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + secondOverdubPtr[i]);
    }

    track.undo();
    const auto& secondUndo = track.getAudioBuffer();
    auto* secondUndoPtr = secondUndo.getReadPointer (0);

    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    // Further undo should have no effect
    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i]);
    }

    track.processRecord (thirdOverdubSine, maxBlock);
    track.finalizeLayer();
    auto* thirdOverdubSinePtr = thirdOverdubSine.getReadPointer (0);
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (audioBufferPtr[i], mainLoopCopyPtr[i] + firstOverdubPtr[i] + thirdOverdubSinePtr[i]);
    }

    track.undo();
    audioBufferPtr = track.getAudioBuffer().getReadPointer (0);
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

// Test normalization prevents clipping
TEST (LoopTrackNormalization, PreventsClippingOnMultipleOverdubs)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 3;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Enable normalization (default)
    track.enableOutputNormalization();

    // Record initial loop with high level (0.8)
    juce::AudioBuffer<float> initialLoop (numChannels, maxBlock);
    initialLoop.clear();
    for (int i = 0; i < maxBlock; ++i)
        initialLoop.setSample (0, i, 0.8f);

    track.processRecord (initialLoop, maxBlock);
    track.finalizeLayer();

    // First overdub at same level
    track.processRecord (initialLoop, maxBlock);
    track.finalizeLayer();

    // Second overdub
    track.processRecord (initialLoop, maxBlock);
    track.finalizeLayer();

    // Check that output is normalized and not clipping
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_LE (std::abs (ptr[i]), 0.9f);  // Should be normalized to 0.9 max
        EXPECT_GT (std::abs (ptr[i]), 0.85f); // Should be close to target
    }
}

// Test normalization disabled with manual gains
TEST (LoopTrackNormalization, ManualGainsDisableNormalization)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Set manual gains - this should disable normalization
    track.setOverdubGains (1.0f, 1.0f);

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    for (int i = 0; i < maxBlock; ++i)
        input.setSample (0, i, 0.5f);

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // With manual gains 1.0/1.0, output should be 0.5 + 0.5 = 1.0 (no normalization)
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (ptr[i], 1.0f);
    }
}

// Test common feedback setting: 70% (typical hardware looper)
TEST (LoopTrackFeedback, Feedback70Percent)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // 70% feedback - old audio fades, new audio at 100%
    track.setOverdubGains (0.7f, 1.0f);

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    for (int i = 0; i < maxBlock; ++i)
        input.setSample (0, i, 0.5f);

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // Result should be: 0.5 * 0.7 + 0.5 * 1.0 = 0.35 + 0.5 = 0.85
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (ptr[i], 0.85f);
    }
}

// Test replace mode: 0% feedback
TEST (LoopTrackFeedback, ReplaceModeZeroFeedback)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // Replace mode - new completely replaces old
    track.setOverdubGains (0.0f, 1.0f);

    juce::AudioBuffer<float> initialLoop (numChannels, maxBlock);
    initialLoop.clear();
    for (int i = 0; i < maxBlock; ++i)
        initialLoop.setSample (0, i, 0.8f);

    track.processRecord (initialLoop, maxBlock);
    track.finalizeLayer();

    juce::AudioBuffer<float> newLoop (numChannels, maxBlock);
    newLoop.clear();
    for (int i = 0; i < maxBlock; ++i)
        newLoop.setSample (0, i, 0.3f);

    track.processRecord (newLoop, maxBlock);
    track.finalizeLayer();

    // Should only have new material
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
    {
        EXPECT_FLOAT_EQ (ptr[i], 0.3f);
    }
}

// Test feedback decay over multiple layers
TEST (LoopTrackFeedback, MultipleLayerDecay)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 5;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    // 50% feedback
    track.setOverdubGains (0.5f, 1.0f);

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    for (int i = 0; i < maxBlock; ++i)
        input.setSample (0, i, 1.0f);

    // Layer 1: 1.0
    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // Layer 2: 1.0 * 0.5 + 1.0 = 1.5
    track.processRecord (input, maxBlock);
    track.finalizeLayer();
    EXPECT_FLOAT_EQ (track.getAudioBuffer().getSample (0, 0), 1.5f);

    // Layer 3: 1.5 * 0.5 + 1.0 = 1.75
    track.processRecord (input, maxBlock);
    track.finalizeLayer();
    EXPECT_FLOAT_EQ (track.getAudioBuffer().getSample (0, 0), 1.75f);

    // Layer 4: 1.75 * 0.5 + 1.0 = 1.875
    track.processRecord (input, maxBlock);
    track.finalizeLayer();
    EXPECT_FLOAT_EQ (track.getAudioBuffer().getSample (0, 0), 1.875f);

    // Converges toward 2.0 (geometric series sum = 1/(1-0.5) = 2.0)
}

// Test normalization maintains relative levels between channels
TEST (LoopTrackNormalization, MaintainsChannelBalance)
{
    LoopTrack track;
    const double sr = 44100.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.enableOutputNormalization();

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();

    // Left channel at 1.0, right at 0.5
    for (int i = 0; i < maxBlock; ++i)
    {
        input.setSample (0, i, 1.0f);
        input.setSample (1, i, 0.5f);
    }

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    auto* leftPtr = track.getAudioBuffer().getReadPointer (0);
    auto* rightPtr = track.getAudioBuffer().getReadPointer (1);

    // After normalization to 0.9, left should be 0.9, right should be 0.45
    EXPECT_NEAR (leftPtr[0], 0.9f, 0.01f);
    EXPECT_NEAR (rightPtr[0], 0.45f, 0.01f);

    // Ratio should be preserved
    EXPECT_NEAR (leftPtr[0] / rightPtr[0], 2.0f, 0.01f);
}
TEST (LoopTrackRealWorld, HumanInteractionTiming)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 30;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 5;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.enableOutputNormalization();

    auto simulateAudioCallback = [&] (juce::AudioBuffer<float>& buffer, int numSamples) { track.processPlayback (buffer, numSamples); };

    auto waitHumanReaction = [] (int milliseconds) { juce::Thread::sleep (milliseconds); };

    // === SCENARIO: Musician building a loop ===

    // 1. Record initial 4-bar drum loop (8 seconds at 120 BPM)
    const int drumLoopSamples = (int) (8.0 * sr);
    juce::AudioBuffer<float> drumLoop (numChannels, drumLoopSamples);
    // Use DC offset + sine for non-zero signal
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < drumLoopSamples; ++i)
            // drumLoop.setSample (ch, i, 0.3f + 0.2f * std::sin (2.0 * M_PI * 100.0 * i / sr));
            drumLoop.setSample (ch, i, 0.5f); // Constant DC signal; easier to test against

    track.processRecord (drumLoop, drumLoopSamples);
    track.finalizeLayer();

    // Human takes 1 second to listen to the loop
    waitHumanReaction (1000);

    // During this time, playback continues
    juce::AudioBuffer<float> playbackBuffer (numChannels, maxBlock);
    for (int i = 0; i < 20; ++i)
    {
        playbackBuffer.clear();
        simulateAudioCallback (playbackBuffer, maxBlock);
        waitHumanReaction (50);
    }

    // 2. User decides to add bass (waits 500ms, then records)
    waitHumanReaction (500);

    juce::AudioBuffer<float> bassLoop (numChannels, drumLoopSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < drumLoopSamples; ++i)
            bassLoop.setSample (ch, i, 0.4f + 0.3f * std::sin (2.0 * M_PI * 50.0 * i / sr));

    track.processRecord (bassLoop, drumLoopSamples);
    track.finalizeLayer();

    EXPECT_TRUE (track.getLength() > 0);

    // Listen again (2 seconds)
    waitHumanReaction (2000);

    // 3. Add guitar melody
    waitHumanReaction (300);

    juce::AudioBuffer<float> guitarLoop (numChannels, drumLoopSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < drumLoopSamples; ++i)
            guitarLoop.setSample (ch, i, 0.35f + 0.25f * std::sin (2.0 * M_PI * 440.0 * i / sr));

    track.processRecord (guitarLoop, drumLoopSamples);
    track.finalizeLayer();

    // 4. Oops, guitar was too loud - undo
    waitHumanReaction (1500);
    track.undo();

    EXPECT_EQ (track.getLength(), drumLoopSamples);

    // 5. Redo to get guitar back
    waitHumanReaction (800);
    track.redo();

    // 6. Actually, let's try a different guitar part - undo again
    waitHumanReaction (500);
    track.undo();

    // 7. Record new guitar with different melody
    waitHumanReaction (1000);

    juce::AudioBuffer<float> guitarLoop2 (numChannels, drumLoopSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < drumLoopSamples; ++i)
            guitarLoop2.setSample (ch, i, 0.3f + 0.2f * std::sin (2.0 * M_PI * 550.0 * i / sr));

    track.processRecord (guitarLoop2, drumLoopSamples);
    track.finalizeLayer();

    // 8. Multiple rapid undos
    waitHumanReaction (100);
    track.undo();

    waitHumanReaction (100);
    track.undo();

    waitHumanReaction (100);
    track.undo();

    // Final state: just drums (normalized, should be close to 0.9 peak)
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    // checking sample 2000 which should be outside the crossfade zone
    EXPECT_LE (std::abs (ptr[2000]), 0.9f); // Normalized peak

    EXPECT_EQ (track.getLength(), drumLoopSamples);
}

// Stress test: rapid operations
TEST (LoopTrackRealWorld, RapidOperationsStressTest)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 2;
    const int undoLayers = 10;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setOverdubGains (0.7f, 1.0f); // 70% feedback

    const int loopSamples = (int) (2.0 * sr); // 2-second loop

    // Record initial loop
    juce::AudioBuffer<float> input (numChannels, loopSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < loopSamples; ++i)
            input.setSample (ch, i, 0.5f);

    track.processRecord (input, loopSamples);
    track.finalizeLayer();

    // Rapid overdubs with minimal delay (worst case scenario)
    for (int layer = 0; layer < 5; ++layer)
    {
        juce::Thread::sleep (50); // Just 50ms between operations
        track.processRecord (input, loopSamples);
        track.finalizeLayer();
    }

    // Rapid undo/redo sequence
    juce::Thread::sleep (50);
    track.undo();

    juce::Thread::sleep (50);
    track.redo();

    juce::Thread::sleep (50);
    track.undo();

    juce::Thread::sleep (50);
    track.undo();

    // Should survive without crashes
    EXPECT_GT (track.getLength(), 0);
}

// Edge case: immediate undo after finalize (no human delay)
TEST (LoopTrackRealWorld, ImmediateUndoAfterFinalize)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 5;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 3;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setOverdubGains (1.0f, 1.0f);

    const int loopSamples = (int) (1.0 * sr);
    juce::AudioBuffer<float> input (numChannels, loopSamples);
    input.clear();
    for (int i = 0; i < loopSamples; ++i)
        input.setSample (0, i, 0.5f);

    // First layer
    track.processRecord (input, loopSamples);
    track.finalizeLayer();

    // Second layer
    track.processRecord (input, loopSamples);
    track.finalizeLayer();

    // IMMEDIATE undo (no delay - worst case)
    // This will force a wait on the async copy
    track.undo();

    // Should work correctly despite forced wait.
    EXPECT_FLOAT_EQ (track.getAudioBuffer().getSample (0, 2000), 0.5f);
}

// Test that overdub stops at loop boundary when wrap disabled
TEST (LoopTrackQuantizedOverdub, StopsAtLoopBoundary)
{
    LoopTrack track;
    const double sr = 48000.0;
    const int maxSeconds = 10;
    const int maxBlock = 512;
    const int numChannels = 1;
    const int undoLayers = 2;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.setOverdubGains (1.0f, 1.0f);

    // Record 2-second loop
    const int loopSamples = (int) (2.0 * sr);
    juce::AudioBuffer<float> initialLoop (numChannels, loopSamples);
    for (int i = 0; i < loopSamples; ++i)
        initialLoop.setSample (0, i, 0.5f);

    track.processRecord (initialLoop, loopSamples);
    track.finalizeLayer();

    EXPECT_EQ (track.getLength(), loopSamples);

    // Disable wrap for quantized overdubs
    track.preventWrapAround();

    // Try to overdub 3 seconds (longer than loop)
    const int overdubSamples = (int) (3.0 * sr);
    juce::AudioBuffer<float> longOverdub (numChannels, overdubSamples);
    for (int i = 0; i < overdubSamples; ++i)
        longOverdub.setSample (0, i, 0.3f);

    // Process in blocks
    int samplesProcessed = 0;
    while (samplesProcessed < overdubSamples && track.isCurrentlyRecording())
    {
        int blockSize = std::min (maxBlock, overdubSamples - samplesProcessed);
        juce::AudioBuffer<float> block (numChannels, blockSize);
        block.copyFrom (0, 0, longOverdub, 0, samplesProcessed, blockSize);

        track.processRecord (block, blockSize);
        samplesProcessed += blockSize;
    }

    // Loop length should remain unchanged
    EXPECT_EQ (track.getLength(), loopSamples);

    // Recording should have stopped at loop boundary
    EXPECT_FALSE (track.isCurrentlyRecording());
}

// Test wrap-around boundary detection
TEST (LoopTrackQuantizedOverdub, DetectsWrapBoundary)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 10;
    const int maxBlock = 4;
    const int numChannels = 1;
    const int undoLayers = 2;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.setOverdubGains (1.0f, 1.0f);

    // Record 8-sample loop
    juce::AudioBuffer<float> initialLoop (numChannels, 8);
    for (int i = 0; i < 8; ++i)
        initialLoop.setSample (0, i, 0.5f);

    track.processRecord (initialLoop, 8);
    track.finalizeLayer();

    track.preventWrapAround();

    // Advance playback to near end (sample 6)
    juce::AudioBuffer<float> dummy (numChannels, maxBlock);
    track.processPlayback (dummy, 4); // At sample 4
    track.processPlayback (dummy, 2); // At sample 6

    // Try to record 4 samples (would wrap after 2)
    juce::AudioBuffer<float> overdub (numChannels, 4);
    for (int i = 0; i < 4; ++i)
        overdub.setSample (0, i, 0.3f);

    track.processRecord (overdub, 4);

    // Should have only written 2 samples and finalized
    EXPECT_FALSE (track.isCurrentlyRecording());

    // Check that only samples 6-7 were overdubbed
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    EXPECT_FLOAT_EQ (ptr[6], 0.8f); // 0.5 + 0.3
    EXPECT_FLOAT_EQ (ptr[7], 0.8f);
    EXPECT_FLOAT_EQ (ptr[0], 0.5f); // Unchanged (wrap prevented)
}

// Test that first recording still allows wrap
TEST (LoopTrackQuantizedOverdub, FirstRecordingAllowsWrap)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 1;
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);

    track.preventWrapAround();

    // First recording should still be able to wrap to establish loop length
    juce::AudioBuffer<float> input (numChannels, 8);
    for (int i = 0; i < 8; ++i)
        input.setSample (0, i, 0.5f);

    track.processRecord (input, 8);
    track.finalizeLayer();

    EXPECT_EQ (track.getLength(), 8);

    // All 8 samples should be recorded
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < 8; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.9f); // norma
}

// Test wrap enabled (default) allows overdub past boundary
TEST (LoopTrackQuantizedOverdub, WrapEnabledAllowsLongOverdub)
{
    LoopTrack track;
    const double sr = 10.0;
    const int maxSeconds = 10;
    const int maxBlock = 4;
    const int numChannels = 1;
    const int undoLayers = 2;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.setOverdubGains (1.0f, 1.0f);

    // Record 8-sample loop
    juce::AudioBuffer<float> initialLoop (numChannels, 8);
    for (int i = 0; i < 8; ++i)
        initialLoop.setSample (0, i, 0.5f);

    track.processRecord (initialLoop, 8);
    track.finalizeLayer();

    // Keep wrap enabled (default)
    track.allowWrapAround();

    // Record 12 samples (wraps around)
    juce::AudioBuffer<float> longOverdub (numChannels, 12);
    for (int i = 0; i < 12; ++i)
        longOverdub.setSample (0, i, 0.3f);

    track.processRecord (longOverdub, 12);
    track.finalizeLayer();

    // All 8 samples should have been overdubbed (4 samples wrapped)
    auto* ptr = track.getAudioBuffer().getReadPointer (0);
    for (int i = 0; i < 8; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.8f); // 0.5 + 0.3
}

TEST (LoopTrackMute, MuteUnmuteFunctionality)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 2;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.setOverdubGains (1.0f, 1.0f);

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    for (int i = 0; i < maxBlock; ++i)
        input.setSample (0, i, 0.5f); // Left channel
    for (int i = 0; i < maxBlock; ++i)
        input.setSample (1, i, 0.25f); // Right channel

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // Initially not muted
    EXPECT_FALSE (track.isMuted());
    juce::AudioBuffer<float> playbackBuffer (numChannels, maxBlock);
    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    auto* ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.5f);

    // Mute the track
    track.setMuted (true);
    EXPECT_TRUE (track.isMuted());

    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.0f); // Should be silent when muted

    // Unmute the track
    track.setMuted (false);
    EXPECT_FALSE (track.isMuted());
    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.5f); // Original audio should be back
    //
    // Test muting again
    track.setMuted (true);
    EXPECT_TRUE (track.isMuted());
    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.0f); // Should be silent when muted
}

TEST (LoopTrackVolume, VolumeAdjustment)
{
    LoopTrack track;
    const double sr = 441.0;
    const int maxSeconds = 10;
    const int maxBlock = 12;
    const int numChannels = 1;
    const int undoLayers = 1;
    track.prepareToPlay (sr, maxBlock, numChannels, maxSeconds, undoLayers);
    track.setCrossFadeLength (0);
    track.setOverdubGains (1.0f, 1.0f);

    juce::AudioBuffer<float> input (numChannels, maxBlock);
    input.clear();
    for (int i = 0; i < maxBlock; ++i)
        input.setSample (0, i, 0.5f); // Mono signal

    track.processRecord (input, maxBlock);
    track.finalizeLayer();

    // Default volume should be 1.0
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 1.0f);

    juce::AudioBuffer<float> playbackBuffer (numChannels, maxBlock);
    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    auto* ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.5f); // Original level

    // Set volume to 0.5
    track.setTrackVolume (0.5f);
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 0.5f);

    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.25f); // Half the original level

    // Set volume to 2.0 (boost)
    track.setTrackVolume (2.0f);
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 1.0f); // we clamp to 1.0

    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.5f); // Clamped to original level (no boost)

    // Set volume back to 1.0
    track.setTrackVolume (1.0f);
    EXPECT_FLOAT_EQ (track.getTrackVolume(), 1.0f);
    playbackBuffer.clear();
    track.processPlayback (playbackBuffer, maxBlock);
    ptr = playbackBuffer.getReadPointer (0);
    for (int i = 0; i < maxBlock; ++i)
        EXPECT_FLOAT_EQ (ptr[i], 0.5f); // Original level restored
}

TEST (Perf, measureCopyTimeForAudioBuffer)
{
    const int numChannels = 2;
    const int numSamples = 48000 * 10; // 10 seconds at 48kHz
    juce::AudioBuffer<float> sourceBuffer (numChannels, numSamples);
    juce::AudioBuffer<float> destBuffer (numChannels, numSamples);

    // Fill source buffer with test data
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            sourceBuffer.setSample (ch, i, static_cast<float> (i % 100) / 100.0f);

    // let's copy 20 times and take the average

    auto durationAvg = 0.0;
    for (int iter = 0; iter < 20; ++iter)
    {
        auto start = std::chrono::high_resolution_clock::now();
        destBuffer.makeCopyOf (sourceBuffer);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono ::duration<double, std::milli> copyDuration = end - start;
        durationAvg += copyDuration.count();
    }
    durationAvg /= 20.0;
    std::cout << "Average AudioBuffer copy time for " << numChannels << " channels and " << numSamples << " samples: " << durationAvg
              << " ms" << std::endl;
    std::cout << "Average AudioBuffer copy speed: " << (numChannels * numSamples * sizeof (float)) / (durationAvg * 1e6) << " GB/s"
              << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    destBuffer.makeCopyOf (sourceBuffer);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> copyDuration = end - start;

    std::cout << "AudioBuffer copy time for " << numChannels << " channels and " << numSamples << " samples: " << copyDuration.count()
              << " ms" << std::endl;

    // Verify that the copy was successful
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            EXPECT_FLOAT_EQ (destBuffer.getSample (ch, i), sourceBuffer.getSample (ch, i));
}
} // namespace audio_plugin_test
