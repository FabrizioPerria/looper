#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (waveformDisplay);
    waveformDisplay.setLoopTrack (processorRef.getLooperEngine().getActiveTrack());
    waveformDisplay.setBridge (&processorRef.getUIBridge());
    setSize (800, 400); // Make it bigger to see
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
}

void AudioPluginAudioProcessorEditor::resized()
{
    waveformDisplay.setBounds (getLocalBounds());
}
