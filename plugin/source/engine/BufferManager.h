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
        clear();
    }

    void clear()
    {
        PERFETTO_FUNCTION();
        audioBuffer->clear();
        length = 0;
        provisionalLength = 0;
        previousReadPos = -1.0;
        fifo.prepareToPlay (audioBuffer->getNumSamples());
    }

    void releaseResources()
    {
        PERFETTO_FUNCTION();
        audioBuffer->setSize (0, 0, false, false, true);
        length = 0;
        provisionalLength = 0;
        previousReadPos = 0.0;
    }

    bool shouldOverdub() const { return length > 0; }

    std::unique_ptr<juce::AudioBuffer<float>>& getAudioBuffer() { return audioBuffer; }

    const int getNumChannels() const { return audioBuffer->getNumChannels(); }
    const int getNumSamples() const { return audioBuffer->getNumSamples(); }

    void updateLoopLength (const int numSamples)
    {
        int bufferSamples = shouldOverdub() ? length : audioBuffer->getNumSamples();
        provisionalLength = std::min (provisionalLength + numSamples, bufferSamples);
    }

    int getLength() const { return length; }
    void setLength (const int newLength) { length = newLength; }

    float* getWritePointer (const int channel) { return audioBuffer->getWritePointer (channel); }
    const float* getReadPointer (const int channel) const { return audioBuffer->getReadPointer (channel); }

    void finalizeLayer()
    {
        PERFETTO_FUNCTION();
        const int currentLength = std::max ({ (int) length, (int) provisionalLength, 1 });
        if (length == 0)
        {
            fifo.setMusicalLength ((int) currentLength);
            length = currentLength;
        }
        provisionalLength = 0;
    }

    bool hasWrappedAround()
    {
        double current = fifo.getExactReadPos();
        bool wrapped = current < previousReadPos;
        previousReadPos = current;
        return wrapped;
    }

    LoopFifo& getFifo() { return fifo; }
    const LoopFifo& getFifo() const { return fifo; }

    bool writeToAudioBuffer (std::function<void (float* destination, const float* source, const int numSamples, const bool shouldOverdub)>
                                 writeFunc,
                             const juce::AudioBuffer<float>& sourceBuffer,
                             const int numSamples)
    {
        int writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap;
        fifo.prepareToWrite (numSamples, writePosBeforeWrap, samplesBeforeWrap, writePosAfterWrap, samplesAfterWrap);

        for (int ch = 0; ch < audioBuffer->getNumChannels(); ++ch)
        {
            if (samplesBeforeWrap > 0)
            {
                float* dest = getWritePointer (ch) + writePosBeforeWrap;
                writeFunc (dest, sourceBuffer.getReadPointer (ch), samplesBeforeWrap, shouldOverdub());
            }
            if (samplesAfterWrap > 0 && shouldOverdub())
            {
                float* dest = getWritePointer (ch) + writePosAfterWrap;
                writeFunc (dest, sourceBuffer.getReadPointer (ch) + samplesBeforeWrap, samplesAfterWrap, shouldOverdub());
            }
        }

        int actualWritten = samplesBeforeWrap + samplesAfterWrap;
        fifo.finishedWrite (actualWritten, shouldOverdub());
        bool fifoPreventedWrap = ! fifo.getWrapAround() && samplesAfterWrap == 0 && numSamples > samplesBeforeWrap;

        if (! fifoPreventedWrap) updateLoopLength ((int) samplesBeforeWrap);

        return fifoPreventedWrap;
    }

private:
    std::unique_ptr<juce::AudioBuffer<float>> audioBuffer = std::make_unique<juce::AudioBuffer<float>>();
    int length;
    int provisionalLength;

    LoopFifo fifo;
    double previousReadPos = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BufferManager)
};
