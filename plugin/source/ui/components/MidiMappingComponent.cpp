#include "MidiMappingComponent.h"
#include "engine/MidiCommandConfig.h"

MidiMappingComponent::MidiMappingComponent (MidiMappingManager* mappingManager, EngineMessageBus* messageBus)
    : midiMappingManager (mappingManager), uiToEngineBus (messageBus)
{
    buildMappingList();

    addAndMakeVisible (activityIndicator);

    viewport.setScrollBarsShown (true, false, true, false);
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&contentComponent, false);

    addAndMakeVisible (saveButton);
    saveButton.setButtonText ("Save Mapping");
    saveButton.onClick = [this]()
    {
        uiToEngineBus->pushCommand ({ .type = EngineMessageBus::CommandType::SaveMidiMappings,
                                      .trackIndex = -1,
                                      .payload = std::monostate {} });
    };

    addAndMakeVisible (loadButton);
    loadButton.setButtonText ("Load Mapping");
    loadButton.onClick = [this]()
    {
        uiToEngineBus->pushCommand ({ .type = EngineMessageBus::CommandType::LoadMidiMappings,
                                      .trackIndex = -1,
                                      .payload = std::monostate {} });
        refreshAllRows();
    };

    addAndMakeVisible (resetButton);
    resetButton.setButtonText ("Reset to Defaults");
    resetButton.onClick = [this]()
    {
        uiToEngineBus->pushCommand ({ .type = EngineMessageBus::CommandType::ResetMidiMappings,
                                      .trackIndex = -1,
                                      .payload = std::monostate {} });
        refreshAllRows();
    };

    uiToEngineBus->addListener (this);
}

MidiMappingComponent::~MidiMappingComponent() { uiToEngineBus->removeListener (this); }

void MidiMappingComponent::paint (juce::Graphics& g) { g.fillAll (LooperTheme::Colors::backgroundDark); }

void MidiMappingComponent::resized()
{
    auto bounds = getLocalBounds();

    activityIndicator.setBounds (bounds.removeFromTop (30));

    auto buttonArea = bounds.removeFromBottom (40);
    buttonArea.reduce (10, 5);
    saveButton.setBounds (buttonArea.removeFromLeft (120));
    buttonArea.removeFromLeft (10);
    loadButton.setBounds (buttonArea.removeFromLeft (120));
    buttonArea.removeFromLeft (10);
    resetButton.setBounds (buttonArea.removeFromLeft (150));

    viewport.setBounds (bounds.reduced (2, 0));

    int yPos = 0;
    int rowHeight = 30;
    int headerHeight = 25;

    // Interleave headers and rows by category
    juce::String currentCategory;
    size_t headerIndex = 0;
    size_t rowIndex = 0;

    for (const auto& data : mappingData)
    {
        if (data.category != currentCategory)
        {
            currentCategory = data.category;
            if (headerIndex < categoryHeaders.size())
            {
                categoryHeaders[headerIndex]->setBounds (0, yPos, viewport.getMaximumVisibleWidth(), headerHeight);
                yPos += headerHeight;
                headerIndex++;
            }
        }

        if (rowIndex < mappingRows.size())
        {
            mappingRows[rowIndex]->setBounds (0, yPos, viewport.getMaximumVisibleWidth(), rowHeight);
            yPos += rowHeight;
            rowIndex++;
        }
    }

    contentComponent.setSize (viewport.getMaximumVisibleWidth(), yPos);
}

void MidiMappingComponent::handleEngineEvent (const EngineMessageBus::Event& event)
{
    if (event.type == EngineMessageBus::EventType::MidiMappingChanged)
    {
        if (currentLearningRow && std::holds_alternative<int> (event.data))
        {
            int learningSessionId = std::get<int> (event.data);
            if (learningSessionId <= lastLearningSessionId) return; // Ignore duplicate events
            currentLearningRow->setLearning (false);
            currentLearningRow = nullptr;
            lastLearningSessionId = learningSessionId;
        }
        refreshAllRows();
    }
    else if (event.type == EngineMessageBus::EventType::MidiActivityReceived)
    {
        if (std::holds_alternative<juce::MidiMessage> (event.data))
        {
            activityIndicator.setMidiMessage (std::get<juce::MidiMessage> (event.data));
        }
    }
    else if (event.type == EngineMessageBus::EventType::MidiMenuEnabledChanged)
    {
        if (std::holds_alternative<bool> (event.data))
        {
            enableMidiMenu (std::get<bool> (event.data));
        }
    }
}

// MidiMappingComponent.cpp - updated buildMappingList() to not pre-position components
void MidiMappingComponent::buildMappingList()
{
    constexpr size_t numCommands = std::size (EngineMessageBus::commandTypeNamesForMenu);
    auto commandNamesMapping = EngineMessageBus::commandTypeNamesForMenu;

    for (int i = 0; i < numCommands; ++i)
    {
        auto commandType = commandNamesMapping[i].first;
        auto name = commandNamesMapping[i].second;
        juce::String category = EngineMessageBus::getCategoryForCommandType (commandNamesMapping[i].first);
        bool isCCCommand = midiMappingManager->isCCCommand (commandNamesMapping[i].first);

        mappingData.push_back ({ commandType, name, category, isCCCommand });
    }

    // Sort by category
    std::sort (mappingData.begin(), mappingData.end(), [] (const auto& a, const auto& b) { return a.category < b.category; });

    juce::String currentCategory;

    for (const auto& data : mappingData)
    {
        if (data.category != currentCategory)
        {
            currentCategory = data.category;
            auto* header = categoryHeaders.add (new CategoryHeader (currentCategory));
            contentComponent.addAndMakeVisible (header);
        }

        auto* row = mappingRows.add (new MappingRow (this, data));
        contentComponent.addAndMakeVisible (row);
    }
}

void MidiMappingComponent::startLearning (MappingRow* row)
{
    if (currentLearningRow && currentLearningRow != row)
    {
        currentLearningRow->setLearning (false);
        currentLearningRow = nullptr;
    }

    currentLearningRow = row;
    currentLearningRow->setLearning (true);

    uiToEngineBus->pushCommand ({ .type = EngineMessageBus::CommandType::StartMidiLearn,
                                  .trackIndex = -1,
                                  .payload = (int) row->getCommandType() });
}

void MidiMappingComponent::cancelLearning()
{
    if (currentLearningRow)
    {
        currentLearningRow->setLearning (false);
        currentLearningRow = nullptr;
    }

    uiToEngineBus->pushCommand ({ .type = EngineMessageBus::CommandType::StopMidiLearn, .trackIndex = -1, .payload = std::monostate {} });
}

void MidiMappingComponent::clearMapping (EngineMessageBus::CommandType command)
{
    currentLearningRow = nullptr;
    uiToEngineBus->pushCommand ({ .type = EngineMessageBus::CommandType::ClearMidiMappings, .trackIndex = -1, .payload = (int) command });
    refreshAllRows();
}

void MidiMappingComponent::refreshAllRows()
{
    for (auto* row : mappingRows)
    {
        row->refresh();
    }
}

MidiMappingManager* MidiMappingComponent::getMappingManager() const { return midiMappingManager; }
