#pragma once
#include "audio/EngineCommandBus.h"
#include "engine/AutomationEngine.h"
#include "ui/colors/TokyoNight.h"
#include "ui/components/ProgressiveSpeedPopup.h"
#include <JuceHeader.h>

class PlaybackSpeedSlider : public juce::Slider
{
public:
    PlaybackSpeedSlider()
    {
        setRange (min, max, step);
        setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    }

    double snapValue (double attemptedValue, juce::Slider::DragMode) override
    {
        if (std::abs (attemptedValue - 0.5) < snapThreshold) return 0.5;
        if (std::abs (attemptedValue - 0.75) < snapThreshold) return 0.75;
        if (std::abs (attemptedValue - 1.0) < snapThreshold) return 1.0;
        if (std::abs (attemptedValue - 1.5) < snapThreshold) return 1.5;
        if (std::abs (attemptedValue - 2.0) < snapThreshold) return 2.0;

        return attemptedValue;
    }

    bool isInSnapRange (double value)
    {
        return (std::abs (value - 0.5) < snapThreshold) || (std::abs (value - 0.75) < snapThreshold)
               || (std::abs (value - 1.0) < snapThreshold) || (std::abs (value - 1.5) < snapThreshold)
               || (std::abs (value - 2.0) < snapThreshold);
    }

    double getValueFromText (const juce::String& text) override { return text.getDoubleValue(); }

    juce::String getTextFromValue (double value) override { return juce::String (value, 2) + "x"; }

    // Custom mapping: linear from 0.5→1.0 (left half) and 1.0→2.0 (right half)
    double valueToProportionOfLength (double value) override
    {
        if (value <= center)
        {
            return juce::jmap (value, min, center, 0.0, 0.5);
        }
        else
        {
            return juce::jmap (value, center, max, 0.5, 1.0);
        }
    }

    double proportionOfLengthToValue (double proportion) override
    {
        if (proportion <= 0.5)
        {
            return juce::jmap (proportion, 0.0, 0.5, min, center);
        }
        else
        {
            return juce::jmap (proportion, 0.5, 1.0, center, max);
        }
    }

    std::function<void()> onShiftClick;

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isShiftDown() && onShiftClick)
        {
            onShiftClick();
            return;
        }
        juce::Slider::mouseDown (event);
    }

private:
    double min = MIN_PLAYBACK_SPEED;
    double max = MAX_PLAYBACK_SPEED;
    double center = 1.0;
    double step = 0.01;
    const double snapThreshold = 0.03;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackSpeedSlider)
};

class PlaybackSpeedComponent : public juce::Component, public EngineMessageBus::Listener
{
public:
    PlaybackSpeedComponent (EngineMessageBus* engineMessageBus, int trackIdx, AudioToUIBridge* bridge, AutomationEngine* automationEngine)
        : trackIndex (trackIdx), uiToEngineBus (engineMessageBus), uiBridge (bridge), automationEngine (automationEngine)
    {
        titleLabel.setText ("SPEED", juce::dontSendNotification);
        titleLabel.setFont (LooperTheme::Fonts::getBoldFont (9.0f));
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (titleLabel);

        speedSlider.setValue (DEFAULT_PLAYBACK_SPEED);
        speedSlider.onValueChange = [this]()
        {
            if (! speedSlider.isMouseButtonDown()) return; // Only respond to user drag
            if (speedMode == SpeedMode::Automation)
            {
                // User touched knob - exit automation
                speedMode = SpeedMode::Manual;
                currentSpeedCurve.preset = ProgressiveSpeedCurve::PresetType::Flat; // Reset to flat
                currentSpeedCurve.endSpeed = (float) speedSlider.getValue();
            }

            uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetPlaybackSpeed,
                                                                    trackIndex,
                                                                    (float) speedSlider.getValue() });
        };
        speedSlider.onShiftClick = [this]() { openProgressiveSpeedPopup(); };
        addAndMakeVisible (speedSlider);
        uiToEngineBus->addListener (this);
    }

    ~PlaybackSpeedComponent() override { uiToEngineBus->removeListener (this); }

    void setValue (double newValue, juce::NotificationType notification) { speedSlider.setValue (newValue, notification); }

    double getValue() const { return speedSlider.getValue(); }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int labelHeight = 12;
        titleLabel.setBounds (bounds.removeFromTop (labelHeight));
        speedSlider.setBounds (bounds.reduced (2));
    }

private:
    juce::Label titleLabel;
    PlaybackSpeedSlider speedSlider;
    int trackIndex;
    EngineMessageBus* uiToEngineBus;
    AudioToUIBridge* uiBridge;
    AutomationEngine* automationEngine;

    std::unique_ptr<ProgressiveSpeedPopup> progressiveSpeedPopup;
    ProgressiveSpeedCurve currentSpeedCurve;

    void openProgressiveSpeedPopup();

    void closeProgressiveSpeedPopup();

    void applyProgressiveSpeed (const ProgressiveSpeedCurve& curve, int index = 0)
    {
        currentSpeedCurve = curve;
        speedMode = SpeedMode::Automation;

        // Convert to automation curve
        AutomationCurve autoCurve;
        autoCurve.breakpoints = curve.breakpoints;
        autoCurve.commandType = EngineMessageBus::CommandType::SetPlaybackSpeed;
        autoCurve.trackIndex = trackIndex;
        autoCurve.enabled = true;
        autoCurve.mode = AutomationMode::LoopBased;

        // Register with automation engine
        juce::String paramId = "track" + juce::String (trackIndex) + "_speed";
        automationEngine->registerCurve (paramId, autoCurve);

        // Set initial speed
        uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SetPlaybackSpeed,
                                                                trackIndex,
                                                                curve.breakpoints[(size_t) index].getY() });
    }

    enum class SpeedMode
    {
        Manual,    // User directly controls speed via knob
        Automation // Speed controlled by progressive curve
    };

    SpeedMode speedMode = SpeedMode::Manual;

    constexpr static EngineMessageBus::EventType subscribedEvents[] = {
        EngineMessageBus::EventType::TrackSpeedChanged,
    };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        if (event.trackIndex != trackIndex) return;
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::TrackSpeedChanged:
                if (std::holds_alternative<float> (event.data))
                {
                    float speed = std::get<float> (event.data);
                    if (std::abs (getValue() - speed) > 0.001) setValue (speed, juce::dontSendNotification);
                }
                break;
        }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlaybackSpeedComponent)
};
