#pragma once

#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/DraggableToggleButtonComponent.h"
#include "ui/components/DraggableValueLabelComponent.h"
#include "ui/components/LevelComponent.h"
#include <JuceHeader.h>

class MetronomeComponent : public juce::Component
{
public:
    MetronomeComponent (EngineMessageBus* engineMessageBus)
        : uiToEngineBus (engineMessageBus), metronomeLevel (engineMessageBus, -1, "Level", 11)
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
        enableButton.setClickingTogglesState (true);
        enableButton.setColour (juce::TextButton::buttonColourId, LooperTheme::Colors::surface);
        enableButton.setColour (juce::TextButton::buttonOnColourId, LooperTheme::Colors::green);
        enableButton.setColour (juce::TextButton::textColourOffId, LooperTheme::Colors::textDim);
        enableButton.setColour (juce::TextButton::textColourOnId, LooperTheme::Colors::background);
        enableButton.onClick = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetMetronomeEnabled,
                                                                    -1,
                                                                    enableButton.getToggleState() });
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
        strongBeatButton.onClick = [this]()
        {
            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetMetronomeStrongBeat,
                                                                    -1,
                                                                    strongBeatButton.getCurrentValue() });
        };
        addAndMakeVisible (strongBeatButton);

        addAndMakeVisible (metronomeLevel);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));

        auto bounds = getLocalBounds().toFloat();
        g.drawLine (bounds.getX(), bounds.getY() + 8, bounds.getX(), bounds.getBottom() - 8, 1.0f);
        g.drawLine (bounds.getRight() - 1, bounds.getY() + 8, bounds.getRight() - 1, bounds.getBottom() - 8, 1.0f);

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

        mainBox.items.add (juce::FlexItem (layoutBox).withFlex (3.0f).withMargin (juce::FlexItem::Margin (2, 2, 2, 2)));
        mainBox.performLayout (bounds.toFloat());
    }

    bool isEnabled() const { return enableButton.getToggleState(); }
    int getBpm() const { return bpmEditor.getText().getIntValue(); }
    int getNumerator() const { return numeratorEditor.getText().getIntValue(); }
    int getDenominator() const { return denominatorEditor.getText().getIntValue(); }
    bool getStrongBeatEnabled() const { return strongBeatButton.getToggleState(); }

    // Setters for external control
    void setEnabled (bool enabled) { enableButton.setToggleState (enabled, juce::dontSendNotification); }
    void setBpm (int bpm) { bpmEditor.setText (juce::String (juce::jlimit (30, 300, bpm)), juce::dontSendNotification); }
    void setTimeSig (int num, int denom)
    {
        numeratorEditor.setText (juce::String (num), juce::dontSendNotification);
        denominatorEditor.setText (juce::String (denom), juce::dontSendNotification);
    }
    void setStrongBeatEnabled (bool enabled) { strongBeatButton.setToggleState (enabled, juce::dontSendNotification); }

private:
    EngineMessageBus* uiToEngineBus;
    juce::TextButton enableButton;

    juce::Label metronomeLabel { "Metronome", "Metronome" };
    juce::Label bpmLabel;
    juce::Label accentLabel;
    DraggableValueLabel bpmEditor { 30, 300, 1 };

    DraggableValueLabel numeratorEditor { 1, 16, 1 };
    DraggableValueLabel denominatorEditor { 1, 16, 1 };

    DraggableToggleButtonComponent strongBeatButton { 1 };

    LevelComponent metronomeLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetronomeComponent)
};
