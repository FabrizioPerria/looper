#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    looperEditor = std::make_unique<LooperEditor> (processorRef.getLooperEngine());
    lookAndFeel = std::make_unique<LooperLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    addAndMakeVisible (looperEditor.get());

    setSize (1200, 900);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() { setLookAndFeel (nullptr); }

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (LooperTheme::Colors::backgroundDark); }

void AudioPluginAudioProcessorEditor::resized() { looperEditor->setBounds (getLocalBounds()); }
