#pragma once

#include "LoopFifo.h"
#include "LoopLifo.h"
#include <JuceHeader.h>
#include <vector>

class UndoBuffer
{
public:
    void prepareToPlay (int numLayers, int numChannels, int bufferSamples)
    {
        lifo.prepareToPlay (numLayers);
        buffers.resize ((size_t) numLayers);
        for (auto& b : buffers)
            b.setSize (numChannels, bufferSamples, false, true, true);
    }

    void pushLayer (const juce::AudioBuffer<float>& source)
    {
        int start1, size1, start2, size2;
        lifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0) // always 0 or 1 for single-slot writes
            copyBuffer (buffers[(size_t) start1], source);

        lifo.finishedWrite (size1, false);
    }

    bool popLayer (juce::AudioBuffer<float>& destination)
    {
        int start1, size1, start2, size2;
        lifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            copyBuffer (destination, buffers[(size_t) start1]);
            lifo.finishedRead (size1, false);
            return true;
        }

        return false;
    }

    const uint getNumSamples() const
    {
        return buffers.empty() ? 0 : (uint) buffers[0].getNumSamples();
    }
    const int getNumChannels() const
    {
        return buffers.empty() ? 0 : buffers[0].getNumChannels();
    }
    const size_t getNumLayers() const
    {
        return buffers.size();
    }

    const std::vector<juce::AudioBuffer<float>>& getBuffers() const
    {
        return buffers;
    }

    void clear()
    {
        lifo.clear();
        for (auto& buf : buffers)
            buf.clear();
    }

    void releaseResources()
    {
        lifo.clear();
        for (auto& buf : buffers)
            buf.setSize (0, 0, false, false, true);
        buffers.clear();
    }

private:
    LoopLifo lifo;
    std::vector<juce::AudioBuffer<float>> buffers {};

    static void copyBuffer (juce::AudioBuffer<float>& dst, const juce::AudioBuffer<float>& src)
    {
        jassert (dst.getNumChannels() == src.getNumChannels());
        jassert (dst.getNumSamples() == src.getNumSamples());

        for (int ch = 0; ch < dst.getNumChannels(); ++ch)
            juce::FloatVectorOperations::copy (dst.getWritePointer (ch), src.getReadPointer (ch), dst.getNumSamples());
    }
};
