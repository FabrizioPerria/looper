#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "themes/mixer/StudioMixerLookAndFeel.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    mixerEditor = std::make_unique<StudioMixerEditor> (processorRef.getLooperEngine());
    lookAndFeel = std::make_unique<StudioMixerLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    auto& engine = processorRef.getLooperEngine();

    addAndMakeVisible (*mixerEditor);

    setSize (900, 600); // Make it bigger to see
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
    mixerEditor->setBounds (getLocalBounds());
}
