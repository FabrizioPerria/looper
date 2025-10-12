#include "TrackComponent.h"

TrackComponent::TrackComponent (LooperEngine& engine, int trackIdx, AudioToUIBridge* bridge) : looperEngine (engine), trackIndex (trackIdx)
{
    trackLabel.setFont (juce::Font (16.0f, juce::Font::bold));
    trackLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (trackLabel);

    undoButton.onClick = [this]() { looperEngine.undo(); };
    addAndMakeVisible (undoButton);

    redoButton.onClick = [this]() { looperEngine.redo(); };
    addAndMakeVisible (redoButton);

    clearButton.onClick = [this]() { looperEngine.clear(); };
    addAndMakeVisible (clearButton);

    volumeSlider.setRange (0.0, 1.0, 0.01);
    volumeSlider.onValueChange = [this]() { looperEngine.setTrackVolume (trackIndex, (float) volumeSlider.getValue()); };
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
    volumeSlider.setSkewFactorFromMidPoint (0.5); // More precision at lower volumes
    addAndMakeVisible (volumeSlider);

    muteButton.onClick = [this]() { looperEngine.setTrackMuted (trackIndex, muteButton.getToggleState()); };
    addAndMakeVisible (muteButton);

    waveformDisplay.setBridge (bridge);
    addAndMakeVisible (waveformDisplay);

    updateControlsFromEngine();
    startTimerHz (10);
}

TrackComponent::~TrackComponent()
{
    stopTimer();
}

void TrackComponent::timerCallback()
{
    updateControlsFromEngine();
}

void TrackComponent::updateControlsFromEngine()
{
    auto* track = looperEngine.getTrackByIndex (trackIndex);
    if (! track) return;

    // Update volume slider if it changed (avoid feedback loop)
    float currentVolume = track->getTrackVolume();
    if (std::abs (volumeSlider.getValue() - currentVolume) > 0.001)
    {
        volumeSlider.setValue (currentVolume, juce::dontSendNotification);
    }

    // Update mute button
    bool currentMuted = track->isMuted();
    if (muteButton.getToggleState() != currentMuted)
    {
        muteButton.setToggleState (currentMuted, juce::dontSendNotification);
    }
}

void TrackComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey.darker (0.2f));
    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds(), 1);
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
    flexbox.items.add (juce::FlexItem (waveformDisplay)
                           .withFlex (1.0f)
                           .withMinWidth (200.0f)
                           .withHeight (100.0f)
                           .withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
    flexbox.performLayout (getLocalBounds().toFloat());
}
