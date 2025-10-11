#include "WaveformComponent.h"

WaveformComponent::WaveformComponent (LoopTrack* track) : loopTrack (track)
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
    g.fillAll (juce::Colours::green.withAlpha (0.4f));
    auto isRecording = loopTrack->isCurrentlyRecording();
    bool shouldOverdub = loopTrack->getLength() > 0;
    auto isMuted = loopTrack->isMuted();

    if (isRecording)
    {
        if (shouldOverdub)
        {
            g.fillAll (juce::Colours::yellow.withAlpha (0.4f));
        }
        else
        {
            g.fillAll (juce::Colours::red.withAlpha (0.4f));
        }
    }
    else if (isMuted)
    {
        g.fillAll (juce::Colours::grey.withAlpha (0.4f));
    }

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

    auto readPos = loopTrack->getCurrentReadPosition();
    // Draw from cache if available
    if (! cache.isEmpty() && cache.getWidth() > 0)
    {
        paintFromCache (g, readPos);
    }
    else
    {
        // Fallback: draw directly (will be replaced by cache soon)
        paintDirect (g, readPos);
    }
}

void WaveformComponent::paintFromCache (juce::Graphics& g, size_t readPos)
{
    int width = cache.getWidth();
    int height = getHeight();
    int length = (int) loopTrack->getLength();
    int samplesPerPixel = std::max (1, length / width);
    int readPixel = readPos / samplesPerPixel;

    // Draw waveform from cache
    for (int x = 0; x < width; ++x)
    {
        float min, max;
        if (cache.getMinMax (x, min, max, 0))
        {
            drawWaveformColumn (g, x, min, max, readPixel, height);
        }
    }

    drawCRTEffects (g, readPixel, width, height);
}

void WaveformComponent::paintDirect (juce::Graphics& g, size_t readPos)
{
    const auto& buffer = loopTrack->getAudioBuffer();
    int length = (int) loopTrack->getLength();
    int width = getWidth();
    int height = getHeight();
    int samplesPerPixel = std::max (1, length / width);
    int readPixel = readPos / samplesPerPixel;

    for (int x = 0; x < width; ++x)
    {
        int startSample = x * samplesPerPixel;
        int endSample = std::min (startSample + samplesPerPixel, length);
        auto range = buffer.findMinMax (0, startSample, endSample - startSample);
        drawWaveformColumn (g, x, range.getStart(), range.getEnd(), readPixel, height);
    }

    drawCRTEffects (g, readPixel, width, height);
}

void WaveformComponent::drawWaveformColumn (juce::Graphics& g, int x, float min, float max, int readPixel, int height)
{
    float midY = height / 2.0f;
    float y1 = midY - (max * midY * 0.9f);
    float y2 = midY - (min * midY * 0.9f);

    juce::Colour waveColour = getWaveformColour (x, readPixel);
    g.setColour (waveColour);
    g.drawLine ((float) x, y1, (float) x, y2, 1.5f);
}

juce::Colour WaveformComponent::getWaveformColour (int x, int readPixel)
{
    int distance = std::abs (x - readPixel);

    if (distance < 2)
    {
        // Bright red at playhead
        return juce::Colour (255, 50, 50);
    }
    else if (distance < 10)
    {
        // Red glow fade to green
        float fade = (10 - distance) / 10.0f;
        return juce::Colour::fromFloatRGBA (0.5f + 0.5f * fade,   // R
                                            0.8f * (1.0f - fade), // G
                                            0.2f * (1.0f - fade), // B
                                            1.0f);
    }
    else
    {
        // Phosphor green
        return juce::Colour (0, 200, 50);
    }
}

void WaveformComponent::drawCRTEffects (juce::Graphics& g, int readPixel, int width, int height)
{
    float midY = height / 2.0f;

    // CRT scanlines
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    for (int y = 0; y < height; y += 2)
    {
        g.drawHorizontalLine (y, 0.0f, (float) width);
    }

    // Playhead vertical line with glow
    for (int i = 0; i < 15; ++i)
    {
        float alpha = (15 - i) / 15.0f * 0.4f;
        g.setColour (juce::Colour (255, 50, 50).withAlpha (alpha));
        if (readPixel - i >= 0) g.drawVerticalLine (readPixel - i, 0.0f, (float) height);
        if (readPixel + i < width) g.drawVerticalLine (readPixel + i, 0.0f, (float) height);
    }

    // Center line
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawHorizontalLine ((int) midY, 0.0f, (float) width);

    // Vignette effect (darker edges like old CRT)
    juce::ColourGradient
        vignette (juce::Colours::transparentBlack, width / 2.0f, height / 2.0f, juce::Colours::black.withAlpha (0.3f), 0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillRect (0, 0, width, height);
}
