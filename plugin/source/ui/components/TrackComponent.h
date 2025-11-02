#pragma once
#include "audio/AudioToUIBridge.h"
#include "audio/EngineStateToUIBridge.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/AccentBarComponent.h"
#include "ui/components/LevelComponent.h"
#include "ui/components/PlaybackPitchComponent.h"
#include "ui/components/PlaybackSpeedComponent.h"
#include "ui/components/TrackEditComponent.h"
#include "ui/components/VolumesComponent.h"
#include "ui/components/WaveformComponent.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class TrackComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    TrackComponent (EngineMessageBus* engineMessageBus, int trackIdx, AudioToUIBridge* audioBridge, EngineStateToUIBridge* engineBridge)
        : trackIndex (trackIdx)
        , waveformDisplay (trackIdx, audioBridge, engineMessageBus)
        , accentBar (engineMessageBus, trackIdx, audioBridge, engineBridge)
        , volumeFader (engineMessageBus, trackIdx, "VOLUME", MidiNotes::TRACK_VOLUME_CC)
        , speedFader (engineMessageBus, trackIdx)
        , pitchFader (engineMessageBus, trackIdx)
        , trackEditComponent (engineMessageBus, trackIdx)
        , volumesComponent (engineMessageBus, trackIdx)
        , uiToEngineBus (engineMessageBus)
        , bridge (audioBridge)
    {
        addAndMakeVisible (waveformDisplay);

        addAndMakeVisible (volumeFader);

        muteButton.setButtonText ("MUTE");
        muteButton.setComponentID ("mute");
        muteButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleMute, trackIndex, {} }); };
        addAndMakeVisible (muteButton);

        soloButton.setButtonText ("SOLO");
        soloButton.setComponentID ("solo");
        soloButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleSolo, trackIndex, {} }); };
        addAndMakeVisible (soloButton);

        syncButton.setButtonText ("SYNC");
        syncButton.setComponentID ("sync");
        syncButton.setToggleState (true, juce::dontSendNotification);
        syncButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleSyncTrack, trackIndex, {} }); };
        addAndMakeVisible (syncButton);

        lockPitchButton.setButtonText ("LOCK");
        lockPitchButton.setComponentID ("lock");
        lockPitchButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::TogglePitchLock, trackIndex, {} }); };
        addAndMakeVisible (lockPitchButton);

        reverseButton.setButtonText ("REV");
        reverseButton.setComponentID ("reverse");
        reverseButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleReverse, trackIndex, {} }); };
        addAndMakeVisible (reverseButton);

        addAndMakeVisible (accentBar);
        addAndMakeVisible (speedFader);
        addAndMakeVisible (pitchFader);
        addAndMakeVisible (trackEditComponent);
        addAndMakeVisible (volumesComponent);

        uiToEngineBus->addListener (this);
    }

    ~TrackComponent() override { uiToEngineBus->removeListener (this); }

    int getTrackIndex() const { return trackIndex; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        g.setColour (isActive ? LooperTheme::Colors::surface.brighter (0.05f) : LooperTheme::Colors::surface);
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

        g.setColour (isActive ? LooperTheme::Colors::cyan : LooperTheme::Colors::border);
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), 4.0f, isActive ? 2.0f : 1.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4);

        // Accent bar on the very left edge
        accentBar.setBounds (bounds.removeFromLeft (28));
        bounds.removeFromLeft (2);

        juce::FlexBox mainRow;
        mainRow.flexDirection = juce::FlexBox::Direction::column;
        mainRow.alignItems = juce::FlexBox::AlignItems::stretch;

        mainRow.items.add (juce::FlexItem (waveformDisplay).withFlex (0.6f));

        juce::FlexBox playbackButtonsColumn;
        playbackButtonsColumn.flexDirection = juce::FlexBox::Direction::column;
        playbackButtonsColumn.items.add (juce::FlexItem (lockPitchButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        playbackButtonsColumn.items.add (juce::FlexItem (reverseButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        juce::FlexBox pitchSpeedRow;
        pitchSpeedRow.flexDirection = juce::FlexBox::Direction::row;
        pitchSpeedRow.items.add (juce::FlexItem (speedFader).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        pitchSpeedRow.items.add (juce::FlexItem (playbackButtonsColumn).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        pitchSpeedRow.items.add (juce::FlexItem (pitchFader).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));

        juce::FlexBox MSButtons;
        MSButtons.flexDirection = juce::FlexBox::Direction::column;
        MSButtons.items.add (juce::FlexItem (muteButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 1, 0)));
        MSButtons.items.add (juce::FlexItem (soloButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (1, 0, 0, 0)));
        juce::FlexBox muteSoloRow;
        muteSoloRow.flexDirection = juce::FlexBox::Direction::row;
        muteSoloRow.items.add (juce::FlexItem (MSButtons).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem (volumeFader).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));
        muteSoloRow.items.add (juce::FlexItem (syncButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        juce::FlexBox controlsRow;
        controlsRow.flexDirection = juce::FlexBox::Direction::row;
        controlsRow.items.add (juce::FlexItem (muteSoloRow).withFlex (0.5f).withMargin (juce::FlexItem::Margin (2, 0, 0, 0)));
        controlsRow.items.add (juce::FlexItem().withFlex (0.1f).withMargin (juce::FlexItem::Margin (2, 0, 2, 0)));
        controlsRow.items.add (juce::FlexItem (trackEditComponent).withFlex (0.5f).withMargin (juce::FlexItem::Margin (2, 0, 2, 0)));
        controlsRow.items.add (juce::FlexItem().withFlex (0.1f).withMargin (juce::FlexItem::Margin (2, 0, 2, 0)));
        controlsRow.items.add (juce::FlexItem (volumesComponent).withFlex (0.5f).withMargin (juce::FlexItem::Margin (2, 0, 2, 0)));
        controlsRow.items.add (juce::FlexItem().withFlex (0.1f).withMargin (juce::FlexItem::Margin (2, 0, 2, 0)));
        controlsRow.items.add (juce::FlexItem (pitchSpeedRow).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 2, 0)));

        mainRow.items.add (juce::FlexItem (controlsRow).withFlex (0.3f));

        mainRow.performLayout (bounds.toFloat());
    }

private:
    int trackIndex;
    bool isActive = false;
    WaveformComponent waveformDisplay;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::TextButton syncButton;

    juce::TextButton lockPitchButton;
    juce::TextButton reverseButton;
    AccentBar accentBar;
    LevelComponent volumeFader;
    PlaybackSpeedComponent speedFader;
    PlaybackPitchComponent pitchFader;
    TrackEditComponent trackEditComponent;

    VolumesComponent volumesComponent;

    EngineMessageBus* uiToEngineBus;
    AudioToUIBridge* bridge;

    void setActive (bool shouldBeActive)
    {
        if (isActive != shouldBeActive)
        {
            isActive = shouldBeActive;
            repaint();
        }
    }

    constexpr static EngineMessageBus::EventType subscribedEvents[] = {
        EngineMessageBus::EventType::TrackMuteChanged,      EngineMessageBus::EventType::TrackSoloChanged,
        EngineMessageBus::EventType::TrackPitchLockChanged, EngineMessageBus::EventType::TrackReverseDirection,
        EngineMessageBus::EventType::TrackVolumeChanged,    EngineMessageBus::EventType::TrackSpeedChanged,
        EngineMessageBus::EventType::TrackPitchChanged,     EngineMessageBus::EventType::ActiveTrackChanged,
        EngineMessageBus::EventType::ActiveTrackCleared,    EngineMessageBus::EventType::TrackSyncChanged
    };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        if (event.trackIndex != trackIndex) return;
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::TrackMuteChanged:
                if (std::holds_alternative<bool> (event.data))
                {
                    bool isMuted = std::get<bool> (event.data);
                    muteButton.setToggleState (isMuted, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::TrackSoloChanged:
                if (std::holds_alternative<bool> (event.data))
                {
                    bool isSoloed = std::get<bool> (event.data);
                    soloButton.setToggleState (isSoloed, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::TrackSyncChanged:
                if (std::holds_alternative<bool> (event.data))
                {
                    bool isSynced = std::get<bool> (event.data);
                    syncButton.setToggleState (isSynced, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::TrackPitchLockChanged:
                if (std::holds_alternative<bool> (event.data))
                {
                    bool isPitchLocked = std::get<bool> (event.data);
                    lockPitchButton.setToggleState (isPitchLocked, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::TrackReverseDirection:
                if (std::holds_alternative<bool> (event.data))
                {
                    bool isReversed = std::get<bool> (event.data);
                    reverseButton.setToggleState (isReversed, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::TrackVolumeChanged:
                if (std::holds_alternative<float> (event.data))
                {
                    float volume = std::get<float> (event.data);
                    if (std::abs (volumeFader.getValue() - volume) > 0.001) volumeFader.setValue (volume, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::TrackSpeedChanged:
                if (std::holds_alternative<float> (event.data))
                {
                    float speed = std::get<float> (event.data);
                    if (std::abs (speedFader.getValue() - speed) > 0.001) speedFader.setValue (speed, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::TrackPitchChanged:
                if (std::holds_alternative<float> (event.data))
                {
                    float pitch = std::get<float> (event.data);
                    if (std::abs (pitchFader.getValue() - pitch) > 0.001) pitchFader.setValue (pitch, juce::dontSendNotification);
                }
                break;
            case EngineMessageBus::EventType::ActiveTrackChanged:
                if (std::holds_alternative<int> (event.data))
                {
                    int activeTrack = std::get<int> (event.data);
                    setActive (activeTrack == trackIndex);
                }
                break;
            case EngineMessageBus::EventType::ActiveTrackCleared:
                setActive (false);
                break;
            default:
                throw juce::String ("Unhandled event type in TrackComponent: ") + juce::String ((int) event.type);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
