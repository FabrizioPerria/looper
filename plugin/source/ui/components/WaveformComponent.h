#pragma once

#include "audio/AudioToUIBridge.h"
#include "audio/EngineCommandBus.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "ui/helpers/WaveformCache.h"
#include "ui/renderers/IRenderer.h"
#include "ui/renderers/LinearRenderer.h"
#include <JuceHeader.h>

class WaveformComponent : public juce::Component, public juce::Timer, public juce::AsyncUpdater, public juce::FileDragAndDropTarget
{
public:
    WaveformComponent (int trackIdx, AudioToUIBridge* audioBridge, EngineMessageBus* engineMessageBus)
        : trackIndex (trackIdx), bridge (audioBridge), uiToEngineBus (engineMessageBus)
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

    void filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/) override
    {
        // Load the first audio file that was dropped
        const juce::String filepath = files[0]; // Just take the first file if a group is provided
        juce::File file (filepath);

        if (isAudioFile (file))
        {
            uiToEngineBus->pushCommand ({ EngineMessageBus::CommandType::LoadAudioFile, trackIndex, file });
        }
    }

private:
    void handleAsyncUpdate() override;

    int trackIndex;
    WaveformCache cache;
    std::unique_ptr<IRenderer> renderer;
    AudioToUIBridge* bridge = nullptr;
    EngineMessageBus* uiToEngineBus = nullptr;
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
