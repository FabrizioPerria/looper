#include "ui/components/WaveformComponent.h"
#include "ui/colors/TokyoNight.h"

void WaveformComponent::timerCallback()
{
    PERFETTO_FUNCTION();
    if (! bridge) return;

    int length, readPos;
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

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.fillAll (LooperTheme::Colors::surface);

    if (! bridge)
    {
        g.setColour (LooperTheme::Colors::textDim);
        g.drawText ("No audio", getLocalBounds(), juce::Justification::centred);
        return;
    }

    int length, readPos;
    bool recording, playing;
    bridge->getPlaybackState (length, readPos, recording, playing);

    bool isSubLoop = regionEndSample > regionStartSample;
    renderer->render (g, cache, (int) readPos, (int) length, getWidth(), getHeight(), recording, isSubLoop);

    if (isSubLoop)
    {
        auto regionBounds = juce::Rectangle<int> (sampleToX (regionStartSample),
                                                  0,
                                                  sampleToX (regionEndSample) - sampleToX (regionStartSample),
                                                  getHeight());

        g.setColour (LooperTheme::Colors::green.brighter (0.6f).withAlpha (0.2f));
        g.fillRect (regionBounds);

        // Subloop region time overlays
        int subloopLength = regionEndSample - regionStartSample;
        double sampleRate = cache.getTrackLength() > 0 ? 44100.0 : 0.0; // You may need to pass actual sample rate

        // Bottom left: start time
        drawTimeOverlay (g,
                         formatTime (regionStartSample, sampleRate),
                         (float) regionBounds.getX() + 4,
                         (float) getHeight() - 4,
                         juce::Justification::bottomLeft);

        // Bottom right: end time
        drawTimeOverlay (g,
                         formatTime (regionEndSample, sampleRate),
                         (float) regionBounds.getRight() - 4,
                         (float) getHeight() - 4,
                         juce::Justification::bottomRight);

        // Top center: region length
        drawTimeOverlay (g,
                         formatTime (subloopLength, sampleRate),
                         (float) regionBounds.getCentreX(),
                         4.0f,
                         juce::Justification::centredTop);
    }

    if (length > 0)
    {
        double sampleRate = 44100.0; // You may need to pass actual sample rate

        // Bottom right: total length
        drawTimeOverlay (g,
                         formatTime (length, sampleRate),
                         (float) getWidth() - 4,
                         (float) getHeight() - 4,
                         juce::Justification::bottomRight);

        // Near playhead: current position
        float playheadX = sampleToX (readPos);
        drawTimeOverlay (g, formatTime (readPos, sampleRate), playheadX + 4, 4.0f, juce::Justification::topLeft);
    }
}

void WaveformComponent::onVBlankCallback()
{
    PERFETTO_FUNCTION();
    if (! bridge) return;

    // Only check atomic flag - very cheap!
    if (bridge->playbackPositionChanged.exchange (false, std::memory_order_acquire))
    {
        int length, readPos;
        bool recording, playing;
        bridge->getPlaybackState (length, readPos, recording, playing);

        lastReadPos = readPos;
        lastRecording = recording;
        lastPlaying = playing;

        repaint(); // Or use partial repaint (see below)
    }

    if (++vblankCounter % 8 == 0) // Check every 4th VBlank to reduce load
    {
        if (bridge->getState().stateVersion.load (std::memory_order_relaxed) != lastProcessedVersion)
        {
            triggerAsyncUpdate();
        }
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
            [this, capturedSnapshot = std::move (snapshot), targetWidth]() mutable
            {
                cache.updateFromBuffer (capturedSnapshot.buffer, capturedSnapshot.length, targetWidth);
                juce::MessageManager::callAsync ([this]() { repaint(); });
            });
    }
}
