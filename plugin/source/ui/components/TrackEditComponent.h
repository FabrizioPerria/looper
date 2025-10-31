#pragma once

#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class TrackEditComponent : public juce::Component
{
public:
    TrackEditComponent (EngineMessageBus* engineMessageBus, int trackIdx) : uiToEngineBus (engineMessageBus), trackIndex (trackIdx)
    {
        undoButton.setButtonText ("UNDO");
        undoButton.setComponentID ("undo");
        undoButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::Undo, trackIndex, {} }); };
        addAndMakeVisible (undoButton);

        redoButton.setButtonText ("REDO");
        redoButton.setComponentID ("redo");
        redoButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::Redo, trackIndex, {} }); };
        addAndMakeVisible (redoButton);

        clearButton.setButtonText ("CLEAR");
        clearButton.setComponentID ("clear");
        clearButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::Clear, trackIndex, {} }); };
        addAndMakeVisible (clearButton);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);

        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::row;
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;

        juce::FlexBox undoredoBox;
        undoredoBox.flexDirection = juce::FlexBox::Direction::column;
        undoredoBox.items.add (juce::FlexItem (undoButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 1, 1)));
        undoredoBox.items.add (juce::FlexItem (redoButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (1, 1, 0, 1)));

        flexBox.items.add (juce::FlexItem (undoredoBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        flexBox.items.add (juce::FlexItem (clearButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));

        flexBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));
        g.drawLine (bounds.getX(), bounds.getY() + 8, bounds.getX(), bounds.getBottom() - 8, 1.0f);
        g.drawLine (bounds.getRight() - 1, bounds.getY() + 8, bounds.getRight() - 1, bounds.getBottom() - 8, 1.0f);
    }

private:
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::TextButton clearButton { "CLEAR" };

    EngineMessageBus* uiToEngineBus;
    int trackIndex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackEditComponent)
};
