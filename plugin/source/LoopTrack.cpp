#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <algorithm>
#include <cassert>

namespace
{
constexpr bool kDebug = false;

static void printBuffer (const juce::AudioBuffer<float>& buffer, const int ch, const int numSamples, const std::string& label)
{
    if constexpr (! kDebug) return;

    auto* ptr = buffer.getReadPointer (ch);
    std::cout << label << ": ";
    for (auto i = 0; i < numSamples; ++i)
        std::cout << ptr[i] << " ";

    std::cout << std::endl;
}

static void printBuffer (const std::deque<juce::AudioBuffer<float>> buffers, const int ch, const int numSamples, const std::string& label)
{
    if constexpr (! kDebug) return;

    for (size_t i = 0; i < buffers.size(); ++i)
        printBuffer (buffers[i], ch, numSamples, label + " " + std::to_string (i));
}

inline void allocateBuffer (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
{
    buffer.setSize (numChannels, numSamples, false, true, true);
}

inline void copyBuffer (juce::AudioBuffer<float>& dst, const juce::AudioBuffer<float>& src)
{
    jassert (dst.getNumChannels() == src.getNumChannels());
    jassert (dst.getNumSamples() == src.getNumSamples());

    for (int ch = 0; ch < dst.getNumChannels(); ++ch)
        juce::FloatVectorOperations::copy (dst.getWritePointer (ch), src.getReadPointer (ch), dst.getNumSamples());
}
} // namespace

LoopTrack::LoopTrack() = default;
LoopTrack::~LoopTrack() = default;

void LoopTrack::prepareToPlay (const double currentSampleRate,
                               const uint maxBlockSize,
                               const uint numChannels,
                               const uint maxSeconds,
                               const size_t maxUndoLayers)
{
    if (isPrepared() || currentSampleRate <= 0.0 || maxBlockSize == 0 || numChannels == 0 || maxSeconds == 0) return;

    sampleRate = currentSampleRate;
    uint totalSamples = std::max ((uint) currentSampleRate * maxSeconds, 1u); // at least 1 block will be allocated
    uint bufferSamples = ((totalSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;

    if (bufferSamples > (uint) audioBuffer.getNumSamples())
    {
        allocateBuffer (audioBuffer, (int) numChannels, (int) bufferSamples);
    }

    undoBuffer.clear();
    undoBuffer.resize (maxUndoLayers);
    for (auto& buf : undoBuffer)
        allocateBuffer (buf, (int) numChannels, (int) bufferSamples);

    allocateBuffer (tmpBuffer, (int) numChannels, (int) maxBlockSize);

    clear();
    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade

    alreadyPrepared = true;
}

void LoopTrack::releaseResources()
{
    if (! isPrepared()) return;

    clear();
    audioBuffer.setSize (0, 0, false, false, true);

    for (auto& buf : undoBuffer)
        buf.setSize (0, 0, false, false, true);

    undoBuffer.clear();

    tmpBuffer.setSize (0, 0, false, false, true);

    sampleRate = 0.0;
    alreadyPrepared = false;
}

//==============================================================================
// Recording
//==============================================================================

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const int numSamples)
{
    if (shouldNotRecordInputBuffer (input, numSamples)) return;

    if (! isRecording)
    {
        isRecording = true;
        saveToUndoBuffer();
    }

    const int numChannels = audioBuffer.getNumChannels();
    const int bufferSamples = audioBuffer.getNumSamples();

    int samplesCanRecord = std::min (numSamples, bufferSamples - provisionalLength);

    for (int ch = 0; ch < numChannels; ++ch)
        processRecordChannel (input, samplesCanRecord, ch);

    if (length <= 0)
    {
        advanceWritePos (samplesCanRecord, bufferSamples);
        updateLoopLength (samplesCanRecord, bufferSamples);
    }

    if (samplesCanRecord < numSamples)
    {
        finalizeLayer();
    }
}

void LoopTrack::processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch)
{
    if (numSamples <= 0 || ch < 0 || ch >= audioBuffer.getNumChannels()) return;

    const float* inputPtr = input.getReadPointer (ch);
    int bufferSize = audioBuffer.getNumSamples();

    int currentWritePos = writePos;
    int samplesLeft = numSamples;

    // when loop exists, wrap the target write region to the musical length
    int wrapSize = (isOverdubbing() ? length : bufferSize);

    while (samplesLeft > 0)
    {
        int wrappedPosition = currentWritePos % wrapSize;
        int block = std::min (samplesLeft, wrapSize - wrappedPosition);
        if (block <= 0) break;

        copyInputToLoopBuffer (ch, inputPtr, wrappedPosition, block);

        inputPtr += block;
        samplesLeft -= block;
        currentWritePos = (currentWritePos + block) % bufferSize;
    }
}

void LoopTrack::saveToUndoBuffer()
{
    if (! isPrepared() || ! isOverdubbing()) return;

    auto buf = std::move (undoBuffer.back());
    undoBuffer.pop_back();

    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (buf.getWritePointer (ch), audioBuffer.getReadPointer (ch), audioBuffer.getNumSamples());
    }

