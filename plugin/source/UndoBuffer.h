#pragma once

#include "LoopLifo.h"
#include "PerfettoProfiler.h"
#include <JuceHeader.h>
#include <vector>

class UndoBuffer
{
public:
    UndoBuffer()
    {
    }
    ~UndoBuffer()
    {
        releaseResources();
    }

    void prepareToPlay (int numLayers, int numChannels, int bufferSamples)
    {
        PERFETTO_FUNCTION();
        undoLifo.prepareToPlay (numLayers);
        redoLifo.prepareToPlay (numLayers);

        undoBuffers.resize ((size_t) numLayers);
        redoBuffers.resize ((size_t) numLayers);

        for (auto& b : undoBuffers)
        {
            auto buf = std::make_unique<juce::AudioBuffer<float>>();
            b = std::move (buf);
            b->setSize (numChannels, bufferSamples, false, true, true);
        }

        for (auto& b : redoBuffers)
        {
            auto buf = std::make_unique<juce::AudioBuffer<float>>();
            b = std::move (buf);
            b->setSize (numChannels, bufferSamples, false, true, true);
        }

        undoStaging->setSize (numChannels, bufferSamples, false, true, true);

        length = 0;
    }

    void printBufferSummary (const juce::AudioBuffer<float>& buf, const std::string& name, bool isActive)
    {
        if (buf.getNumChannels() > 0 && buf.getNumSamples() > 0)
        {
            auto* ptr = buf.getReadPointer (0);
            std::cout << name;
            if (isActive)
                std::cout << ">> ";
            else
                std::cout << "   ";
            int len = std::min (20, buf.getNumSamples());
            for (int i = 0; i < len; ++i)
                std::cout << " " << ptr[i];
        }
        std::cout << std::endl;
    }

    void printSummary (const juce::AudioBuffer<float>& destination, int uStart1, int rStart1, std::string action)
    {
        printBufferSummary (destination, "Dest" + action, false);
        for (uint i = 0; i < undoBuffers.size(); ++i)
            printBufferSummary (*undoBuffers[i], "Undo Buffer " + std::to_string (i) + action, i == (uint) uStart1);
        for (uint i = 0; i < redoBuffers.size(); ++i)
            printBufferSummary (*redoBuffers[i], "Redo Buffer " + std::to_string (i) + action, i == (uint) rStart1);
        std::cout << "----" << std::endl;
    }

    void pushLayer (std::unique_ptr<juce::AudioBuffer<float>>& source, size_t loopLength)
    {
        PERFETTO_FUNCTION();
        for (int ch = 0; ch < source->getNumChannels(); ++ch)
        {
            juce::FloatVectorOperations::copy (undoStaging->getWritePointer (ch), source->getReadPointer (ch), (int) length);
        }

        int start1, size1, start2, size2;
        undoLifo.prepareToWrite (1, start1, size1, start2, size2);

        if (loopLength > 0 && loopLength < (size_t) source->getNumSamples())
            length = loopLength;
        else
            length = (size_t) source->getNumSamples();

        std::swap (undoBuffers[(size_t) start1], source);

        undoLifo.finishedWrite (size1, false);

        redoLifo.clear();
    }

    bool undo (std::unique_ptr<juce::AudioBuffer<float>>& destination)
    {
        PERFETTO_FUNCTION();
        int uStart1, uSize1, uStart2, uSize2;
        undoLifo.prepareToRead (1, uStart1, uSize1, uStart2, uSize2);

        if (uSize1 > 0)
        {
            int rStart1, rSize1, rStart2, rSize2;
            redoLifo.prepareToWrite (1, rStart1, rSize1, rStart2, rSize2);

            std::swap (redoBuffers[(size_t) rStart1], destination);
            std::swap (destination, undoBuffers[(size_t) uStart1]);

            redoLifo.finishedWrite (rSize1, false);
            undoLifo.finishedRead (uSize1, false);

            return true;
        }
        return false;
    }

    bool redo (std::unique_ptr<juce::AudioBuffer<float>>& destination)
    {
        PERFETTO_FUNCTION();
        // Get top of redo stack
        int rStart1, rSize1, rStart2, rSize2;
        redoLifo.prepareToRead (1, rStart1, rSize1, rStart2, rSize2);

        if (rSize1 > 0)
        {
            // Save current destination to undo stack
            int uStart1, uSize1, uStart2, uSize2;
            undoLifo.prepareToWrite (1, uStart1, uSize1, uStart2, uSize2);

            std::swap (undoBuffers[(size_t) uStart1], destination);
            std::swap (destination, redoBuffers[(size_t) rStart1]);

            undoLifo.finishedWrite (uSize1, false);
            redoLifo.finishedRead (rSize1, false);

            return true;
        }
        return false;
    }

    const uint getNumSamples() const
    {
        PERFETTO_FUNCTION();
        return undoBuffers.empty() ? 0 : (uint) undoBuffers[0]->getNumSamples();
    }
    const int getNumChannels() const
    {
        PERFETTO_FUNCTION();
        return undoBuffers.empty() ? 0 : undoBuffers[0]->getNumChannels();
    }
    const size_t getNumLayers() const
    {
        PERFETTO_FUNCTION();
        return undoBuffers.size();
    }

    const std::vector<std::unique_ptr<juce::AudioBuffer<float>>>& getBuffers() const
    {
        PERFETTO_FUNCTION();
        return undoBuffers;
    }

    void clear()
    {
        PERFETTO_FUNCTION();
        undoLifo.clear();
        redoLifo.clear();
        for (auto& buf : undoBuffers)
            buf->clear();
        for (auto& buf : redoBuffers)
            buf->clear();
        undoStaging->clear();
        length = 0;
    }

    void releaseResources()
    {
        PERFETTO_FUNCTION();
        undoLifo.clear();
        redoLifo.clear();
        for (auto& buf : undoBuffers)
            buf->setSize (0, 0, false, false, true);
        for (auto& buf : redoBuffers)
            buf->setSize (0, 0, false, false, true);
        undoStaging->setSize (0, 0, false, false, true);
        undoBuffers.clear();
        redoBuffers.clear();
    }

    void finalizeCopyAndPush (std::unique_ptr<juce::AudioBuffer<float>>& tmpBuffer, size_t loopLength)
    {
        PERFETTO_FUNCTION();
        int start1, size1, start2, size2;
        undoLifo.prepareToWrite (1, start1, size1, start2, size2);

        length = loopLength;
        std::swap (undoBuffers[(size_t) start1], tmpBuffer);

        undoLifo.finishedWrite (size1, false);
        redoLifo.clear();
    }

private:
    LoopLifo undoLifo;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> undoBuffers {};

    LoopLifo redoLifo;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> redoBuffers {};

    size_t length { 0 };
    std::unique_ptr<juce::AudioBuffer<float>> undoStaging = std::make_unique<juce::AudioBuffer<float>>();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoBuffer)
};
