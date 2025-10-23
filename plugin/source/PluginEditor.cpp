#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    theme = ThemeFactory::createTheme ("Daw", processorRef.getLooperEngine());
    setLookAndFeel (theme->lookAndFeel.get());

    addAndMakeVisible (theme->editorComponent.get());

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
    theme->editorComponent->setBounds (getLocalBounds());
    cpuMonitorButton.setBounds (10, 10, 120, 30);
}
