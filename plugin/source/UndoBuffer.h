#pragma once

#include "LoopStack.h"
#include <JuceHeader.h>
#include <vector>

class UndoBuffer
{
public:
    void prepareToPlay (int numLayers, int numChannels, int bufferSamples)
    {
        undoLifo.prepareToPlay (numLayers);
        redoLifo.prepareToPlay (numLayers);

        undoBuffers.resize ((size_t) numLayers);
        redoBuffers.resize ((size_t) numLayers);

        for (auto& b : undoBuffers)
            b.setSize (numChannels, bufferSamples, false, true, true);

        for (auto& b : redoBuffers)
            b.setSize (numChannels, bufferSamples, false, true, true);
    }

    void pushLayer (const juce::AudioBuffer<float>& source)
    {
        int start1, size1, start2, size2;
        undoLifo.prepareToWrite (1, start1, size1, start2, size2);

        if (size1 > 0)
        {
            copyBuffer (undoBuffers[(size_t) start1], source);
        }

        undoLifo.finishedWrite (size1, false);

        redoLifo.clear();
    }

    bool undo (juce::AudioBuffer<float>& destination)
    {
        int uStart1, uSize1, uStart2, uSize2;
        undoLifo.prepareToRead (1, uStart1, uSize1, uStart2, uSize2);

        if (uSize1 > 0)
        {
            int rStart1, rSize1, rStart2, rSize2;
            redoLifo.prepareToWrite (1, rStart1, rSize1, rStart2, rSize2);
            copyBuffer (redoBuffers[(size_t) rStart1], destination);
            redoLifo.finishedWrite (rSize1, false);

            copyBuffer (destination, undoBuffers[(size_t) uStart1]);
            undoLifo.finishedRead (uSize1, false);

            return true;
        }
        return false;
    }

    bool redo (juce::AudioBuffer<float>& destination)
    {
        int rStart1, rSize1, rStart2, rSize2;
        redoLifo.prepareToRead (1, rStart1, rSize1, rStart2, rSize2);

        if (rSize1 > 0)
        {
            // Save current destination to undo stack
            int uStart1, uSize1, uStart2, uSize2;
            undoLifo.prepareToWrite (1, uStart1, uSize1, uStart2, uSize2);
            copyBuffer (undoBuffers[(size_t) uStart1], destination);
            undoLifo.finishedWrite (uSize1, false);

            // Restore from redo
            copyBuffer (destination, redoBuffers[(size_t) rStart1]);
            redoLifo.finishedRead (rSize1, false);

            return true;
        }
        return false;
    }

    const uint getNumSamples() const
    {
        return undoBuffers.empty() ? 0 : (uint) undoBuffers[0].getNumSamples();
    }
    const int getNumChannels() const
    {
        return undoBuffers.empty() ? 0 : undoBuffers[0].getNumChannels();
    }
    const size_t getNumLayers() const
    {
        return undoBuffers.size();
    }

    const std::vector<juce::AudioBuffer<float>>& getBuffers() const
    {
        return undoBuffers;
    }

    void clear()
    {
        undoLifo.clear();
        for (auto& buf : undoBuffers)
            buf.clear();
    }

    void releaseResources()
    {
        undoLifo.clear();
        for (auto& buf : undoBuffers)
            buf.setSize (0, 0, false, false, true);
        undoBuffers.clear();
    }

private:
    static void copyBuffer (juce::AudioBuffer<float>& dst, const juce::AudioBuffer<float>& src)
    {
        jassert (dst.getNumChannels() == src.getNumChannels());
        jassert (dst.getNumSamples() == src.getNumSamples());

        for (int ch = 0; ch < dst.getNumChannels(); ++ch)
            juce::FloatVectorOperations::copy (dst.getWritePointer (ch), src.getReadPointer (ch), dst.getNumSamples());
    }

    LoopStack undoLifo;
    std::vector<juce::AudioBuffer<float>> undoBuffers {};

    LoopStack redoLifo;
    std::vector<juce::AudioBuffer<float>> redoBuffers {};
};
