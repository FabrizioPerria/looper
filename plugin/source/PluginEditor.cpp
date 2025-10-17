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

    setSize (1200, 900); // Make it bigger to see
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (LooperTheme::Colors::backgroundDark);
}

void AudioPluginAudioProcessorEditor::resized()
{
    theme->editorComponent->setBounds (getLocalBounds());
}
