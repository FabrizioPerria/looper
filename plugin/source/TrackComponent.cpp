#include "TrackComponent.h"

TrackComponent::TrackComponent (const juce::String& name, LoopTrack* looptrack) : trackLabel (name), loopTrack (looptrack)
{
    addAndMakeVisible (trackLabel);
    trackLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    trackLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (undoButton);
    undoButton.addListener (this);

    addAndMakeVisible (redoButton);
    redoButton.addListener (this);

    addAndMakeVisible (clearButton);
    clearButton.addListener (this);

    addAndMakeVisible (volumeSlider);
    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.setValue (loopTrack ? loopTrack->getTrackVolume() : 1.0);
    volumeSlider.onValueChange = [this]()
    {
        if (loopTrack) loopTrack->setTrackVolume ((float) volumeSlider.getValue());
    };
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    volumeSlider.setSkewFactorFromMidPoint (0.5); // More precision at lower volumes

    muteButton.setToggleState (loopTrack ? loopTrack->isMuted() : false, juce::dontSendNotification);
    addAndMakeVisible (muteButton);
    muteButton.addListener (this);

    waveformDisplay = std::make_unique<WaveformComponent> (loopTrack);
    addAndMakeVisible (*waveformDisplay);
}

TrackComponent::~TrackComponent()
{
    undoButton.removeListener (this);
    redoButton.removeListener (this);
    clearButton.removeListener (this);
    muteButton.removeListener (this);
}

void TrackComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey.darker (0.2f));
    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds(), 1);
}

void TrackComponent::buttonClicked (juce::Button* button)
{
    if (button == &undoButton)
    {
        if (loopTrack)
        {
            loopTrack->undo();
        }
    }
    else if (button == &redoButton)
    {
        if (loopTrack)
        {
            loopTrack->redo();
        }
    }
    else if (button == &clearButton)
    {
        if (loopTrack)
        {
            loopTrack->clear();
        }
    }
    else if (button == &muteButton)
    {
        if (loopTrack)
        {
            loopTrack->setMuted (muteButton.getToggleState());
        }
    }
}

void TrackComponent::resized()
{
    juce::FlexBox flexbox;
    flexbox.flexDirection = juce::FlexBox::Direction::row;
    flexbox.flexWrap = juce::FlexBox::Wrap::noWrap;
    flexbox.alignItems = juce::FlexBox::AlignItems::center;
    flexbox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    flexbox.items.add (juce::FlexItem (trackLabel).withFlex (1.0f).withMinWidth (100.0f).withHeight (24.0f));
    flexbox.items
        .add (juce::FlexItem (undoButton).withMinWidth (60.0f).withHeight (24.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
    flexbox.items
        .add (juce::FlexItem (redoButton).withMinWidth (60.0f).withHeight (24.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
    flexbox.items
        .add (juce::FlexItem (clearButton).withMinWidth (60.0f).withHeight (24.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
    flexbox.items
        .add (juce::FlexItem (volumeSlider).withMinWidth (150.0f).withHeight (24.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
    flexbox.items
        .add (juce::FlexItem (muteButton).withMinWidth (60.0f).withHeight (24.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
    flexbox.items.add (juce::FlexItem (*waveformDisplay)
                           .withFlex (1.0f)
                           .withMinWidth (200.0f)
                           .withHeight (100.0f)
                           .withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
    flexbox.performLayout (getLocalBounds().toFloat());
}
