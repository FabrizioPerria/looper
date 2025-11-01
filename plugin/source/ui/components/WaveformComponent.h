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
        vBlankAttachment = std::make_unique<juce::VBlankAttachment> (this, [this]() { onVBlankCallback(); });
        startTimerHz (10);
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
        const juce::String file = files[0]; // Just take the first file if a group is provided

        return (file.endsWithIgnoreCase (".mp3") || file.endsWithIgnoreCase (".wav") || file.endsWithIgnoreCase (".aiff")
                || file.endsWithIgnoreCase (".aif") || file.endsWithIgnoreCase (".m4a") || file.endsWithIgnoreCase (".flac"));
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

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isShiftDown())
        { // Shift+drag to set loop region
            isDraggingRegion = true;
            dragStartX = event.x;
            regionStartSample = xToSample (event.x);
        }
        else
        {
            isDraggingRegion = false;
            regionStartSample = 0;
            regionEndSample = 0;
            uiToEngineBus->pushCommand ({ EngineMessageBus::CommandType::ClearSubLoopRegion, trackIndex, {} });
        }
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (isDraggingRegion)
        {
            dragEndX = event.x;
            regionEndSample = xToSample (event.x);
            if (regionEndSample < regionStartSample) std::swap (regionStartSample, regionEndSample);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (isDraggingRegion)
        {
            dragEndX = event.x;
            regionEndSample = xToSample (event.x);
            if (regionEndSample < regionStartSample) std::swap (regionStartSample, regionEndSample);
            isDraggingRegion = false;
            repaint();
            uiToEngineBus->pushCommand ({ EngineMessageBus::CommandType::SetSubLoopRegion,
                                          trackIndex,
                                          std::make_pair (regionStartSample, regionEndSample) });
        }
    }

private:
    void onVBlankCallback();
    void handleAsyncUpdate();

    bool isDraggingRegion = false;
    int dragStartX = 0;
    int dragEndX = 0;
    int regionStartSample = 0;
    int regionEndSample = 0;
    int xToSample (int x)
    {
        float normalized = (float) x / getWidth();
        return (int) (normalized * cache.getTrackLength());
    }

    int sampleToX (int sample)
    {
        float normalized = (float) sample / cache.getTrackLength();
        return (int) (normalized * getWidth());
    }

    int trackIndex;
    WaveformCache cache;
    std::unique_ptr<IRenderer> renderer;
    AudioToUIBridge* bridge = nullptr;
    EngineMessageBus* uiToEngineBus = nullptr;
    juce::ThreadPool backgroundProcessor { 1 };

    int vblankCounter = 0;
    std::unique_ptr<juce::VBlankAttachment> vBlankAttachment;

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
