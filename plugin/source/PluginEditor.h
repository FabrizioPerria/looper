#pragma once
#include "PluginProcessor.h"
#include "ui/editor/DawLookAndFeel.h"
#include "ui/editor/LooperEditor.h"
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
    juce::TextButton cpuMonitorButton;
    AudioPluginAudioProcessor& processorRef;
    std::unique_ptr<LooperEditor> looperEditor;
    std::unique_ptr<LooperLookAndFeel> lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
