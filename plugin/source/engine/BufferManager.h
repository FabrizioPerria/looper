#pragma once

#include "engine/LoopFifo.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

class BufferManager
{
public:
    BufferManager() {}
    void prepareToPlay (const int numChannels, const int bufferSize)
    {
        PERFETTO_FUNCTION();
        audioBuffer->setSize (numChannels, bufferSize, false, true, true);
        scratchBuffer->setSize (numChannels, bufferSize, false, true, true);
        clear();
    }

    void clear()
    {
        PERFETTO_FUNCTION();
        fifo.prepareToPlay (audioBuffer->getNumSamples());
        audioBuffer->clear();
        length = 0;
        provisionalLength = 0;
        previousReadPos = -1.0;
    }

    void releaseResources()
    {
        PERFETTO_FUNCTION();
        audioBuffer->setSize (0, 0, false, false, true);
        length = 0;
        provisionalLength = 0;
        previousReadPos = 0.0;
    }

    // bool shouldOverdub() const { return length > 0; }

    std::unique_ptr<juce::AudioBuffer<float>>& getAudioBuffer() { return audioBuffer; }

    const int getNumChannels() const { return audioBuffer->getNumChannels(); }
    const int getNumSamples() const { return audioBuffer->getNumSamples(); }

    void updateLoopLength (const int numSamples, const bool isOverdub)
    {
        int bufferSamples = isOverdub ? length : audioBuffer->getNumSamples();
        provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
    }

    int getLength() const { return length; }
    void setLength (const int newLength) { length = newLength; }

    float* getWritePointer (const int channel) { return audioBuffer->getWritePointer (channel); }
    const float* getReadPointer (const int channel) const { return audioBuffer->getReadPointer (channel); }

    void finalizeLayer (const bool isOverdub)
    {
        PERFETTO_FUNCTION();
        const int currentLength = std::max ({ (int) length, (int) provisionalLength, 1 });
        if (length == 0)
        {
            fifo.setMusicalLength ((int) currentLength);
            length = currentLength;
        }
        provisionalLength = 0;
        fifo.finishedWrite (0, isOverdub, true);
    }

    // void syncReadPositionToWritePosition() { fifo.finishedWrite (0, shouldOverdub(), true); }

    bool hasWrappedAround()
    {
        double current = fifo.getExactReadPos();
        bool wrapped = current < previousReadPos;
        previousReadPos = current;
        return wrapped;
    }

    bool writeToAudioBuffer (std::function<void (float* destination, const float* source, const int numSamples, const bool shouldOverdub)>
                                 writeFunc,
                             const juce::AudioBuffer<float>& sourceBuffer,
                             const int numSamples,
                             const bool isOverdub,
                             const bool syncWriteWithRead)
    {
        int writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap;
        fifo.prepareToWrite (numSamples, writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap);
        bool isReverse = fifo.getLastPlaybackRate() < 0.0f;

        for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
        {
            if (samplesBeforeWrap > 0)
            {
                float* dest = getWritePointer (ch) + writePosBeforeWrap;
                const float* src = sourceBuffer.getReadPointer (ch);

                if (! isReverse)
                {
                    writeFunc (dest, src, samplesBeforeWrap, isOverdub);
                }
                else
                {
                    juce::FloatVectorOperations::copy (scratchBuffer->getWritePointer (ch), src, samplesBeforeWrap);
                    scratchBuffer->reverse (0, samplesBeforeWrap);
                    writeFunc (dest, scratchBuffer->getReadPointer (ch), samplesBeforeWrap, isOverdub);
                }
            }

            if (samplesAfterWrap > 0 && isOverdub)
            {
                float* dest = getWritePointer (ch) + writePosAfterWrap;
                const float* src = sourceBuffer.getReadPointer (ch) + samplesBeforeWrap;

                if (! isReverse)
                {
                    writeFunc (dest, src, samplesAfterWrap, isOverdub);
                }
                else
                {
                    juce::FloatVectorOperations::copy (scratchBuffer->getWritePointer (ch), src, samplesAfterWrap);
                    scratchBuffer->reverse (0, samplesAfterWrap);
                    writeFunc (dest, scratchBuffer->getReadPointer (ch), samplesAfterWrap, isOverdub);
                }
            }
            scratchBuffer->clear();
        }

        int actualWritten = samplesBeforeWrap + samplesAfterWrap;
        fifo.finishedWrite (actualWritten, isOverdub, syncWriteWithRead);
        bool fifoPreventedWrap = ! fifo.getWrapAround() && samplesAfterWrap == 0 && numSamples > samplesBeforeWrap;

        if (! fifoPreventedWrap) updateLoopLength (samplesBeforeWrap, isOverdub);

        return fifoPreventedWrap;
    }

