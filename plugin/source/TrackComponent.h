#pragma once
#include "LooperEngine.h"
#include "WaveformComponent.h"
#include <JuceHeader.h>

class TrackComponent : public juce::Component, public juce::Timer
{
public:
    TrackComponent (LooperEngine& engine, int trackIdx, AudioToUIBridge* bridge);
    ~TrackComponent();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateControlsFromEngine();

    LooperEngine& looperEngine;
    int trackIndex;
    WaveformComponent waveformDisplay;
    juce::Label trackLabel;
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton clearButton { "Clear" };
    juce::Slider volumeSlider;
    juce::ToggleButton muteButton { "Mute" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
