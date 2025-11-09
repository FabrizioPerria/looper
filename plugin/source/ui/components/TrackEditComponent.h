#pragma once

#include "audio/EngineCommandBus.h"
#include "engine/Constants.h"
#include <JuceHeader.h>

class TrackEditComponent : public juce::Component, public EngineMessageBus::Listener
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

        syncButton.setButtonText ("SYNC");
        syncButton.setComponentID ("sync");
        syncButton.setToggleState (DEFAULT_TRACK_SYNCED, juce::dontSendNotification);
        syncButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleSyncTrack, trackIndex, {} }); };
        addAndMakeVisible (syncButton);
        uiToEngineBus->addListener (this);
    }
    ~TrackEditComponent() override { uiToEngineBus->removeListener (this); }

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

        juce::FlexBox clearSyncBox;
        clearSyncBox.flexDirection = juce::FlexBox::Direction::column;
        clearSyncBox.items.add (juce::FlexItem (clearButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 1, 1)));
        clearSyncBox.items.add (juce::FlexItem (syncButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (1, 1, 0, 1)));

        flexBox.items.add (juce::FlexItem (undoredoBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        flexBox.items.add (juce::FlexItem (clearSyncBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        flexBox.performLayout (bounds.toFloat());
    }

private:
    juce::TextButton undoButton { "UNDO" };
    juce::TextButton redoButton { "REDO" };
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton syncButton { "SYNC" };

    EngineMessageBus* uiToEngineBus;
    int trackIndex;

    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::TrackSyncChanged };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        if (event.trackIndex != trackIndex) return;
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::TrackSyncChanged:
                if (std::holds_alternative<bool> (event.data))
                {
                    bool isSynced = std::get<bool> (event.data);
                    syncButton.setToggleState (isSynced, juce::dontSendNotification);
                }
                break;
        }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackEditComponent)
};
