#pragma once

#include "CustomStandaloneFilterWindow.h"
#include "audio/EngineCommandBus.h"
#include "engine/Constants.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/MeterWithGainComponent.h"
#include <JuceHeader.h>

class FooterComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    FooterComponent (EngineMessageBus* engineMessageBus, EngineStateToUIBridge* bridge)
        : inputMeter ("IN",
                      engineMessageBus,
                      bridge,
                      EngineMessageBus::CommandType::SetInputGain,
                      EngineMessageBus::EventType::InputGainChanged,
                      juce::Decibels::decibelsToGain (DEFAULT_INPUT_GAIN))
        , outputMeter ("OUT",
                       engineMessageBus,
                       bridge,
                       EngineMessageBus::CommandType::SetOutputGain,
                       EngineMessageBus::EventType::OutputGainChanged,
                       juce::Decibels::decibelsToGain (DEFAULT_OUTPUT_GAIN))
        , uiToEngineBus (engineMessageBus)
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
        settingsLabel.setText ("Settings", juce::dontSendNotification);
        settingsLabel.setJustificationType (juce::Justification::centred);
        settingsLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (settingsLabel);

        audioSettingsButton.setButtonText ("Audio");
        audioSettingsButton.setComponentID ("audioSettings");
        audioSettingsButton.onClick = [this]() { juce::StandalonePluginHolder::getInstance()->showAudioSettingsDialog(); };
        if (JUCEApplicationBase::isStandaloneApp()) addAndMakeVisible (audioSettingsButton);

        midiButton.setButtonText ("Midi");
        midiButton.setComponentID ("midi");
        addAndMakeVisible (midiButton);

        playModeLabel.setText ("Play Mode", juce::dontSendNotification);
        playModeLabel.setJustificationType (juce::Justification::centred);
        playModeLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (playModeLabel);

        playModeButton.setButtonText ("Single Track");
        playModeButton.setComponentID ("single");
        playModeButton.setToggleState (DEFAULT_SINGLE_PLAY_MODE, juce::dontSendNotification);
        playModeButton.onClick = [this]()
        { uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleSinglePlayMode, -1, {} }); };
        addAndMakeVisible (playModeButton);

        saveLabel.setText ("Save", juce::dontSendNotification);
        saveLabel.setJustificationType (juce::Justification::centred);
        saveLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        addAndMakeVisible (saveLabel);
        activeTrack.setButtonText ("Active Track");
        activeTrack.setComponentID ("saveActive");
        activeTrack.onClick = [this]()
        {
            fileChooser = std::make_unique<juce::FileChooser> ("Save track as...",
                                                               juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                               "*.wav");

            fileChooser->launchAsync (juce::FileBrowserComponent::saveMode,
                                      [this] (const juce::FileChooser& fc)
                                      {
                                          auto result = fc.getResult();
                                          uiToEngineBus->pushCommand (EngineMessageBus::Command {
                                              EngineMessageBus::CommandType::SaveTrackToFile,
                                              -1,
                                              result });
                                      });
        };
        addAndMakeVisible (activeTrack);

        allTracks.setButtonText ("All Tracks");
        allTracks.setComponentID ("saveAll");
        allTracks.onClick = [this]()
        {
            fileChooser = std::make_unique<juce::FileChooser> ("Select folder...",
                                                               juce::File::getSpecialLocation (juce::File::userHomeDirectory));

            fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                      [this] (const juce::FileChooser& fc)
                                      {
                                          auto result = fc.getResult();
                                          uiToEngineBus->pushCommand (EngineMessageBus::Command {
                                              EngineMessageBus::CommandType::SaveAllTracksToFolder,
                                              -1,
                                              result });
                                      });
        };

        addAndMakeVisible (allTracks);

        uiToEngineBus->addListener (this);
    }

    ~FooterComponent() override { uiToEngineBus->removeListener (this); }

    void resized() override
    {
        auto bounds = getLocalBounds();
        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::row;
        mainBox.alignItems = juce::FlexBox::AlignItems::stretch;

        mainBox.items.add (juce::FlexItem (inputMeter).withFlex (0.5f).withMargin (juce::FlexItem::Margin (4, 50, 0, 1)));

        juce::FlexBox audioMidisettingsBox;
        audioMidisettingsBox.flexDirection = juce::FlexBox::Direction::row;
        audioMidisettingsBox.alignItems = juce::FlexBox::AlignItems::stretch;
        audioMidisettingsBox.items
            .add (juce::FlexItem (audioSettingsButton).withFlex (0.9f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        audioMidisettingsBox.items.add (juce::FlexItem (midiButton).withFlex (0.9f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        juce::FlexBox settingsBox;
        settingsBox.flexDirection = juce::FlexBox::Direction::column;
        settingsBox.alignItems = juce::FlexBox::AlignItems::stretch;
        settingsBox.items.add (juce::FlexItem (settingsLabel).withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        settingsBox.items.add (juce::FlexItem (audioMidisettingsBox).withFlex (0.9f).withMargin (juce::FlexItem::Margin (6, 1, 0, 1)));

        mainBox.items.add (juce::FlexItem (settingsBox).withFlex (0.4f).withMargin (juce::FlexItem::Margin (4, 1, 0, 1)));

        juce::FlexBox playModeBox;
        playModeBox.flexDirection = juce::FlexBox::Direction::column;
        playModeBox.alignItems = juce::FlexBox::AlignItems::stretch;
        playModeBox.items.add (juce::FlexItem (playModeLabel).withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        playModeBox.items.add (juce::FlexItem (playModeButton).withFlex (0.9f).withMargin (juce::FlexItem::Margin (6, 1, 0, 1)));

        mainBox.items.add (juce::FlexItem (playModeBox).withFlex (0.2f).withMargin (juce::FlexItem::Margin (4, 1, 0, 1)));

        juce::FlexBox saveButtonsBox;
        saveButtonsBox.flexDirection = juce::FlexBox::Direction::row;
        saveButtonsBox.alignItems = juce::FlexBox::AlignItems::stretch;
        saveButtonsBox.items.add (juce::FlexItem (activeTrack).withFlex (0.9f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        saveButtonsBox.items.add (juce::FlexItem (allTracks).withFlex (0.9f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));

        juce::FlexBox saveBox;
        saveBox.flexDirection = juce::FlexBox::Direction::column;
        saveBox.alignItems = juce::FlexBox::AlignItems::stretch;
        saveBox.items.add (juce::FlexItem (saveLabel).withFlex (0.3f).withMargin (juce::FlexItem::Margin (0, 1, 0, 1)));
        saveBox.items.add (juce::FlexItem (saveButtonsBox).withFlex (0.9f).withMargin (juce::FlexItem::Margin (6, 1, 0, 1)));

        mainBox.items.add (juce::FlexItem (saveBox).withFlex (0.4f).withMargin (juce::FlexItem::Margin (4, 1, 0, 1)));

        mainBox.items.add (juce::FlexItem (outputMeter).withFlex (0.5f).withMargin (juce::FlexItem::Margin (4, 1, 0, 50)));

        mainBox.performLayout (bounds.toFloat());
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (LooperTheme::Colors::surface);

        g.setColour (LooperTheme::Colors::surface.brighter (0.2f));

        auto midiTitleBounds = settingsLabel.getBounds().toFloat();
        g.fillRect (midiTitleBounds.getX() + 3.0f, midiTitleBounds.getBottom() + 3.0f, midiTitleBounds.getWidth() - 6.0f, 1.0f);

        auto playModeTitleBounds = playModeLabel.getBounds().toFloat();
        g.fillRect (playModeTitleBounds.getX() + 3.0f, playModeTitleBounds.getBottom() + 3.0f, playModeTitleBounds.getWidth() - 6.0f, 1.0f);

        auto saveTitleBounds = saveLabel.getBounds().toFloat();
        g.fillRect (saveTitleBounds.getX() + 3.0f, saveTitleBounds.getBottom() + 3.0f, saveTitleBounds.getWidth() - 6.0f, 1.0f);
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
    juce::TextButton audioSettingsButton { "Audio Settings" };

    juce::TextButton activeTrack { "Active Track" };
    juce::TextButton allTracks { "All Tracks" };

    EngineMessageBus* uiToEngineBus;

    juce::Label settingsLabel;
    juce::Label playModeLabel;
    juce::Label saveLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterComponent)
};
