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
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour (LooperTheme::Colors::backgroundLight);
    g.fillRoundedRectangle (bounds, LooperTheme::Dimensions::cornerRadius);

    // Subtle gradient overlay
    juce::ColourGradient gradient (LooperTheme::Colors::backgroundLight.withAlpha (0.8f),
                                   bounds.getX(),
                                   bounds.getY(),
                                   LooperTheme::Colors::backgroundDark.withAlpha (0.8f),
                                   bounds.getX(),
                                   bounds.getBottom(),
                                   false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, LooperTheme::Dimensions::cornerRadius);

    // Border with subtle glow
    g.setColour (LooperTheme::Colors::primary.withAlpha (0.1f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), LooperTheme::Dimensions::cornerRadius, 2.0f);

    g.setColour (LooperTheme::Colors::border);
    g.drawRoundedRectangle (bounds.reduced (1), LooperTheme::Dimensions::cornerRadius, LooperTheme::Dimensions::borderWidth);
}

void TrackComponent::resized()
{
    using namespace LooperTheme;

    auto bounds = getLocalBounds();
    bounds.reduce (Dimensions::padding, Dimensions::padding);

    // Create main vertical flex container
    juce::FlexBox mainFlex;
    mainFlex.flexDirection = juce::FlexBox::Direction::column;
    mainFlex.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // Control bar flex container
    juce::FlexBox controlsFlex;
    controlsFlex.flexDirection = juce::FlexBox::Direction::row;
    controlsFlex.alignItems = juce::FlexBox::AlignItems::center;
    controlsFlex.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    controlsFlex.items.add (juce::FlexItem (trackLabel)
                                .withMinWidth (100.0f)
                                .withMinHeight (Dimensions::labelHeight)
                                .withMargin (juce::FlexItem::Margin (0, Dimensions::spacing * 2, 0, 0)));

    controlsFlex.items.add (juce::FlexItem (undoButton)
                                .withMinWidth (70.0f)
                                .withMinHeight (Dimensions::buttonHeight)
                                .withMargin (juce::FlexItem::Margin (0, Dimensions::spacing, 0, 0)));

    controlsFlex.items.add (juce::FlexItem (redoButton)
                                .withMinWidth (70.0f)
                                .withMinHeight (Dimensions::buttonHeight)
                                .withMargin (juce::FlexItem::Margin (0, Dimensions::spacing, 0, 0)));

    controlsFlex.items.add (juce::FlexItem (clearButton)
                                .withMinWidth (70.0f)
                                .withMinHeight (Dimensions::buttonHeight)
                                .withMargin (juce::FlexItem::Margin (0, Dimensions::spacing * 3, 0, 0)));

    controlsFlex.items.add (juce::FlexItem (volumeSlider)
                                .withFlex (1.0f)
                                .withMinWidth (150.0f)
                                .withMinHeight (Dimensions::sliderHeight)
                                .withMargin (juce::FlexItem::Margin (0, Dimensions::spacing, 0, 0)));

    controlsFlex.items.add (juce::FlexItem (muteButton).withMinWidth (70.0f).withMinHeight (Dimensions::buttonHeight));

    // Add controls to main flex
    mainFlex.items.add (juce::FlexItem().withHeight (Dimensions::buttonHeight).withFlex (0));

    // Waveform takes remaining space
    mainFlex.items.add (juce::FlexItem (waveformDisplay)
                            .withFlex (1.0f)
                            .withMinHeight (100.0f)
                            .withMargin (juce::FlexItem::Margin (Dimensions::spacing, 0, 0, 0)));

    // Perform layout for controls first
    auto controlBounds = bounds.removeFromTop (Dimensions::buttonHeight);
    controlsFlex.performLayout (controlBounds.toFloat());

    // Perform layout for main container
    mainFlex.performLayout (bounds.toFloat());
}
