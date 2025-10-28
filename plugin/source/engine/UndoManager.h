#pragma once

#include "LoopLifo.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>
#include <vector>

class UndoStackManager
{
public:
    UndoStackManager() {}
    ~UndoStackManager() { releaseResources(); }

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

    bool undo (std::unique_ptr<juce::AudioBuffer<float>>& destination)
    {
        PERFETTO_FUNCTION();
        juce::Logger::
            outputDebugString ("########################################################################\nUndoStackManager::undo called");
        printDebugInfo();
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

            juce::Logger::outputDebugString (
                "########################################################################\nUndoStackManager::undo completed");
            printDebugInfo();
            return true;
        }
        return false;
    }

    bool redo (std::unique_ptr<juce::AudioBuffer<float>>& destination)
    {
        PERFETTO_FUNCTION();
        // juce::Logger::outputDebugString ("UndoStackManager::redo called");
        // printDebugInfo();
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

            // juce::Logger::outputDebugString ("UndoStackManager::redo completed");
            // printDebugInfo();
            return true;
        }
        return false;
    }

    const int getNumSamples() const { return undoBuffers.empty() ? 0 : (int) undoBuffers[0]->getNumSamples(); }
    const int getNumChannels() const { return undoBuffers.empty() ? 0 : undoBuffers[0]->getNumChannels(); }
    const int getNumLayers() const { return (int) undoBuffers.size(); }

    const std::vector<std::unique_ptr<juce::AudioBuffer<float>>>& getBuffers() const { return undoBuffers; }

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

    void finalizeCopyAndPush (int loopLength)
    {
        PERFETTO_FUNCTION();
        juce::Logger::outputDebugString (
            "##########################################################################\nUndoStackManager::finalizeCopyAndPush called");
        printDebugInfo();
        int start1, size1, start2, size2;
        undoLifo.prepareToWrite (1, start1, size1, start2, size2);

        length = loopLength;
        std::swap (undoBuffers[(size_t) start1], undoStaging);

        undoLifo.finishedWrite (size1, false);
        redoLifo.clear();
        juce::Logger::outputDebugString (
            "###########################################################################\nUndoStackManager::finalizeCopyAndPush completed");
        printDebugInfo();
    }

    void stageCurrentBuffer (const juce::AudioBuffer<float>& sourceBuffer, int numSamples)
    {
        juce::Logger::outputDebugString (
            "###########################################################################\nUndoStackManager::stageCurrentBuffer called");
        printDebugInfo();
        PERFETTO_FUNCTION();
        undoStaging->setSize (sourceBuffer.getNumChannels(), numSamples, false, true, true);
        for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch)
        {
            juce::FloatVectorOperations::copy (undoStaging->getWritePointer (ch), sourceBuffer.getReadPointer (ch), numSamples);
        }
        juce::Logger::outputDebugString (
            "###########################################################################\nUndoStackManager::stageCurrentBuffer completed");
        printDebugInfo();
    }

private:
    LoopLifo undoLifo;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> undoBuffers {};

    LoopLifo redoLifo;
    std::vector<std::unique_ptr<juce::AudioBuffer<float>>> redoBuffers {};

    int length { 0 };
    std::unique_ptr<juce::AudioBuffer<float>> undoStaging = std::make_unique<juce::AudioBuffer<float>>();

    void printDebugInfo()
    {
        PERFETTO_FUNCTION();
        juce::Logger::outputDebugString ("Undo Stack Manager:");

        juce::Logger::outputDebugString ("STAGING BUFFER:");
        juce::String sampleStr;
        for (int i = 0; i < std::min (10, undoStaging->getNumSamples()); ++i)
        {
            sampleStr += juce::String (undoStaging->getReadPointer (0)[i]) + " ";
        }
        juce::Logger::outputDebugString ("    " + sampleStr);

        // now print all layers in undo stack
        juce::String nextLayer = ">>>";
        for (int layer = 0; layer < undoBuffers.size(); ++layer)
        {
            bool isNextLayer = (layer == undoLifo.getNextLayerIndex());
            if (isNextLayer)
                juce::Logger::outputDebugString (nextLayer + " UNDO LAYER " + juce::String (layer) + ":");
            else
                juce::Logger::outputDebugString ("UNDO LAYER " + juce::String (layer) + ":");
            juce::String sampleStr;
            for (int i = 0; i < std::min (10, undoBuffers[(size_t) layer]->getNumSamples()); ++i)
            {
                sampleStr += juce::String (undoBuffers[(size_t) layer]->getReadPointer (0)[i]) + " ";
            }
            juce::Logger::outputDebugString ("    " + sampleStr);
        }

        // now print all layers in redo stack
        for (int layer = 0; layer < redoBuffers.size(); ++layer)
        {
            bool isNextLayer = (layer == redoLifo.getNextLayerIndex());
            if (isNextLayer)
                juce::Logger::outputDebugString (nextLayer + " REDO LAYER " + juce::String (layer) + ":");
            else
                juce::Logger::outputDebugString ("REDO LAYER " + juce::String (layer) + ":");
            juce::String sampleStr;
            for (int i = 0; i < std::min (10, redoBuffers[(size_t) layer]->getNumSamples()); ++i)
            {
                sampleStr += juce::String (redoBuffers[(size_t) layer]->getReadPointer (0)[i]) + " ";
            }
            juce::Logger::outputDebugString ("    " + sampleStr);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoStackManager)
};
