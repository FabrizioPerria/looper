#pragma once
#include "LoopTrack.h"
#include "WaveformComponent.h"
#include <JuceHeader.h>

class TrackComponent : public juce::Component, public juce::Button::Listener
{
public:
    TrackComponent (const juce::String& name, LoopTrack* track);
    ~TrackComponent() override;

    void paint (juce::Graphics& g) override;

    void resized() override;

private:
    juce::Label trackLabel;
    juce::TextButton undoButton { "Undo" };
    juce::TextButton redoButton { "Redo" };
    juce::TextButton clearButton { "Clear" };
    juce::Slider volumeSlider;
    juce::ToggleButton muteButton { "Mute" };

    std::unique_ptr<WaveformComponent> waveformDisplay;

    LoopTrack* loopTrack;

    void buttonClicked (juce::Button* button) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
