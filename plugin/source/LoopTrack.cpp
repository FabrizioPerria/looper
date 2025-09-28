#include "LoopTrack.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <algorithm>
#include <cassert>

static void printBuffer (const juce::AudioBuffer<float>& buffer, const int ch, const int numSamples, const std::string& label)
{
    auto* ptr = buffer.getReadPointer (ch);
    std::cout << label << ": ";
    for (auto i = 0; i < numSamples; ++i)
    {
        std::cout << ptr[i] << " ";
    }
    std::cout << std::endl;
}

LoopTrack::LoopTrack()
{
}

LoopTrack::~LoopTrack()
{
}

void LoopTrack::prepareToPlay (const double currentSampleRate,
                               const uint maxBlockSize,
                               const uint numChannels,
                               const uint maxSeconds,
                               const size_t maxUndoLayers)
{
    if (currentSampleRate <= 0.0 || maxBlockSize == 0 || numChannels == 0 || maxSeconds == 0)
    {
        return;
    }
    sampleRate = currentSampleRate;
    uint totalSamples = std::max ((uint) currentSampleRate * maxSeconds, 1u); // at least 1 block will be allocated
    uint bufferSamples = ((totalSamples + maxBlockSize - 1) / maxBlockSize) * maxBlockSize;

    if (bufferSamples > (uint) audioBuffer.getNumSamples())
    {
        audioBuffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);
    }

    undoBuffer.clear();
    for (int i = 0; i < maxUndoLayers; ++i)
    {
        auto buffer = juce::AudioBuffer<float>();
        buffer.setSize ((int) numChannels, (int) bufferSamples, false, true, true);

        undoBuffer.push_back (std::move (buffer));
    }

    tmpBuffer.setSize (1, maxBlockSize, false, true, true);

    clear();

    setCrossFadeLength ((int) (0.01 * sampleRate)); // default 10 ms crossfade

    alreadyPrepared = true;
}

void LoopTrack::releaseResources()
{
    if (! alreadyPrepared)
    {
        return;
    }
    clear();
    audioBuffer.setSize (0, 0, false, false, true);
    for (auto& buf : undoBuffer)
    {
        buf.setSize (0, 0, false, false, true);
    }
    undoBuffer.clear();
    tmpBuffer.setSize (0, 0, false, false, true);
    sampleRate = 0.0;
}

void LoopTrack::processRecord (const juce::AudioBuffer<float>& input, const int numSamples)
{
    if (numSamples <= 0 || input.getNumSamples() < numSamples || ! alreadyPrepared
        || input.getNumChannels() != audioBuffer.getNumChannels())
    {
        return;
    }

    if (! isRecording)
    {
        isRecording = true;
        saveToUndoBuffer (length);
    }

    const int numChannels = audioBuffer.getNumChannels();
    const int bufferSamples = audioBuffer.getNumSamples();

    int samplesCanRecord = std::min (numSamples, bufferSamples - provisionalLength);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        processRecordChannel (input, samplesCanRecord, ch);
    }

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

// void LoopTrack::processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch)
// {
//     if (numSamples <= 0 || ch < 0 || ch >= audioBuffer.getNumChannels())
//     {
//         return;
//     }
//     const float* inputPtr = input.getReadPointer (ch);
//     int bufferSize = audioBuffer.getNumSamples();
//
//     int currentWritePos = writePos;
//     int samplesLeft = numSamples;
//
//     while (samplesLeft > 0)
//     {
//         int block = std::min (samplesLeft, bufferSize - currentWritePos);
//         if (block <= 0)
//         {
//             break;
//         }
//         copyInputToLoopBuffer (ch, inputPtr, currentWritePos, block);
//         inputPtr += block;
//         samplesLeft -= block;
//         currentWritePos = (currentWritePos + block) % bufferSize;
//     }
// }

void LoopTrack::processRecordChannel (const juce::AudioBuffer<float>& input, const int numSamples, const int ch)
{
    if (numSamples <= 0 || ch < 0 || ch >= audioBuffer.getNumChannels())
        return;

    const float* inputPtr = input.getReadPointer (ch);
    int bufferSize = audioBuffer.getNumSamples();

    int currentWritePos = writePos;
    int samplesLeft = numSamples;

    while (samplesLeft > 0)
    {
        // when loop exists, wrap the target write region to the musical length
        int wrapSize = (length > 0 ? length : bufferSize);

        int block = std::min (samplesLeft, wrapSize - (currentWritePos % wrapSize));
        if (block <= 0)
            break;

        int wrappedPos = (length > 0) ? (currentWritePos % length) : currentWritePos;

        copyInputToLoopBuffer (ch, inputPtr, wrappedPos, block);

        inputPtr += block;
        samplesLeft -= block;
        currentWritePos = (currentWritePos + block) % bufferSize; // physical pointer still wraps to buffer size
    }
}

void LoopTrack::saveToUndoBuffer (const int numSamples)
{
    if (! alreadyPrepared || length <= 0)
        return;

    auto& buf = undoBuffer[undoIndex];

    // Ensure the buffer has the right size
    if (buf.getNumChannels() != audioBuffer.getNumChannels() || buf.getNumSamples() != audioBuffer.getNumSamples())
    {
        buf.setSize (audioBuffer.getNumChannels(), audioBuffer.getNumSamples(), false, true, true);
    }

    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (buf.getWritePointer (ch), audioBuffer.getReadPointer (ch), audioBuffer.getNumSamples());
    }

    activeUndoLayers = std::min (activeUndoLayers + 1, undoBuffer.size());
    undoIndex = (undoIndex + 1) % undoBuffer.size();
    for (auto i = 0; i < undoBuffer.size(); ++i)
    {
        printBuffer (undoBuffer[i], 0, length, "UNDO " + std::to_string (i));
    }
}

void LoopTrack::copyInputToLoopBuffer (const int ch, const float* bufPtr, const int offset, const int numSamples)
{
    auto* loopPtr = audioBuffer.getWritePointer (ch);

    if (isRecording && length > 0)
    {
        printBuffer (audioBuffer, ch, length, "LOOP BEFORE ADD/SCALE");
        std::cout << "Offset: " << offset << " NumSamples: " << numSamples << std::endl;
        std::cout << "AudioBuffer NumSamples: " << audioBuffer.getNumSamples() << std::endl;
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
    if (length <= 0 || activeUndoLayers == 0)
        return;

    // Update indexes
    undoIndex = (undoIndex + undoBuffer.size() - 1) % undoBuffer.size();
    // The most recent snapshot is at undoIndex
    auto& buf = undoBuffer[undoIndex];
    printBuffer (audioBuffer, 0, length, "BEFORE UNDO");

    // Copy snapshot back into audioBuffer
    for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
    {
        juce::FloatVectorOperations::copy (audioBuffer.getWritePointer (ch), buf.getReadPointer (ch), audioBuffer.getNumSamples());
    }
    printBuffer (audioBuffer, 0, length, "AFTER UNDO");

    activeUndoLayers--;

    // Re-finalize loop length if needed
    finalizeLayer();
}
