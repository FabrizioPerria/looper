#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (waveformDisplay);
    waveformDisplay.setBridge (processorRef.getLooperEngine().getUIBridgeForTrack (processorRef.getLooperEngine().getActiveTrackIndex()));
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
