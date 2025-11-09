#pragma once

#include "audio/EngineCommandBus.h"
#include "engine/Metronome.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/BeatIndicatorComponent.h"
#include "ui/components/DraggableToggleButtonComponent.h"
#include "ui/components/DraggableValueLabelComponent.h"
#include "ui/components/LevelComponent.h"
#include <JuceHeader.h>

class MetronomeComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    MetronomeComponent (EngineMessageBus* engineMessageBus, Metronome* m)
        : uiToEngineBus (engineMessageBus)
        , metronomeLevel (engineMessageBus, -1, "Level", EngineMessageBus::CommandType::SetMetronomeVolume)
        , beatIndicator (engineMessageBus, m)
    {
        metronomeLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        metronomeLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (metronomeLabel);

        bpmLabel.setText ("BPM", juce::dontSendNotification);
        bpmLabel.setFont (LooperTheme::Fonts::getRegularFont (10.0f));
        bpmLabel.setJustificationType (juce::Justification::centred);
        bpmLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (bpmLabel);

        accentLabel.setText ("Accent", juce::dontSendNotification);
        accentLabel.setFont (LooperTheme::Fonts::getRegularFont (10.0f));
        accentLabel.setJustificationType (juce::Justification::centred);
        accentLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (accentLabel);

        enableButton.setButtonText ("Enable");
        enableButton.setColour (juce::TextButton::buttonColourId, LooperTheme::Colors::surface);
        enableButton.setColour (juce::TextButton::buttonOnColourId, LooperTheme::Colors::green);
        enableButton.setColour (juce::TextButton::textColourOffId, LooperTheme::Colors::textDim);
        enableButton.setColour (juce::TextButton::textColourOnId, LooperTheme::Colors::background);
        enableButton.onClick = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleMetronomeEnabled,
                                                                    -1,
                                                                    std::monostate {} });
        };
        addAndMakeVisible (enableButton);

        // BPM Editor
        bpmEditor.setText ("120", juce::dontSendNotification);
        bpmEditor.setFont (LooperTheme::Fonts::getBoldFont (13.0f));
        bpmEditor.setColour (juce::Label::textColourId, LooperTheme::Colors::text);
        bpmEditor.setEditable (true);
        bpmEditor.setJustificationType (juce::Justification::centred);
        bpmEditor.onTextChange = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetMetronomeBPM,
                                                                    -1,
                                                                    bpmEditor.getText().getIntValue() });
        };
        addAndMakeVisible (bpmEditor);

        // Numerator (beats per measure)
        numeratorEditor.setText ("4", juce::dontSendNotification);
        numeratorEditor.setFont (LooperTheme::Fonts::getBoldFont (13.0f));
        numeratorEditor.setColour (juce::Label::textColourId, LooperTheme::Colors::text);
        numeratorEditor.setEditable (true);
        numeratorEditor.setJustificationType (juce::Justification::centred);
        numeratorEditor.onTextChange = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command {
                EngineMessageBus::CommandType::SetMetronomeTimeSignature,
                -1,
                std::pair<int, int> { numeratorEditor.getText().getIntValue(), denominatorEditor.getText().getIntValue() } });
        };
        addAndMakeVisible (numeratorEditor);

        // Denominator (note value)
        denominatorEditor.setText ("4", juce::dontSendNotification);
        denominatorEditor.setFont (LooperTheme::Fonts::getBoldFont (13.0f));
        denominatorEditor.setColour (juce::Label::textColourId, LooperTheme::Colors::text);
        denominatorEditor.setEditable (true);
        denominatorEditor.setJustificationType (juce::Justification::centred);
        denominatorEditor.onTextChange = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command {
                EngineMessageBus::CommandType::SetMetronomeTimeSignature,
                -1,
                std::pair<int, int> { numeratorEditor.getText().getIntValue(), denominatorEditor.getText().getIntValue() } });
        };
        addAndMakeVisible (denominatorEditor);

        strongBeatButton.setMaxValue (numeratorEditor.getText().getIntValue());
        strongBeatButton.setColour (juce::TextButton::textColourOffId, LooperTheme::Colors::textDim);
        strongBeatButton.setColour (juce::TextButton::textColourOnId, LooperTheme::Colors::background);
        strongBeatButton.onValueChanged = [this] (int currentValue)
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetMetronomeStrongBeat,
                                                                    -1,
                                                                    currentValue });
        };
        addAndMakeVisible (strongBeatButton);

        addAndMakeVisible (metronomeLevel);

        addAndMakeVisible (beatIndicator);

        uiToEngineBus->addListener (this);
    }

    ~MetronomeComponent() override { uiToEngineBus->removeListener (this); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));

        auto titleBounds = metronomeLabel.getBounds();
        g.drawLine (titleBounds.getX() + 3.0f,
                    titleBounds.getBottom() + 3.0f,
                    titleBounds.getRight() - 3.0f,
                    titleBounds.getBottom() + 3.0f,
                    1.0f);

        auto timeSigBounds = numeratorEditor.getBounds().getUnion (denominatorEditor.getBounds());
        const float lineThickness = 2.0f;
        const float lineWidth = timeSigBounds.getWidth() * 0.5f;
        const float lineY = (numeratorEditor.getBottom() + denominatorEditor.getY()) * 0.5f;
        const float lineX = timeSigBounds.getCentreX() - (lineWidth * 0.5f);
        g.drawLine (lineX, lineY, lineX + lineWidth, lineY, lineThickness);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::column;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;
        mainBox.justifyContent = juce::FlexBox::JustifyContent::center;

        mainBox.items.add (juce::FlexItem (metronomeLabel).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        juce::FlexBox layoutBox;
        layoutBox.flexDirection = juce::FlexBox::Direction::row;
        layoutBox.alignItems = juce::FlexBox::AlignItems::stretch;
        layoutBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        layoutBox.items.add (juce::FlexItem (enableButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        juce::FlexBox bpmBox;
        bpmBox.flexDirection = juce::FlexBox::Direction::column;
        bpmBox.alignItems = juce::FlexBox::AlignItems::stretch;
        bpmBox.items.add (juce::FlexItem (bpmLabel).withFlex (0.5f).withMargin (juce::FlexItem::Margin (1, 1, 1, 1)));
        bpmBox.items.add (juce::FlexItem (bpmEditor).withFlex (1.0f).withMargin (juce::FlexItem::Margin (1, 1, 1, 1)));

        layoutBox.items.add (juce::FlexItem (bpmBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        juce::FlexBox timeSigBox;
        timeSigBox.flexDirection = juce::FlexBox::Direction::column;
        timeSigBox.alignItems = juce::FlexBox::AlignItems::stretch;
        timeSigBox.items.add (juce::FlexItem (numeratorEditor).withFlex (1.0f).withMargin (juce::FlexItem::Margin (1, 1, 1, 1)));
        timeSigBox.items.add (juce::FlexItem (denominatorEditor).withFlex (1.0f).withMargin (juce::FlexItem::Margin (1, 1, 1, 1)));

        layoutBox.items.add (juce::FlexItem (timeSigBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        juce::FlexBox accentBox;
        accentBox.flexDirection = juce::FlexBox::Direction::column;
        accentBox.alignItems = juce::FlexBox::AlignItems::stretch;
        accentBox.items.add (juce::FlexItem (accentLabel).withFlex (0.5f).withMargin (juce::FlexItem::Margin (1, 1, 1, 1)));
        accentBox.items.add (juce::FlexItem (strongBeatButton).withFlex (1.0f).withMargin (juce::FlexItem::Margin (1, 1, 1, 1)));

        layoutBox.items.add (juce::FlexItem (accentBox).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        layoutBox.items.add (juce::FlexItem (metronomeLevel).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        layoutBox.items.add (juce::FlexItem (beatIndicator).withFlex (1.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));

        mainBox.items.add (juce::FlexItem (layoutBox).withFlex (3.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        mainBox.performLayout (bounds.toFloat());
    }

private:
    EngineMessageBus* uiToEngineBus;
    juce::TextButton enableButton;

    juce::Label metronomeLabel { "Metronome", "Metronome" };
    juce::Label bpmLabel;
    juce::Label accentLabel;
    DraggableValueLabel bpmEditor { 30, 300, 1 };

    DraggableValueLabel numeratorEditor { 1, 16, 1 };
    DraggableValueLabel denominatorEditor { 1, 16, 1 };

    DraggableToggleButtonComponent strongBeatButton;

    LevelComponent metronomeLevel;

    BeatIndicatorComponent beatIndicator;

    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::MetronomeEnabledChanged,
                                                                        EngineMessageBus::EventType::MetronomeBPMChanged,
                                                                        EngineMessageBus::EventType::MetronomeTimeSignatureChanged,
                                                                        EngineMessageBus::EventType::MetronomeStrongBeatChanged,
                                                                        EngineMessageBus::EventType::MetronomeVolumeChanged };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::MetronomeEnabledChanged:
            {
                bool isEnabled = std::get<bool> (event.data);
                enableButton.setToggleState (isEnabled, juce::dontSendNotification);
                break;
            }
            case EngineMessageBus::EventType::MetronomeBPMChanged:
            {
                int bpm = std::get<int> (event.data);
                bpmEditor.setText (juce::String (bpm), juce::dontSendNotification);
                break;
            }
            case EngineMessageBus::EventType::MetronomeTimeSignatureChanged:
            {
                if (std::holds_alternative<std::pair<int, int>> (event.data))
                {
                    auto timeSig = std::get<std::pair<int, int>> (event.data);
                    numeratorEditor.setText (juce::String (timeSig.first), juce::dontSendNotification);
                    denominatorEditor.setText (juce::String (timeSig.second), juce::dontSendNotification);
                    strongBeatButton.setMaxValue (timeSig.first);
                }
                return;
            }
            case EngineMessageBus::EventType::MetronomeStrongBeatChanged:
            {
                int strongBeat = std::get<int> (event.data);
                strongBeatButton.setCurrentValue (strongBeat);
                strongBeatButton.setToggleState (strongBeat > 0, juce::dontSendNotification);

                break;
            }
            case EngineMessageBus::EventType::MetronomeVolumeChanged:
            {
                float volume = std::get<float> (event.data);
                metronomeLevel.setValue ((double) volume, juce::dontSendNotification);
                break;
            }
            default:
                throw juce::String ("Unhandled event type in MetronomeComponent" + juce::String (static_cast<int> (event.type)));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetronomeComponent)
};
