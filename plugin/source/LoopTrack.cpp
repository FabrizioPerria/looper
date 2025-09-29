#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <algorithm>
#include <cassert>

namespace
{
constexpr bool kDebug = false;

static void printBuffer (const juce::AudioBuffer<float>& buffer, const uint ch, const uint numSamples, const std::string& label)
{
    if constexpr (! kDebug) return;

    auto* ptr = buffer.getReadPointer ((int) ch);
    std::cout << label << ": ";
    for (auto i = 0; i < numSamples; ++i)
        std::cout << ptr[i] << " ";

    std::cout << std::endl;
}

static void
    printBuffer (const std::deque<juce::AudioBuffer<float>>& buffers, const uint ch, const uint numSamples, const std::string& label)
{
    if constexpr (! kDebug) return;

    for (size_t i = 0; i < buffers.size(); ++i)
        printBuffer (buffers[i], ch, numSamples, label + " " + std::to_string (i));
}

inline void allocateBuffer (juce::AudioBuffer<float>& buffer, uint numChannels, uint numSamples)
{
    buffer.setSize ((int) numChannels, (int) numSamples, false, true, true);
}

} // namespace

//==============================================================================
// Setup
//==============================================================================

LoopTrack::LoopTrack() = default;
LoopTrack::~LoopTrack() = default;

void LoopTrack::prepareToPlay (const double currentSampleRate,
                               const uint maxBlockSize,
                               const uint numChannels,
                               const uint maxSeconds,
                               const size_t maxUndoLayers)
{
    if (isPrepared() || currentSampleRate <= 0.0 || ! maxBlockSize || ! numChannels || ! maxSeconds) return;

    sampleRate = currentSampleRate;
    const uint totalSamples = std::max ((uint) currentSampleRate * maxSeconds, 1u); // at least 1 block will be allocated
    const uint bufferSamples = ((totalSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;

    if (bufferSamples > (uint) audioBuffer.getNumSamples())
    {
        allocateBuffer (audioBuffer, numChannels, bufferSamples);
    }

    fifo.prepareToPlay ((int) bufferSamples);
    undoBuffer.prepareToPlay ((int) maxUndoLayers, (int) numChannels, (int) bufferSamples);

    allocateBuffer (tmpBuffer, numChannels, maxBlockSize);

    clear();
    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade

    alreadyPrepared = true;
}

void LoopTrack::releaseResources()
{
    if (! isPrepared()) return;

    clear();
    audioBuffer.setSize (0, 0, false, false, true);

    undoBuffer.clear();
    fifo.clear();

    tmpBuffer.setSize (0, 0, false, false, true);

    sampleRate = 0.0;
    alreadyPrepared = false;
}

//==============================================================================
// Recording
//==============================================================================

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const uint numSamples)
{
    if (shouldNotRecordInputBuffer (input, numSamples)) return;

    if (! isRecording)
    {
        isRecording = true;
        saveToUndoBuffer();
    }

    int start1, size1, start2, size2;
    fifo.prepareToWrite ((int) numSamples, start1, size1, start2, size2);

    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    {
        if (size1 > 0) copyInputToLoopBuffer ((uint) ch, input.getReadPointer (ch), (uint) start1, (uint) size1);
        if (size2 > 0 && shouldOverdub()) copyInputToLoopBuffer ((uint) ch, input.getReadPointer (ch) + size1, (uint) start2, (uint) size2);
    }

    int actualWritten = size1 + size2;
    fifo.finishedWrite (actualWritten, shouldOverdub());

    updateLoopLength ((uint) actualWritten, (uint) audioBuffer.getNumSamples());
}

void LoopTrack::saveToUndoBuffer()
{
    if (! isPrepared() || ! shouldOverdub()) return;

    undoBuffer.pushLayer (audioBuffer);
}

void LoopTrack::copyInputToLoopBuffer (const uint ch, const float* bufPtr, const uint offset, const uint numSamples)
{
    auto* loopPtr = audioBuffer.getWritePointer ((int) ch);

    if (isRecording && length > 0)
    {
        for (uint i = 0; i < numSamples; ++i)
        {
            loopPtr[i + offset] = loopPtr[i + offset] * overdubOldGain + bufPtr[i] * overdubNewGain;
        }
    }
    else if (isRecording)
    {
        juce::FloatVectorOperations::copy (loopPtr + offset, bufPtr, (int) numSamples);
    }
}

void LoopTrack::updateLoopLength (const uint numSamples, const uint bufferSamples)
{
    provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
}

void LoopTrack::finalizeLayer()
{
    const int currentLength = std::max ({ length, provisionalLength, 1u });
    if (length == 0)
    {
        fifo.setMusicalLength ((int) currentLength);
        length = currentLength;
    }
    provisionalLength = 0;
    isRecording = false;

    // float maxSample = 0.0f;
    // for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    //     maxSample = std::max (maxSample, audioBuffer.getMagnitude (ch, 0, length));
    //
    // if (maxSample > 0.0f)
    //     audioBuffer.applyGain (0, length, 1.0f / maxSample);

    const float overallGain = 1.0f;
    // audioBuffer.applyGain (overallGain);

    const uint fadeSamples = std::min (crossFadeLength, length / 4);
    if (fadeSamples > 0)
    {
        audioBuffer.applyGainRamp (0, fadeSamples, 0.0f, overallGain);                    // fade in
        audioBuffer.applyGainRamp (length - fadeSamples, fadeSamples, overallGain, 0.0f); // fade out
    }
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const uint numSamples)
{
    jassert (output.getNumChannels() == audioBuffer.getNumChannels());

    int start1, size1, start2, size2;
    fifo.prepareToRead ((int) numSamples, start1, size1, start2, size2);

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        float* outPtr = output.getWritePointer (ch);
        const float* loopPtr = audioBuffer.getReadPointer (ch);

        if (size1 > 0) juce::FloatVectorOperations::add (outPtr, loopPtr + start1, size1);
        if (size2 > 0) juce::FloatVectorOperations::add (outPtr + size1, loopPtr + start2, size2);
    }

    fifo.finishedRead (size1 + size2, shouldOverdub());
}

void LoopTrack::clear()
{
    audioBuffer.clear();
    undoBuffer.clear();
    tmpBuffer.clear();
    writePos = 0;
    readPos = 0;
    length = 0;
    provisionalLength = 0;
}

void LoopTrack::undo()
{
    if (! shouldOverdub() || ! isPrepared()) return;

    if (undoBuffer.popLayer (tmpBuffer))
    {
        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
        {
            juce::FloatVectorOperations::copy (audioBuffer.getWritePointer (ch),
                                               tmpBuffer.getReadPointer (ch),
                                               audioBuffer.getNumSamples());
        }
        finalizeLayer();
    }
}
