#pragma once

#include "ui/colors/TokyoNight.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class TrackEditComponent : public juce::Component
{
public:
    TrackEditComponent (MidiCommandDispatcher* dispatcher, int trackIdx) : midiDispatcher (dispatcher), trackIndex (trackIdx)
    {
        undoButton.setButtonText ("UNDO");
        undoButton.setComponentID ("undo");
        undoButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::UNDO_BUTTON, trackIndex); };
        addAndMakeVisible (undoButton);

        redoButton.setButtonText ("REDO");
        redoButton.setComponentID ("redo");
        redoButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::REDO_BUTTON, trackIndex); };
        addAndMakeVisible (redoButton);

        clearButton.setButtonText ("CLEAR");
        clearButton.setComponentID ("clear");
        clearButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::CLEAR_BUTTON, trackIndex); };
        addAndMakeVisible (clearButton);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);

        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::row;
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;

        flexBox.items.add (juce::FlexItem (undoButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        flexBox.items.add (juce::FlexItem (redoButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        flexBox.items.add (juce::FlexItem (clearButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        flexBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));
        g.drawLine (bounds.getX(), bounds.getY() + 8, bounds.getX(), bounds.getBottom() - 8, 1.0f);
        g.drawLine (bounds.getRight() - 1, bounds.getY() + 8, bounds.getRight() - 1, bounds.getBottom() - 8, 1.0f);
    }

private:
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::TextButton clearButton { "CLEAR" };

    MidiCommandDispatcher* midiDispatcher;
    int trackIndex;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackEditComponent)
};