    bool readFromAudioBuffer (std::function<void (float* destination, const float* source, const int numSamples)> readFunc,
                              juce::AudioBuffer<float>& destBuffer,
                              const int numSamples,
                              const float speedMultiplier,
                              const bool isOverdub)
    {
        bool isReverse = speedMultiplier < 0.0f;

        if (! isReverse)
        {
            // Forward: use fifo regions normally
            int readPosBeforeWrap, samplesBeforeWrap, readPosAfterWrap, samplesAfterWrap;
            fifo.prepareToRead (numSamples, readPosBeforeWrap, samplesBeforeWrap, readPosAfterWrap, samplesAfterWrap);

            for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
            {
                if (samplesBeforeWrap > 0)
                {
                    const float* src = getReadPointer (ch) + readPosBeforeWrap;
                    readFunc (destBuffer.getWritePointer (ch), src, samplesBeforeWrap);
                }
                if (samplesAfterWrap > 0)
                {
                    const float* src = getReadPointer (ch) + readPosAfterWrap;
                    readFunc (destBuffer.getWritePointer (ch) + samplesBeforeWrap, src, samplesAfterWrap);
                }
            }
        }
        else
        {
            // Reverse: read backward manually
            for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
            {
                float* destPtr = destBuffer.getWritePointer (ch);
                const float* srcPtr = audioBuffer->getReadPointer (ch);

                for (int i = 0; i < numSamples; ++i)
                {
                    destPtr[i] = srcPtr[fifo.getReverseReadIndex (i)];
                }
            }
        }

        fifo.finishedRead (numSamples, speedMultiplier, isOverdub);
        return true;
    }

    bool linearizeAndReadFromAudioBuffer (juce::AudioBuffer<float>& destBuffer,
                                          int sourceSamples, // How much to linearize
                                          int outputSamples, // How much to advance fifo
                                          float speedMultiplier,
                                          const bool isOverdub)
    {
        PERFETTO_FUNCTION();

        if (length == 0) return false;

        int readResult = readFromAudioBuffer ([=] (float* destination, const float* source, const int numSamples)
                                              { juce::FloatVectorOperations::copy (destination, source, numSamples); },
                                              destBuffer,
                                              sourceSamples,
                                              speedMultiplier,
                                              isOverdub);

        // Advance the fifo accounting for the difference between sourceSamples and outputSamples
        fifo.finishedRead (outputSamples - sourceSamples, speedMultiplier, isOverdub);

        return readResult;
    }

    int getReadPosition() const { return fifo.getReadPos(); }
    int getWritePosition() const { return fifo.getWritePos(); }

private:
    std::unique_ptr<juce::AudioBuffer<float>> audioBuffer = std::make_unique<juce::AudioBuffer<float>>();
    std::unique_ptr<juce::AudioBuffer<float>> scratchBuffer = std::make_unique<juce::AudioBuffer<float>>();
    int length;
    int provisionalLength;

    LoopFifo fifo;
    double previousReadPos = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BufferManager)
};
