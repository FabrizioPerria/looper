#pragma once

#include "IRenderer.h"
#include "profiler/PerfettoProfiler.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/WaveformCache.h"
#include <JuceHeader.h>

class LinearRenderer : public IRenderer
{
public:
    LinearRenderer() {}

    void render (juce::Graphics& g, const WaveformCache& cache, int readPos, int length, int width, int height, bool isRecording)
    {
        // Background - use theme color, not black
        g.fillAll (LooperTheme::Colors::backgroundDark);

        int centerY = height / 2;

        // Center line - always visible
        g.setColour (LooperTheme::Colors::border.withAlpha (0.3f));
        g.drawLine (0.0f, (float) centerY, (float) width, (float) centerY, 1.0f);

        // If no loop, show flat line and return
        if (cache.isEmpty() || length == 0)
        {
            // Draw flat line in the center to indicate "ready to record"
            g.setColour (LooperTheme::Colors::textDim.withAlpha (0.3f));
            g.drawLine (0.0f, (float) centerY, (float) width, (float) centerY, 2.0f);
            return;
        }

        int cacheWidth = cache.getWidth();
        if (cacheWidth == 0) return;

        // Draw waveform
        g.setColour (LooperTheme::Colors::cyan.withAlpha (0.6f));

        for (int x = 0; x < width; ++x)
        {
            int cacheIndex = (x * cacheWidth) / width;
            float min, max;

            if (cache.getMinMax (cacheIndex, min, max, 0))
            {
                float y1 = centerY - (max * centerY * 0.85f);
                float y2 = centerY - (min * centerY * 0.85f);

                g.drawLine ((float) x, y1, (float) x, y2, 1.5f);
            }
        }

        // Playhead - color changes based on state
        float playheadX = (float) readPos / (float) length * width;
        juce::Colour playheadColor;

        if (isRecording)
            playheadColor = LooperTheme::Colors::red; // Red when recording
        else
            playheadColor = LooperTheme::Colors::cyan; // Cyan when playing

        // Glow effect
        for (int i = 5; i > 0; --i)
        {
            g.setColour (playheadColor.withAlpha (0.05f * i));
            g.drawLine (playheadX, 0, playheadX, (float) height, (float) (i * 2));
        }

        // Solid playhead line
        g.setColour (playheadColor);
        g.drawLine (playheadX, 0, playheadX, (float) height, 2.0f);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinearRenderer)
};