    undoBuffer.push_front (std::move (buf));

    activeUndoLayers = std::min (activeUndoLayers + 1, undoBuffer.size());

    printBuffer (undoBuffer, 0, length, "UNDO");
}

void LoopTrack::copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples)
{
    auto* loopPtr = audioBuffer.getWritePointer (ch);

    if (isRecording && length > 0)
    {
        printBuffer (audioBuffer, ch, length, "LOOP BEFORE ADD/SCALE");
        for (int i = 0; i < numSamples; ++i)
        {
            loopPtr[i + offset] = loopPtr[i + offset] * overdubOldGain + bufPtr[i] * overdubNewGain;
        }
        printBuffer (audioBuffer, ch, length, "LOOP AFTER SCALE");
    }
    else if (isRecording)
    {
        juce::FloatVectorOperations::copy (loopPtr + offset, bufPtr, numSamples);
    }
}

void LoopTrack::advanceWritePos (const int numSamples, const int bufferSamples)
{
    writePos = (writePos + numSamples) % bufferSamples;
}

void LoopTrack::updateLoopLength (const int numSamples, const int bufferSamples)
{
    provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
}

void LoopTrack::finalizeLayer()
{
    length = std::max ({ provisionalLength, length, 1 });
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

    const int fadeSamples = std::min (crossFadeLength, length / 4);
    if (fadeSamples > 0)
    {
        audioBuffer.applyGainRamp (0, fadeSamples, 0.0f, overallGain);                    // fade in
        audioBuffer.applyGainRamp (length - fadeSamples, fadeSamples, overallGain, 0.0f); // fade out
    }

    printBuffer (audioBuffer, 0, length, "FINALIZED LOOP");
}

void LoopTrack::processPlayback (juce::AudioBuffer<float>& output, const int numSamples)
{
    jassert (output.getNumChannels() == audioBuffer.getNumChannels());
    int numChannels = audioBuffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processPlaybackChannel (output, numSamples, ch);
    }
    advanceReadPos (numSamples, length);
}

void LoopTrack::processPlaybackChannel (juce::AudioBuffer<float>& output, const int numSamples, const int ch)
{
    if (length <= 0 || numSamples <= 0 || ch < 0 || ch >= audioBuffer.getNumChannels())
    {
        return;
    }
    float* outPtr = output.getWritePointer (ch);
    const float* loopPtr = audioBuffer.getReadPointer (ch);

    int currentReadPos = readPos;
    int samplesLeft = numSamples;
    while (samplesLeft > 0)
    {
        int block = std::min (samplesLeft, length - currentReadPos);
        if (block <= 0)
        {
            currentReadPos = 0;
            block = std::min (samplesLeft, length);
        }
        juce::FloatVectorOperations::add (outPtr, loopPtr + currentReadPos, block);
        samplesLeft -= block;
        outPtr += block;
        currentReadPos = (currentReadPos + block) % length;
    }
}

void LoopTrack::advanceReadPos (const int numSamples, const int bufferSamples)
{
    readPos = (readPos + numSamples) % bufferSamples;
    if (length > 0)
    {
        writePos = readPos; // keep write position in sync for overdubbing
    }
}

void LoopTrack::clear()
{
    audioBuffer.clear();
    for (auto& buf : undoBuffer)
    {
        buf.clear();
    }
    tmpBuffer.clear();
    writePos = 0;
    readPos = 0;
    length = 0;
    provisionalLength = 0;
    activeUndoLayers = 0;
}

void LoopTrack::undo()
{
    if (length <= 0 || activeUndoLayers == 0) return;

    printBuffer (audioBuffer, 0, length, "BEFORE UNDO");

    auto frontBuf = std::move (undoBuffer.front());
    undoBuffer.pop_front();

    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (audioBuffer.getWritePointer (ch), frontBuf.getReadPointer (ch), audioBuffer.getNumSamples());
    }

    frontBuf.clear();
    undoBuffer.push_back (std::move (frontBuf));

    printBuffer (audioBuffer, 0, length, "AFTER UNDO");

    printBuffer (undoBuffer, 0, length, "UNDO");

    activeUndoLayers--;

    finalizeLayer();
}
