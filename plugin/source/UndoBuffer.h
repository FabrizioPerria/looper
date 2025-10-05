#pragma once

#include "LoopStack.h"
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

        // printSummary (*source, start1, -1, " Before Copy");
        std::swap (undoBuffers[(size_t) start1], source);
        // printSummary (*source, start1, -1, " After Copy");

        undoLifo.finishedWrite (size1, false);

        redoLifo.clear();
    }

    bool undo (std::unique_ptr<juce::AudioBuffer<float>>& destination)
    {
        // fair to wait even if audio thread runs it. Realistically, only tests will hit the edge case where operations
        // are so fast that the copy isn't done by the time we get here.
        waitForPendingCopy();

        int uStart1, uSize1, uStart2, uSize2;
        undoLifo.prepareToRead (1, uStart1, uSize1, uStart2, uSize2);

        if (uSize1 > 0)
        {
            int rStart1, rSize1, rStart2, rSize2;
            redoLifo.prepareToWrite (1, rStart1, rSize1, rStart2, rSize2);

            // printSummary (*destination, uStart1, rStart1, " Before Undo");

            std::swap (redoBuffers[(size_t) rStart1], destination);
            std::swap (destination, undoBuffers[(size_t) uStart1]);
            // printSummary (*destination, uStart1, rStart1, " After Undo");

            redoLifo.finishedWrite (rSize1, false);
            undoLifo.finishedRead (uSize1, false);

            return true;
        }
        return false;
    }

    bool redo (std::unique_ptr<juce::AudioBuffer<float>>& destination)
    {
        // fair to wait even if audio thread runs it. Realistically, only tests will hit the edge case where operations
        // are so fast that the copy isn't done by the time we get here.
        waitForPendingCopy();

        // Get top of redo stack
        int rStart1, rSize1, rStart2, rSize2;
        redoLifo.prepareToRead (1, rStart1, rSize1, rStart2, rSize2);

        if (rSize1 > 0)
        {
            // Save current destination to undo stack
            int uStart1, uSize1, uStart2, uSize2;
            undoLifo.prepareToWrite (1, uStart1, uSize1, uStart2, uSize2);

            // printSummary (*destination, uStart1, rStart1, " Before Redo");

            std::swap (undoBuffers[(size_t) uStart1], destination);
            std::swap (destination, redoBuffers[(size_t) rStart1]);
            // printSummary (*destination, uStart1, rStart1, " After Redo");

            undoLifo.finishedWrite (uSize1, false);
            redoLifo.finishedRead (rSize1, false);

            return true;
        }
        return false;
    }

    const uint getNumSamples() const
    {
        return undoBuffers.empty() ? 0 : (uint) undoBuffers[0]->getNumSamples();
    }
    const int getNumChannels() const
    {
        return undoBuffers.empty() ? 0 : undoBuffers[0]->getNumChannels();
    }
    const size_t getNumLayers() const
    {
        return undoBuffers.size();
    }

    const std::vector<std::unique_ptr<juce::AudioBuffer<float>>>& getBuffers() const
    {
        return undoBuffers;
    }

    void clear()
    {
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
        waitForPendingCopy();
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

    // UndoBuffer.cpp
    void startAsyncCopy (const juce::AudioBuffer<float>* source, juce::AudioBuffer<float>* destination, size_t length)
    {
        // Wait for any previous copy
        activeCopy.doneEvent.reset();

        activeCopy.source = source;
        activeCopy.destination = destination;
        activeCopy.length = length;
        activeCopy.doneEvent.reset();

        threadPool.addJob (
            [this]()
            {
                // Copy using the raw pointers (safe as long as buffers aren't deallocated)
                for (int ch = 0; ch < activeCopy.source->getNumChannels(); ++ch)
                {
                    juce::FloatVectorOperations::copy (activeCopy.destination->getWritePointer (ch),
                                                       activeCopy.source->getReadPointer (ch),
                                                       (int) activeCopy.length);
                }

                activeCopy.doneEvent.signal();
            });
    }

    void finalizeCopyAndPush (std::unique_ptr<juce::AudioBuffer<float>>& tmpBuffer, size_t loopLength)
    {
        // fair to wait even if audio thread runs it. Realistically, only tests will hit the edge case where operations
        // are so fast that the copy isn't done by the time we get here.
        waitForPendingCopy();

        // Now safe to swap - background thread is done with tmpBuffer
        int start1, size1, start2, size2;
        undoLifo.prepareToWrite (1, start1, size1, start2, size2);

        length = loopLength;
        std::swap (undoBuffers[(size_t) start1], tmpBuffer);

        undoLifo.finishedWrite (size1, false);
        redoLifo.clear();
    }

    void waitForPendingCopy()
    {
        activeCopy.doneEvent.wait (100);
    }

    bool isCopyComplete() const
    {
        return activeCopy.doneEvent.wait (0);
    }

private:
    struct CopyOperation
    {
        const juce::AudioBuffer<float>* source { nullptr };
        juce::AudioBuffer<float>* destination { nullptr };
        size_t length { 0 };
        juce::WaitableEvent doneEvent;
    };

    CopyOperation activeCopy;
    juce::ThreadPool threadPool { 1 };

    LoopStack undoLifo;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> undoBuffers {};

    LoopStack redoLifo;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> redoBuffers {};

    size_t length { 0 };
    std::unique_ptr<juce::AudioBuffer<float>> undoStaging = std::make_unique<juce::AudioBuffer<float>>();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoBuffer)
};
