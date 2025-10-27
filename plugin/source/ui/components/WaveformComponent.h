#pragma once

#include "audio/AudioToUIBridge.h"
#include "audio/UIToEngineBridge.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "ui/helpers/WaveformCache.h"
#include "ui/renderers/IRenderer.h"
#include "ui/renderers/LinearRenderer.h"
#include <JuceHeader.h>

class WaveformComponent : public juce::Component, public juce::Timer, public juce::AsyncUpdater, public juce::FileDragAndDropTarget
{
public:
    WaveformComponent (AudioToUIBridge* audioBridge, UIToEngineBridge* engineBridge) : bridge (audioBridge), uiToEngineBridge (engineBridge)
    {
        renderer = std::make_unique<LinearRenderer>();
        startTimerHz (60);
    }

    ~WaveformComponent() override
    {
        stopTimer();
        cancelPendingUpdate();
        backgroundProcessor.removeAllJobs (true, 5000);
    }

    void paint (juce::Graphics& g) override;
    void timerCallback() override;
    void resized() override
    {
        if (bridge) triggerAsyncUpdate();
    }

    void setRenderer (std::unique_ptr<IRenderer> newRenderer)
    {
        renderer = std::move (newRenderer);
        repaint();
    }

    void clearTrack()
    {
        cache.clear();
        repaint();
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        // Check if any of the dragged files are audio files
        for (const auto& file : files)
        {
            if (file.endsWithIgnoreCase (".mp3") || file.endsWithIgnoreCase (".wav") || file.endsWithIgnoreCase (".aiff")
                || file.endsWithIgnoreCase (".aif") || file.endsWithIgnoreCase (".m4a") || file.endsWithIgnoreCase (".flac"))
            {
                return true;
            }
        }
        return false;
    }

    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        // Load the first audio file that was dropped
        for (const auto& filepath : files)
        {
            juce::File file (filepath);

            if (isAudioFile (file))
            {
                uiToEngineBridge->updateAudioFile (file);
                break; // Load only the first valid audio file
            }
        }
    }

private:
    void handleAsyncUpdate() override;

    WaveformCache cache;
    std::unique_ptr<IRenderer> renderer;
    AudioToUIBridge* bridge = nullptr;
    UIToEngineBridge* uiToEngineBridge = nullptr;
    juce::ThreadPool backgroundProcessor { 1 };

    // State tracking
    int lastReadPos = 0;
    bool lastRecording = false;
    bool lastPlaying = false;
    int lastProcessedVersion = -1;

    bool isAudioFile (const juce::File& file)
    {
        auto extension = file.getFileExtension().toLowerCase();
        return extension == ".mp3" || extension == ".wav" || extension == ".aiff" || extension == ".aif" || extension == ".m4a"
               || extension == ".flac";
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
