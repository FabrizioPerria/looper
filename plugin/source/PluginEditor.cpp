#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    int numTracks = processorRef.getLooperEngine().getNumTracks();
    for (int i = 1; i < numTracks; ++i)
    {
        auto trackComp = std::make_unique<TrackComponent> ("Track " + std::to_string (i + 1));
        trackComp->setLoopTrack (processorRef.getLooperEngine().getTrack (i));
        trackComponents.push_back (std::move (trackComp));
        addAndMakeVisible (*trackComponents.back());
    }
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
    juce::FlexBox box;
    box.flexDirection = juce::FlexBox::Direction::column;
    box.flexWrap = juce::FlexBox::Wrap::noWrap;
    box.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    for (auto& trackComp : trackComponents)
    {
        box.items.add (juce::FlexItem (*trackComp).withFlex (1.0f).withMargin (5.0f));
        trackComp->resized();
    }
}
