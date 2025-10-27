#pragma once
#include "audio/AudioToUIBridge.h"
#include "audio/EngineStateToUIBridge.h"
#include "engine/MidiCommandConfig.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/AccentBarComponent.h"
#include "ui/components/LevelComponent.h"
#include "ui/components/PlaybackSpeedComponent.h"
#include "ui/components/TrackEditComponent.h"
#include "ui/components/VolumesComponent.h"
#include "ui/components/WaveformComponent.h"
#include "ui/helpers/MidiCommandDispatcher.h"
#include <JuceHeader.h>

class TrackComponent : public juce::Component, private juce::Timer
{
public:
    TrackComponent (MidiCommandDispatcher* midiCommandDispatcher,
                    int trackIdx,
                    AudioToUIBridge* audioBridge,
                    EngineStateToUIBridge* engineBridge,
                    UIToEngineBridge* uiToEngineBridge)
        : trackIndex (trackIdx)
        , waveformDisplay (audioBridge, uiToEngineBridge)
        , accentBar (midiCommandDispatcher, trackIdx, audioBridge, engineBridge)
        , volumeFader (midiCommandDispatcher, trackIdx, "VOLUME", MidiNotes::TRACK_VOLUME_CC)
        , speedFader (midiCommandDispatcher, trackIdx)
        , trackEditComponent (midiCommandDispatcher, trackIdx)
        , volumesComponent (midiCommandDispatcher, trackIdx)
        , midiDispatcher (midiCommandDispatcher)
        , bridge (audioBridge)
    {
        addAndMakeVisible (waveformDisplay);

        addAndMakeVisible (volumeFader);

        muteButton.setButtonText ("MUTE");
        muteButton.setComponentID ("mute");
        muteButton.setClickingTogglesState (true);
        muteButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::MUTE_BUTTON, trackIndex); };
        addAndMakeVisible (muteButton);

        soloButton.setButtonText ("SOLO");
        soloButton.setComponentID ("solo");
        soloButton.setClickingTogglesState (true);
        soloButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::SOLO_BUTTON, trackIndex); };
        addAndMakeVisible (soloButton);

        lockPitchButton.setButtonText ("LOCK");
        lockPitchButton.setComponentID ("lock");
        lockPitchButton.setClickingTogglesState (true);
        lockPitchButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::KEEP_PITCH_BUTTON, trackIndex); };
        addAndMakeVisible (lockPitchButton);

        reverseButton.setButtonText ("REV");
        reverseButton.setComponentID ("reverse");
        reverseButton.setClickingTogglesState (true);
        reverseButton.onClick = [this]() { midiDispatcher->sendCommandToEngine (MidiNotes::REVERSE_BUTTON, trackIndex); };
        addAndMakeVisible (reverseButton);

        addAndMakeVisible (accentBar);
        addAndMakeVisible (speedFader);
        addAndMakeVisible (trackEditComponent);
        addAndMakeVisible (volumesComponent);

        // pitchFader.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        // pitchFader.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        // pitchFader.setValue (0.0);
        // pitchFader.onValueChange = [this]()
        // { midiDispatcher.sendControlChangeToEngine (MidiNotes::PITCH_SHIFT_CC, trackIndex, pitchFader.getValue() + 12.0); };
        // addAndMakeVisible (pitchFader);

        updateControlsFromEngine();
        startTimerHz (10);
    }

    ~TrackComponent() override { stopTimer(); }

    void timerCallback() override { updateControlsFromEngine(); }

    int getTrackIndex() const { return trackIndex; }

    void updateControlsFromEngine()
    {
        float currentVolume = midiDispatcher->getCurrentVolume (trackIndex);
        if (std::abs (volumeFader.getValue() - currentVolume) > 0.001)
        {
            volumeFader.setValue (currentVolume, juce::dontSendNotification);
        }

        bool currentMuted = midiDispatcher->isMuted (trackIndex);
        if (muteButton.getToggleState() != currentMuted)
        {
            muteButton.setToggleState (currentMuted, juce::dontSendNotification);
        }

        accentBar.repaint();
    }

    void setActive (bool shouldBeActive)
    {
        if (isActive != shouldBeActive)
        {
            isActive = shouldBeActive;
            repaint();
        }
    }

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

        juce::FlexBox MSButtons;
        MSButtons.flexDirection = juce::FlexBox::Direction::column;
        MSButtons.items.add (juce::FlexItem (muteButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 0, 1, 0)));
        MSButtons.items.add (juce::FlexItem (soloButton).withFlex (0.5f).withMargin (juce::FlexItem::Margin (1, 0, 0, 0)));
        juce::FlexBox muteSoloRow;
        muteSoloRow.flexDirection = juce::FlexBox::Direction::row;
        muteSoloRow.items.add (juce::FlexItem (MSButtons).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        muteSoloRow.items.add (juce::FlexItem (volumeFader).withFlex (0.5f).withMargin (juce::FlexItem::Margin (0, 4, 0, 4)));

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
    juce::TextButton lockPitchButton;
    juce::TextButton reverseButton;
    AccentBar accentBar;
    LevelComponent volumeFader;
    PlaybackSpeedComponent speedFader;
    TrackEditComponent trackEditComponent;

    VolumesComponent volumesComponent;

    juce::Slider pitchFader;

    MidiCommandDispatcher* midiDispatcher;
    AudioToUIBridge* bridge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackComponent)
};
