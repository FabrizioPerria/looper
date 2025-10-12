#include "PluginEditor.h"
#include "LooperLookAndFeel.h"
#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    lookAndFeel = std::make_unique<LooperLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    auto& engine = processorRef.getLooperEngine();

    for (int i = 0; i < engine.getNumTracks(); ++i)
    {
        if (auto* bridge = engine.getUIBridgeByIndex (i))
        {
            std::unique_ptr<TrackComponent> trackComp = std::make_unique<TrackComponent> (engine, i, bridge);
            trackComponents.push_back (std::move (trackComp));
            addAndMakeVisible (*trackComponents.back());
        }
    }

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
    auto bounds = getLocalBounds();
    bounds.reduce (LooperTheme::Dimensions::padding, LooperTheme::Dimensions::padding);

    // Stack tracks vertically
    for (auto& track : trackComponents)
    {
        track->setBounds (bounds.removeFromTop (200));
        bounds.removeFromTop (LooperTheme::Dimensions::spacing);
    }
}
