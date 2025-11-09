#pragma once

#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/MeterWithGainComponent.h"
#include <JuceHeader.h>

class FooterComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    FooterComponent (EngineMessageBus* engineMessageBus, EngineStateToUIBridge* bridge)
        : inputMeter ("IN", engineMessageBus, bridge), outputMeter ("OUT", engineMessageBus, bridge), uiToEngineBus (engineMessageBus)
    {
        addAndMakeVisible (inputMeter);
        addAndMakeVisible (outputMeter);

        midiButton.setClickingTogglesState (true);
        midiButton.onClick = [this]()
        {
            uiToEngineBus->broadcastEvent (EngineMessageBus::Event (EngineMessageBus::EventType::MidiMenuEnabledChanged,
                                                                    -1,
                                                                    midiButton.getToggleState()));
        };
        midiLabel.setText ("Midi", juce::dontSendNotification);
        midiLabel.setJustificationType (juce::Justification::centred);
        midiLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (midiLabel);

        midiButton.setButtonText ("Settings");
        midiButton.setComponentID ("midi");
        addAndMakeVisible (midiButton);

        playModeLabel.setText ("Play Mode", juce::dontSendNotification);
        playModeLabel.setJustificationType (juce::Justification::centred);
        playModeLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (playModeLabel);

        playModeButton.setButtonText ("Single Track");
        playModeButton.setComponentID ("single");
        playModeButton.setToggleState (true, juce::dontSendNotification);
        playModeButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleSinglePlayMode, -1, {} }); };
        addAndMakeVisible (playModeButton);

        uiToEngineBus->addListener (this);
    }

    ~FooterComponent() override { uiToEngineBus->removeListener (this); }

    void resized() override
    {
        auto bounds = getLocalBounds();
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::row;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        mainBox.items.add (juce::FlexItem (inputMeter).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        mainBox.items.add (juce::FlexItem().withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        juce::FlexBox midiBox;
        midiBox.flexDirection = juce::FlexBox::Direction::column;
        midiBox.alignItems = juce::FlexBox::AlignItems::stretch;
        midiBox.items.add (juce::FlexItem (midiLabel).withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        midiBox.items.add (juce::FlexItem (midiButton).withFlex (0.9f).withMargin (juce::FlexItem::Margin (6, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (midiBox).withFlex (0.2f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        mainBox.items.add (juce::FlexItem().withFlex (0.2f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));

        juce::FlexBox playModeBox;
        playModeBox.flexDirection = juce::FlexBox::Direction::column;
        playModeBox.alignItems = juce::FlexBox::AlignItems::stretch;
        playModeBox.items.add (juce::FlexItem (playModeLabel).withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        playModeBox.items.add (juce::FlexItem (playModeButton).withFlex (0.9f).withMargin (juce::FlexItem::Margin (6, 1, 0, 1)));
        mainBox.items.add (juce::FlexItem (playModeBox).withFlex (0.2f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        mainBox.items.add (juce::FlexItem().withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 0, 1)));
        mainBox.items.add (juce::FlexItem (outputMeter).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 0)));

        mainBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (LooperTheme::Colors::surface);

        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));

        auto midiTitleBounds = midiLabel.getBounds();
        g.fillRect (midiTitleBounds.getX() + 3.0f, midiTitleBounds.getBottom() + 3.0f, midiTitleBounds.getWidth() - 6.0f, 1.0f);

        auto playModeTitleBounds = playModeLabel.getBounds();
        g.fillRect (playModeTitleBounds.getX() + 3.0f, playModeTitleBounds.getBottom() + 3.0f, playModeTitleBounds.getWidth() - 6.0f, 1.0f);
    }

private:
    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::SinglePlayModeChanged };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::SinglePlayModeChanged:
            {
                bool isSinglePlayMode = std::get<bool> (event.data);
                playModeButton.setToggleState (isSinglePlayMode, juce::dontSendNotification);
                break;
            }
            default:
                throw juce::String ("Unhandled event type in TransportControlsComponent" + juce::String (static_cast<int> (event.type)));
        }
    }
    MeterWithGainComponent inputMeter;
    MeterWithGainComponent outputMeter;
    juce::TextButton midiButton { "MIDI Settings" };
    juce::TextButton playModeButton { "Single Track" };

    EngineMessageBus* uiToEngineBus;

    juce::Label midiLabel;
    juce::Label playModeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterComponent)
};
