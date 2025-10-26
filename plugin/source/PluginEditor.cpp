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

    cpuMonitorButton.setButtonText ("CPU Monitor");
    cpuMonitorButton.onClick = [this] { openCPUMonitor(); };
    addAndMakeVisible (cpuMonitorButton);

    setSize (1200, 900); // Make it bigger to see
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    cpuMonitorWindow.reset();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g) { g.fillAll (LooperTheme::Colors::backgroundDark); }

void AudioPluginAudioProcessorEditor::resized()
{
    looperEditor->setBounds (getLocalBounds());
    cpuMonitorButton.setBounds (10, 10, 120, 30);
}
