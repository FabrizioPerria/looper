#pragma once
#include "audio/EngineCommandBus.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class LooperEngine;

class MidiMappingComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    MidiMappingComponent (MidiMappingManager* mappingManager, EngineMessageBus* messageBus);
    ~MidiMappingComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void handleEngineEvent (const EngineMessageBus::Event& event) override;

    void enableMidiMenu (bool enable) { setVisible (enable); }

private:
    struct MappingRowData
    {
        EngineMessageBus::CommandType command;
        juce::String displayName;
        juce::String category;
        bool isCCCommand;
    };

    class CategoryHeader : public juce::Component
    {
    public:
        CategoryHeader (const juce::String& name) : categoryName (name) {}

        void paint (juce::Graphics& g) override
        {
            g.fillAll (LooperTheme::Colors::backgroundDark);
            g.setColour (LooperTheme::Colors::cyan);
            g.setFont (LooperTheme::Fonts::getBoldFont());
            g.drawText (categoryName, getLocalBounds().withTrimmedLeft (10), juce::Justification::centredLeft);
        }

    private:
        juce::String categoryName;
    };

    class MappingRow : public juce::Component
    {
    public:
        MappingRow (MidiMappingComponent* parent, const MappingRowData& data) : parentComponent (parent), rowData (data)
        {
            addAndMakeVisible (learnButton);
            learnButton.setButtonText ("Learn");
            learnButton.onClick = [this]() { onLearnClicked(); };

            addAndMakeVisible (clearButton);
            clearButton.setButtonText ("Clear");
            clearButton.onClick = [this]() { onClearClicked(); };
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();

            g.fillAll (rowColor);

            g.setColour (LooperTheme::Colors::white);
            g.setFont (LooperTheme::Fonts::getRegularFont (13.0f));

            // Command name column (200px)
            auto commandRect = bounds.removeFromLeft (200);
            juce::String commandText = rowData.displayName;
            if (g.getCurrentFont().getStringWidth (commandText) > commandRect.getWidth() - 10)
            {
                while (g.getCurrentFont().getStringWidth (commandText + "...") > commandRect.getWidth() - 10 && commandText.length() > 0)
                    commandText = commandText.dropLastCharacters (1);
                commandText += "...";
            }
            g.drawText (commandText, commandRect.withTrimmedLeft (10), juce::Justification::centredLeft);

            // Type column (60px)
            auto typeRect = bounds.removeFromLeft (60);
            g.drawText (rowData.isCCCommand ? "CC" : "Note", typeRect, juce::Justification::centred);

            // Key column (60px)
            auto keyRect = bounds.removeFromLeft (60);
            juce::String keyText = isLearning ? "--" : getMappingString();
            g.drawText (keyText, keyRect, juce::Justification::centred);

            // Waiting text for learning - AFTER buttons area
            if (isLearning)
            {
                bounds.removeFromLeft (100); // Skip buttons area
                g.setColour (LooperTheme::Colors::orange);
                g.drawText ("Waiting for MIDI input...", bounds, juce::Justification::centredLeft);
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            bounds.removeFromLeft (200 + 60 + 60); // Skip to buttons area

            learnButton.setBounds (bounds.removeFromLeft (80).reduced (2));
            clearButton.setBounds (bounds.removeFromLeft (80).reduced (2));
        }

        void setLearning (bool learning)
        {
            isLearning = learning;
            learnButton.setButtonText (learning ? "Cancel" : "Learn");
            rowColor = learning ? LooperTheme::Colors::orange.darker() : LooperTheme::Colors::backgroundDark;
            clearButton.setVisible (! learning);
            refresh();
        }

        void refresh()
        {
            repaint();
            if (auto* viewport = findParentComponentOfClass<juce::Viewport>())
            {
                viewport->repaint (getLocalBounds());
            }
        }

        EngineMessageBus::CommandType getCommandType() const { return rowData.command; }

    private:
        void onLearnClicked()
        {
            if (isLearning)
            {
                parentComponent->cancelLearning();
            }
            else
            {
                parentComponent->startLearning (this);
            }
        }

        void onClearClicked() { parentComponent->clearMapping (rowData.command); }

        juce::String getMappingString()
        {
            auto* manager = parentComponent->getMappingManager();
            if (! manager) return "--";

            if (rowData.isCCCommand)
            {
                for (uint8_t i = 0; i < 128; ++i)
                {
                    if (manager->getControlChangeId (i) == rowData.command) return juce::String (i);
                }
            }
            else
            {
                for (uint8_t i = 0; i < 128; ++i)
                {
                    if (manager->getCommandForNoteOn (i) == rowData.command) return juce::String (i);
                }
            }
            return "--";
        }

        bool getToggleState() const { return (reinterpret_cast<std::uintptr_t> (this) / 32) % 2 == 0; }

        MidiMappingComponent* parentComponent;
        MappingRowData rowData;
        juce::TextButton learnButton;
        juce::TextButton clearButton;
        std::atomic<bool> isLearning { false };
        juce::Colour rowColor { LooperTheme::Colors::backgroundDark };
    };

    class MidiActivityIndicator : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override
        {
            g.fillAll (LooperTheme::Colors::backgroundDark);
            g.setColour (LooperTheme::Colors::green);
            g.setFont (LooperTheme::Fonts::getRegularFont (13.0f));
            g.drawText (lastMidiMessage, getLocalBounds().withTrimmedLeft (10), juce::Justification::centredLeft);
        }

        void setMidiMessage (const juce::MidiMessage& message)
        {
            lastMidiMessage = "Last MIDI: " + message.getDescription();
            repaint();
        }

    private:
        juce::String lastMidiMessage = "No MIDI activity";
    };

    void buildMappingList();
    void startLearning (MappingRow* row);
    void cancelLearning();
    void clearMapping (EngineMessageBus::CommandType command);
    void refreshAllRows();

    MidiMappingManager* midiMappingManager;
    EngineMessageBus* uiToEngineBus;

    MidiMappingManager* getMappingManager() const;

    juce::Viewport viewport;
    juce::Component contentComponent;
    MidiActivityIndicator activityIndicator;

    juce::TextButton saveButton;
    juce::TextButton loadButton;
    juce::TextButton resetButton;

    std::vector<MappingRowData> mappingData;
    juce::OwnedArray<CategoryHeader> categoryHeaders;
    juce::OwnedArray<MappingRow> mappingRows;

    MappingRow* currentLearningRow = nullptr;

    std::atomic<bool> midiMenuEnabled { false };
    int lastLearningSessionId = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMappingComponent)
};
