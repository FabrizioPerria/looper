// MidiMappingComponent.cpp
#include "MidiMappingComponent.h"
#include "engine/LooperEngine.h"

MidiMappingComponent::MidiMappingComponent (MidiMappingManager* mappingManager, EngineMessageBus* messageBus)
    : midiMappingManager (mappingManager), uiToEngineBus (messageBus)
{
    buildMappingList();

    addAndMakeVisible (activityIndicator);

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

void MidiMappingComponent::paint (juce::Graphics& g) { g.fillAll (juce::Colours::darkgrey); }

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

    viewport.setBounds (bounds);

    int yPos = 0;
    int rowHeight = 30;
    int headerHeight = 25;

    for (auto* header : categoryHeaders)
    {
        header->setBounds (0, yPos, viewport.getMaximumVisibleWidth(), headerHeight);
        yPos += headerHeight;
    }

    for (auto* row : mappingRows)
    {
        row->setBounds (0, yPos, viewport.getMaximumVisibleWidth(), rowHeight);
        yPos += rowHeight;
    }

    contentComponent.setSize (viewport.getMaximumVisibleWidth(), yPos);
}

void MidiMappingComponent::handleEngineEvent (const EngineMessageBus::Event& event)
{
    if (event.type == EngineMessageBus::EventType::MidiMappingChanged)
    {
        if (currentLearningRow)
        {
            currentLearningRow->setLearning (false);
            currentLearningRow = nullptr;
        }
        refreshAllRows();
    }
    else if (event.type == EngineMessageBus::EventType::MidiActivityReceived)
    {
        if (std::holds_alternative<juce::String> (event.data))
        {
            activityIndicator.setMidiMessage (std::get<juce::String> (event.data));
        }
    }
}

void MidiMappingComponent::buildMappingList()
{
    mappingData = {
        // Transport
        { EngineMessageBus::CommandType::ToggleRecord, "Toggle Record", "Transport", false },
        { EngineMessageBus::CommandType::TogglePlay, "Toggle Play", "Transport", false },
        { EngineMessageBus::CommandType::Stop, "Stop", "Transport", false },

        // Track Management
        { EngineMessageBus::CommandType::NextTrack, "Next Track", "Track Management", false },
        { EngineMessageBus::CommandType::PreviousTrack, "Previous Track", "Track Management", false },
        { EngineMessageBus::CommandType::SelectTrack, "Select Track", "Track Management", true },
        { EngineMessageBus::CommandType::ToggleSyncTrack, "Sync Track", "Track Management", false },
        { EngineMessageBus::CommandType::ToggleSinglePlayMode, "Single Play Mode", "Track Management", false },
        { EngineMessageBus::CommandType::ToggleFreeze, "Freeze", "Track Management", false },

        // Track Operations
        { EngineMessageBus::CommandType::Undo, "Undo", "Track Operations", false },
        { EngineMessageBus::CommandType::Redo, "Redo", "Track Operations", false },
        { EngineMessageBus::CommandType::Clear, "Clear", "Track Operations", false },
        { EngineMessageBus::CommandType::ToggleMute, "Mute", "Track Operations", false },
        { EngineMessageBus::CommandType::ToggleSolo, "Solo", "Track Operations", false },
        { EngineMessageBus::CommandType::ToggleVolumeNormalize, "Normalize", "Track Operations", false },
        { EngineMessageBus::CommandType::ToggleReverse, "Reverse", "Track Operations", false },
        { EngineMessageBus::CommandType::TogglePitchLock, "Pitch Lock", "Track Operations", false },

        // Playback Controls
        { EngineMessageBus::CommandType::SetVolume, "Track Volume", "Playback Controls", true },
        { EngineMessageBus::CommandType::SetPlaybackSpeed, "Playback Speed", "Playback Controls", true },
        { EngineMessageBus::CommandType::SetPlaybackPitch, "Pitch Shift", "Playback Controls", true },

        // Recording Controls
        { EngineMessageBus::CommandType::SetNewOverdubGain, "Overdub Level", "Recording Controls", true },
        { EngineMessageBus::CommandType::SetExistingAudioGain, "Existing Audio Level", "Recording Controls", true },

        // Metronome
        { EngineMessageBus::CommandType::SetMetronomeEnabled, "Metro Toggle", "Metronome", false },
        { EngineMessageBus::CommandType::SetMetronomeStrongBeat, "Metro Strong Beat", "Metronome", false },
        { EngineMessageBus::CommandType::SetMetronomeBPM, "Metronome BPM", "Metronome", true },
        { EngineMessageBus::CommandType::SetMetronomeVolume, "Metronome Volume", "Metronome", true },

        // Master
        { EngineMessageBus::CommandType::SetInputGain, "Input Gain", "Master", true },
        { EngineMessageBus::CommandType::SetOutputGain, "Output Gain", "Master", true },
    };

    juce::String currentCategory;
    int yPos = 0;

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
