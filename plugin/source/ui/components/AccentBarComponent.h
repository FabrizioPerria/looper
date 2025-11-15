#pragma once
#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class AccentBar : public juce::Component, public EngineMessageBus::Listener
{
public:
    AccentBar (EngineMessageBus* engineMessageBus, int trackIdx) : uiToEngineBus (engineMessageBus), trackIndex (trackIdx)
    {
        setInterceptsMouseClicks (true, false);
        isTrackActive = false;
        isPendingTrack = false;
        uiToEngineBus->addListener (this);
    }
    ~AccentBar() override { uiToEngineBus->removeListener (this); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Choose color
        if (isPendingTrack && ! isTrackActive)
            g.setColour (LooperTheme::Colors::yellow.withAlpha (0.8f));
        else if (isTrackActive)
            g.setColour (LooperTheme::Colors::cyan.withAlpha (0.8f));
        else
            g.setColour (LooperTheme::Colors::primary.withAlpha (0.3f));

        // ACTUALLY DRAW IT!
        g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

        // Draw track number
        g.setColour (isTrackActive ? LooperTheme::Colors::backgroundDark : LooperTheme::Colors::cyan);
        g.setFont (LooperTheme::Fonts::getBoldFont (14.0f));
        g.drawText (juce::String (trackIndex + 1), bounds, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::SelectTrack, trackIndex, {} });
    }

    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void mouseExit (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::NormalCursor); }

    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::ActiveTrackChanged,
                                                                        EngineMessageBus::EventType::PendingTrackChanged,
                                                                        EngineMessageBus::EventType::ActiveTrackCleared };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        if (event.trackIndex != trackIndex) return;
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::ActiveTrackChanged:
                if (std::holds_alternative<int> (event.data))
                {
                    int activeTrack = std::get<int> (event.data);
                    isTrackActive = (activeTrack == trackIndex);
                    isPendingTrack = false;
                }
                break;
            case EngineMessageBus::EventType::PendingTrackChanged:
                if (std::holds_alternative<int> (event.data))
                {
                    int pendingTrack = std::get<int> (event.data);
                    isPendingTrack = (pendingTrack == trackIndex);
                    isTrackActive = false;
                }
                break;
            case EngineMessageBus::EventType::ActiveTrackCleared:
                isTrackActive = false;
                isPendingTrack = false;
                break;
            default:
                throw juce::String ("Unhandled event type in AccentBar: ") + juce::String ((int) event.type);
        }
        repaint();
    }

private:
    EngineMessageBus* uiToEngineBus;
    int trackIndex;
    std::atomic<bool> isTrackActive { false };
    std::atomic<bool> isPendingTrack { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AccentBar)
};
