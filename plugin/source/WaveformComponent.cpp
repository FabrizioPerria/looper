#include "WaveformComponent.h"

WaveformComponent::WaveformComponent()
{
    startTimerHz (30);
}

WaveformComponent::~WaveformComponent()
{
    stopTimer();
}

void WaveformComponent::timerCallback()
{
    if (! loopTrack || ! loopTrack->isPrepared())
    {
        repaint();
        return;
    }

    int length = loopTrack->getLength();
    int width = getWidth();

    if (length == 0 || width == 0)
    {
        repaint();
        return;
    }

    // Copy the buffer to avoid threading issues
    const auto& srcBuffer = loopTrack->getAudioBuffer();
    juce::AudioBuffer<float> bufferCopy (srcBuffer.getNumChannels(), length);

    for (int ch = 0; ch < srcBuffer.getNumChannels(); ++ch)
    {
        bufferCopy.copyFrom (ch, 0, srcBuffer, ch, 0, length);
    }

    // Update cache on background thread pool
    pool.addJob (
        [this, bufferCopy = std::move (bufferCopy), length, width]() mutable
        {
            cache.updateFromBuffer (bufferCopy, length, width);

            // Trigger repaint on message thread
            juce::MessageManager::callAsync ([this]() { repaint(); });
        });
}

void WaveformComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    if (! loopTrack || ! loopTrack->isPrepared())
    {
        g.setColour (juce::Colours::white);
        g.drawText ("No audio", getLocalBounds(), juce::Justification::centred);
        return;
    }

    int length = (int) loopTrack->getLength();
    if (length == 0)
    {
        g.setColour (juce::Colours::white);
        g.drawText ("Empty loop", getLocalBounds(), juce::Justification::centred);
        return;
    }

    // Draw from cache if available
    if (! cache.isEmpty() && cache.getWidth() > 0)
    {
        paintFromCache (g);
    }
    else
    {
        // Fallback: draw directly (will be replaced by cache soon)
        paintDirect (g);
    }
}

void WaveformComponent::paintFromCache (juce::Graphics& g)
{
    int width = cache.getWidth();
    int height = getHeight();
    float midY = height / 2.0f;

    juce::Path waveformPath;

    for (int x = 0; x < width; ++x)
    {
        float min, max;
        if (cache.getMinMax (x, min, max, 0)) // Channel 0
        {
            float y1 = midY - (max * midY * 0.9f);
            float y2 = midY - (min * midY * 0.9f);

            waveformPath.addLineSegment (juce::Line<float> ((float) x, y1, (float) x, y2), 1.0f);
        }
    }

    g.setColour (juce::Colours::green);
    g.fillPath (waveformPath);

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawHorizontalLine ((int) midY, 0.0f, (float) width);
}

void WaveformComponent::paintDirect (juce::Graphics& g)
{
    const auto& buffer = loopTrack->getAudioBuffer();
    int length = (int) loopTrack->getLength();
    int width = getWidth();
    int height = getHeight();
    float midY = height / 2.0f;

    int samplesPerPixel = std::max (1, length / width);

    juce::Path waveformPath;

    for (int x = 0; x < width; ++x)
    {
        int startSample = x * samplesPerPixel;
        int endSample = std::min (startSample + samplesPerPixel, length);

        float min = 0.0f, max = 0.0f;
        const float* data = buffer.getReadPointer (0);

        for (int i = startSample; i < endSample; ++i)
        {
            min = std::min (min, data[i]);
            max = std::max (max, data[i]);
        }

        float y1 = midY - (max * midY * 0.9f);
        float y2 = midY - (min * midY * 0.9f);

        waveformPath.addLineSegment (juce::Line<float> ((float) x, y1, (float) x, y2), 1.0f);
    }

    g.setColour (juce::Colours::green.withAlpha (0.5f)); // Dimmed to show it's temporary
    g.fillPath (waveformPath);

    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawHorizontalLine ((int) midY, 0.0f, (float) width);
}
