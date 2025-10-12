#pragma once

#include "IRenderer.h"
#include "PerfettoProfiler.h"
#include "WaveformCache.h"
#include <JuceHeader.h>

class CircularRenderer : public IRenderer
{
public:
    CircularRenderer()
    {
    }
    void render (juce::Graphics& g, const WaveformCache& cache, int readPos, int length, int width, int height, bool recording) override
    {
        PERFETTO_FUNCTION();

        if (length <= 0 || width <= 0 || height <= 0) return;

        float centerX = width / 2.0f;
        float centerY = height / 2.0f;
        float radius = std::min (width, height) / 2.0f * 0.8f;

        int cacheWidth = cache.getWidth();
        int samplesPerPixel = std::max (1, length / cacheWidth);

        // Draw circular waveform
        for (int i = 0; i < cacheWidth; ++i)
        {
            float min, max;
            if (! cache.getMinMax (i, min, max, 0)) continue;

            float angle = (float) i / (float) cacheWidth * juce::MathConstants<float>::twoPi;

            float waveformHeight = (max - min) * radius * 0.3f;
            float innerRadius = radius - waveformHeight / 2.0f;
            float outerRadius = radius + waveformHeight / 2.0f;

            float x1 = centerX + std::cos (angle) * innerRadius;
            float y1 = centerY + std::sin (angle) * innerRadius;
            float x2 = centerX + std::cos (angle) * outerRadius;
            float y2 = centerY + std::sin (angle) * outerRadius;

            int playheadPixel = (readPos / samplesPerPixel) % cacheWidth;
            juce::Colour waveColour = getWaveformColour (i, playheadPixel, recording);

            g.setColour (waveColour);
            g.drawLine (x1, y1, x2, y2, 1.5f);
        }

        drawCircularEffects (g, readPos, length, centerX, centerY, radius, recording);
    }

private:
    void drawCircularEffects (juce::Graphics& g, int readPos, int length, float centerX, float centerY, float radius, bool recording)
    {
        PERFETTO_FUNCTION();

        int cacheWidth = 800; // Approximate - could pass this in
        int samplesPerPixel = std::max (1, length / cacheWidth);
        int playheadPixel = (readPos / samplesPerPixel) % cacheWidth;

        float playheadAngle = (float) playheadPixel / (float) cacheWidth * juce::MathConstants<float>::twoPi;

        juce::Colour playheadColour = recording ? juce::Colour (255, 50, 50) : juce::Colours::white;

        // Playhead radial line
        for (int i = 0; i < 10; ++i)
        {
            float alpha = (10 - i) / 15.0f * 0.5f;
            g.setColour (playheadColour.withAlpha (alpha));

            float innerR = radius * 0.5f;
            float outerR = radius * 1.2f;

            float x1 = centerX + std::cos (playheadAngle) * innerR;
            float y1 = centerY + std::sin (playheadAngle) * innerR;
            float x2 = centerX + std::cos (playheadAngle) * outerR;
            float y2 = centerY + std::sin (playheadAngle) * outerR;

            g.drawLine (x1, y1, x2, y2, 3.0f - i * 0.2f);
        }

        // Center circle
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (centerX - radius * 0.4f, centerY - radius * 0.4f, radius * 0.8f, radius * 0.8f);

        // Outer ring
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawEllipse (centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // Vignette
        juce::ColourGradient
            vignette (juce::Colours::transparentBlack, centerX, centerY, juce::Colours::black.withAlpha (0.5f), 0.0f, 0.0f, true);
        g.setGradientFill (vignette);
        g.fillRect (0, 0, (int) (centerX * 2), (int) (centerY * 2));
    }

    juce::Colour getWaveformColour (int x, int playheadPixel, bool recording)
    {
        int distance = std::abs (x - playheadPixel);

        if (distance < 2)
            return recording ? juce::Colour (255, 50, 50) : juce::Colours::white;
        else if (distance < 10)
        {
            float fade = (10 - distance) / 10.0f;
            return juce::Colour (0, 200, 50).withAlpha (0.5f + 0.5f * fade);
        }
        else
            return juce::Colour (0, 200, 50);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CircularRenderer)
};
