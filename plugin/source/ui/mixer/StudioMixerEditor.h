#pragma once
#include "LooperTheme.h"
#include "MixerChannelComponent.h"
#include <JuceHeader.h>

class StudioMixerEditor : public juce::Component
{
public:
    StudioMixerEditor (LooperEngine& engine) : looperEngine (engine)
    {
        for (int i = 1; i <= engine.getNumTracks(); ++i)
        {
            auto* channel = new MixerChannelComponent (engine, i - 1, engine.getUIBridgeByIndex (i - 1));
            channels.add (channel);
            addAndMakeVisible (channel);
        }

        // Transport buttons
        recordButton.setButtonText ("REC");
        recordButton.setClickingTogglesState (true);
        recordButton.onClick = [this]()
        {
            if (recordButton.getToggleState())
                looperEngine.startRecording();
            else
                looperEngine.stop();
        };
        addAndMakeVisible (recordButton);

        stopButton.setButtonText ("STOP");
        stopButton.onClick = [this]() { looperEngine.stop(); };
        addAndMakeVisible (stopButton);

        playButton.setButtonText ("PLAY");
        playButton.setClickingTogglesState (true);
        playButton.onClick = [this]()
        {
            if (playButton.getToggleState())
                looperEngine.startPlaying();
            else
                looperEngine.stop();
        };
        addAndMakeVisible (playButton);

        // Master section
        masterLabel.setText ("MASTER", juce::dontSendNotification);
        masterLabel.setFont (LooperTheme::Fonts::getBoldFont (12.0f));
        masterLabel.setJustificationType (juce::Justification::centred);
        masterLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (masterLabel);

        masterFader.setSliderStyle (juce::Slider::LinearVertical);
        masterFader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
        masterFader.setRange (0.0, 1.0, 0.01);
        masterFader.setValue (0.8);
        addAndMakeVisible (masterFader);
    }

    void paint (juce::Graphics& g) override
    {
        // Background
        g.fillAll (LooperTheme::Colors::backgroundDark);

        // Top bar
        auto topBar = getLocalBounds().removeFromTop (50);
        g.setColour (LooperTheme::Colors::surface);
        g.fillRect (topBar);

        // Bottom border for top bar
        g.setColour (LooperTheme::Colors::border);
        g.drawLine (0, 50, getWidth(), 50, 1.0f);

        // Draw title text
        g.setColour (LooperTheme::Colors::cyan);
        g.setFont (LooperTheme::Fonts::getTitleFont (18.0f));
        g.drawText ("LOOPER", juce::Rectangle<float> (12, 8, 150, 34), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Top bar with transport controls
        auto topBar = bounds.removeFromTop (50);
        topBar.reduce (12, 8);

        // Transport buttons in the center
        auto transportBounds = topBar.withSizeKeepingCentre (230, 34);

        juce::FlexBox transportFlex;
        transportFlex.flexDirection = juce::FlexBox::Direction::row;
        transportFlex.justifyContent = juce::FlexBox::JustifyContent::center;
        transportFlex.alignItems = juce::FlexBox::AlignItems::center;

        transportFlex.items
            .add (juce::FlexItem (recordButton).withWidth (70.0f).withHeight (34.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 0)));
        transportFlex.items
            .add (juce::FlexItem (stopButton).withWidth (70.0f).withHeight (34.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        transportFlex.items
            .add (juce::FlexItem (playButton).withWidth (70.0f).withHeight (34.0f).withMargin (juce::FlexItem::Margin (0, 0, 0, 4)));

        transportFlex.performLayout (transportBounds.toFloat());

        bounds.removeFromTop (8);
        bounds.reduce (8, 0);

        // Main horizontal flex for channels + master
        juce::FlexBox mainFlex;
        mainFlex.flexDirection = juce::FlexBox::Direction::row;
        mainFlex.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        mainFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        // Add all channels - let them stretch vertically
        for (auto* channel : channels)
        {
            mainFlex.items.add (juce::FlexItem (*channel).withWidth (140.0f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        }

        mainFlex.items.add (juce::FlexItem().withWidth (8.0f));

        // Master section (fixed width, flexible height)
        juce::FlexBox masterFlex;
        masterFlex.flexDirection = juce::FlexBox::Direction::column;
        masterFlex.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        masterFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        masterFlex.items.add (juce::FlexItem (masterLabel).withHeight (30.0f).withMargin (juce::FlexItem::Margin (0, 0, 8, 0)));

        // Master fader takes remaining space
        masterFlex.items.add (juce::FlexItem (masterFader).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 0, 8, 0)));

        mainFlex.items.add (juce::FlexItem().withWidth (120.0f));

        mainFlex.performLayout (bounds.toFloat());

        // Perform master section layout
        auto masterBounds = bounds.removeFromRight (120);
        masterFlex.performLayout (masterBounds.toFloat());
    }

private:
    LooperEngine& looperEngine;
    juce::OwnedArray<MixerChannelComponent> channels;

    juce::TextButton recordButton;
    juce::TextButton stopButton;
    juce::TextButton playButton;

    juce::Label masterLabel;
    juce::Slider masterFader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StudioMixerEditor)
};
