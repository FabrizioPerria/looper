#pragma once

#include "audio/AudioToUIBridge.h"
#include "ui/helpers/WaveformCache.h"
#include "ui/renderers/IRenderer.h"
#include "ui/renderers/LinearRenderer.h"
#include <JuceHeader.h>

class WaveformComponent : public juce::Component, public juce::Timer, public juce::AsyncUpdater
{
public:
    WaveformComponent (AudioToUIBridge* audioBridge) : bridge (audioBridge)
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

private:
    void handleAsyncUpdate() override;

    WaveformCache cache;
    std::unique_ptr<IRenderer> renderer;
    AudioToUIBridge* bridge = nullptr;
    juce::ThreadPool backgroundProcessor { 1 };

    // State tracking
    int lastReadPos = 0;
    bool lastRecording = false;
    bool lastPlaying = false;
    int lastProcessedVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
