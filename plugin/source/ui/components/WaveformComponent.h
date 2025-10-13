#pragma once

#include "audio/AudioToUIBridge.h"
#include "ui/components/WaveformCache.h"
#include "ui/renderers/IRenderer.h"
#include <JuceHeader.h>

class WaveformComponent : public juce::Component, public juce::Timer, public juce::AsyncUpdater
{
public:
    WaveformComponent();
    ~WaveformComponent() override;

    void paint (juce::Graphics& g) override;
    void timerCallback() override;
    void resized() override
    {
        if (bridge) triggerAsyncUpdate();
    }

    void setBridge (AudioToUIBridge* newBridge)
    {
        bridge = newBridge;
    }

    void setRenderer (std::unique_ptr<IRenderer> newRenderer)
    {
        renderer = std::move (newRenderer);
        repaint();
    }

private:
    void handleAsyncUpdate() override;

    WaveformCache cache;
    std::unique_ptr<IRenderer> renderer;
    AudioToUIBridge* bridge = nullptr;
    juce::ThreadPool backgroundProcessor { 1 };

    // State tracking
    size_t lastReadPos = 0;
    bool lastRecording = false;
    bool lastPlaying = false;
    int lastProcessedVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
