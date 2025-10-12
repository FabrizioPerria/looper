#pragma once

#include "AudioToUIBridge.h"
#include "IRenderer.h"
#include "WaveformCache.h"
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
        PERFETTO_FUNCTION();
        // When resized, rebuild cache with new width
        if (bridge)
        {
            triggerAsyncUpdate();
        }
    }

    void setBridge (AudioToUIBridge* newBridge)
    {
        bridge = newBridge;
    }

private:
    void getMinMaxForPixel (int pixelIndex, float& min, float& max);
    void paintFromCache (juce::Graphics& g, int readPos, int length, bool recording);
    void paintDirect (juce::Graphics& g, size_t readPos);

    void handleAsyncUpdate() override
    {
        PERFETTO_FUNCTION();
        if (! bridge) return;

        AudioToUIBridge::WaveformSnapshot snapshot;

        // Get snapshot from bridge (non-blocking)
        if (bridge->getWaveformSnapshot (snapshot))
        {
            int targetWidth = getWidth();
            if (targetWidth <= 0) return;

            lastProcessedVersion = snapshot.version;

            // Process waveform on background thread
            backgroundProcessor.addJob (
                [this, snapshot = std::move (snapshot), targetWidth]() mutable
                {
                    // This runs on background thread - safe to do expensive work
                    cache.updateFromBuffer (snapshot.buffer, snapshot.length, targetWidth);

                    // Trigger repaint on message thread
                    juce::MessageManager::callAsync ([this]() { repaint(); });
                });
        }
    }
    WaveformCache cache;
    std::unique_ptr<IRenderer> renderer;

    AudioToUIBridge* bridge = nullptr;
    juce::ThreadPool backgroundProcessor { 1 };

    // State tracking
    size_t lastReadPos = 0;
    bool lastRecording = false;
    bool lastPlaying = false;
    int lastProcessedVersion = -1;
    size_t currentLength = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
