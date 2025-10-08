#pragma once
#include "PluginProcessor.h"
#include "WaveformComponent.h"
#include <JuceHeader.h>

//==============================================================================
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& processorRef;
    WaveformComponent waveformDisplay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
