#pragma once

#include "LoopTrack.h"
#include "WaveformCache.h"
#include <JuceHeader.h>

class WaveformComponent : public juce::Component, public juce::Timer
{
public:
    WaveformComponent();
    ~WaveformComponent() override;

    void setLoopTrack (LoopTrack* track)
    {
        if (! track) return;
        loopTrack = track;
    }

    void paint (juce::Graphics& g) override;
    void timerCallback() override;

private:
    void getMinMaxForPixel (int pixelIndex, float& min, float& max);
    void paintFromCache (juce::Graphics& g, size_t readPos);
    void paintDirect (juce::Graphics& g, size_t readPos);
    void drawCRTEffects (juce::Graphics& g, int readPixel, int width, int height);
    juce::Colour getWaveformColour (int x, int readPixel);
    void drawWaveformColumn (juce::Graphics& g, int x, float min, float max, int readPixel, int height);
    LoopTrack* loopTrack = nullptr;
    WaveformCache cache;
    juce::ThreadPool pool { 1 }; // Single background thread for updates

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformComponent)
};
