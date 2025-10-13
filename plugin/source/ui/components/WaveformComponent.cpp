#include "WaveformComponent.h"
#include "ui/renderers/CircularRenderer.h"
#include "ui/renderers/LinearRenderer.h"

WaveformComponent::WaveformComponent()
{
    // renderer = std::make_unique<CircularRenderer>();
    renderer = std::make_unique<LinearRenderer>();
    startTimerHz (60);
}

WaveformComponent::~WaveformComponent()
{
    stopTimer();
    cancelPendingUpdate();
    backgroundProcessor.removeAllJobs (true, 5000);
}

void WaveformComponent::timerCallback()
{
    PERFETTO_FUNCTION();
    if (! bridge) return;

    size_t length, readPos;
    bool recording, playing;
    bridge->getPlaybackState (length, readPos, recording, playing);

    bool stateChanged = (recording != lastRecording || playing != lastPlaying);
    bool posChanged = (readPos != lastReadPos);

    if (stateChanged || posChanged)
    {
        lastReadPos = readPos;
        lastRecording = recording;
        lastPlaying = playing;
        repaint();
    }

    if (bridge->getState().stateVersion.load (std::memory_order_relaxed) != lastProcessedVersion)
    {
        triggerAsyncUpdate();
    }
}

void WaveformComponent::paint (juce::Graphics& g)
{
    PERFETTO_FUNCTION();
    g.fillAll (juce::Colours::black);

    if (! bridge)
    {
        g.setColour (juce::Colours::white);
        g.drawText ("No audio bridge", getLocalBounds(), juce::Justification::centred);
        return;
    }

    size_t length, readPos;
    bool recording, playing;
    bridge->getPlaybackState (length, readPos, recording, playing);

    if (length == 0)
    {
        g.setColour (juce::Colours::white);
        g.drawText ("Empty loop", getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (! cache.isEmpty() && cache.getWidth() > 0)
    {
        renderer->render (g, cache, (int) readPos, (int) length, getWidth(), getHeight(), recording);
    }
    else
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Loading waveform...", getLocalBounds(), juce::Justification::centred);
    }
}

void WaveformComponent::handleAsyncUpdate()
{
    PERFETTO_FUNCTION();
    if (! bridge) return;

    AudioToUIBridge::WaveformSnapshot snapshot;

    if (bridge->getWaveformSnapshot (snapshot))
    {
        int targetWidth = getWidth();
        if (targetWidth <= 0) return;

        lastProcessedVersion = snapshot.version;

        backgroundProcessor.addJob (
            [this, snapshot = std::move (snapshot), targetWidth]() mutable
            {
                cache.updateFromBuffer (snapshot.buffer, snapshot.length, targetWidth);
                juce::MessageManager::callAsync ([this]() { repaint(); });
            });
    }
}
