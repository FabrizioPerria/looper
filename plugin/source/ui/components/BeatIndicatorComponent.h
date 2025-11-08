#pragma once
#include "audio/EngineCommandBus.h"
#include "engine/Metronome.h"
#include "profiler/PerfettoProfiler.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

/**
 * Beat indicator with tap tempo functionality.
 * 
 * Click behavior:
 * - If metronome is off: First click enables it at current BPM
 * - If metronome is on: Click = tap tempo
 *   - 2+ taps within 3 seconds: Calculate and set new BPM
 *   - Taps must be between 30-300 BPM range (200ms - 2000ms intervals)
 */
class BeatIndicatorComponent : public juce::Component, public juce::Timer
{
public:
    BeatIndicatorComponent (EngineMessageBus* messageBus, Metronome* metronomeToUse)
        : metronome (metronomeToUse), engineMessageBus (messageBus)
    {
        startTimerHz (60);
    }

    ~BeatIndicatorComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        PERFETTO_FUNCTION();

        bool enabled = metronome && metronome->isEnabled();

        if (! enabled)
        {
            // Off - dim gray, but show it's clickable
            g.setColour (LooperTheme::Colors::surface.brighter (0.2f));
            g.fillEllipse (ledBounds);

            // Optional: small dot in center to show it's interactive
            auto centerDot = ledBounds.reduced (ledBounds.getWidth() * 0.3f);
            g.setColour (LooperTheme::Colors::cyan.withAlpha (0.3f));
            g.fillEllipse (centerDot);
            return;
        }

        // Active - show with flash intensity
        juce::Colour ledColor = strongBeat ? LooperTheme::Colors::red : LooperTheme::Colors::cyan;
        float alpha = juce::jmap (flashIntensity, 0.0f, 1.0f, 0.2f, 1.0f);
        g.setColour (ledColor.withAlpha (alpha));
        g.fillEllipse (ledBounds);

        // Glow effect when bright
        if (flashIntensity > 0.5f)
        {
            float glowAlpha = (flashIntensity - 0.5f) * 0.4f;
            g.setColour (ledColor.withAlpha (glowAlpha));
            g.fillEllipse (ledBounds.expanded (3.0f));
        }

        // Visual feedback during tap tempo
        if (metronome->isTapTempoActive() && metronome->wasLastTapRecent())
        {
            // Show ring around LED during active tapping
            g.setColour (LooperTheme::Colors::yellow.withAlpha (0.5f));
            g.drawEllipse (ledBounds.expanded (2.0f), 2.0f);
        }

        g.setColour (LooperTheme::Colors::surface);
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold));
        g.drawText (juce::String (lastBeat + 1), ledBounds.toNearestInt(), juce::Justification::centred);
    }

    void resized() override
    {
        auto center = getLocalBounds().toFloat().getCentre();
        float diameter = 20.0f;
        ledBounds = juce::Rectangle<float> (center.x - diameter / 2, center.y - diameter / 2, diameter, diameter);
    }

    // void mouseDown (const juce::MouseEvent& e) override
    // {
    //     if (! metronome || ! engineMessageBus) return;
    //
    //     bool enabled = metronome->isEnabled();
    //
    //     if (! enabled)
    //     {
    //         toggleMetronome();
    //
    //         return;
    //     }
    //
    //     // Metronome is on - handle tap tempo
    //     handleTap();
    // }
    void mouseDown (const juce::MouseEvent&) override
    {
        if (! metronome || ! engineMessageBus) return;
        mouseDownTime = juce::Time::getMillisecondCounter();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! metronome || ! engineMessageBus) return;

        auto holdDuration = juce::Time::getMillisecondCounter() - mouseDownTime;
        if (holdDuration > 500) // 500ms = half second hold threshold
        {
            // Long press - disable metronome
            if (metronome->isEnabled()) toggleMetronome();
        }
        else
        {
            // Short click - normal behavior
            if (! metronome->isEnabled())
                toggleMetronome();
            else
                handleTap();
        }
    }

    void timerCallback() override
    {
        if (! metronome) return;

        bool enabled = metronome->isEnabled();

        if (! enabled)
        {
            if (flashIntensity > 0.0f)
            {
                flashIntensity = 0.0f;
                lastBeat = -1;
                repaint();
            }
            return;
        }

        // Read current beat
        int currentBeat = metronome->getCurrentBeat();

        bool needsRepaint = false;

        if (currentBeat != lastBeat)
        {
            lastBeat = currentBeat;
            strongBeat = metronome->isStrongBeat();
            flashIntensity = 1.0f;
            needsRepaint = true;
        }

        if (flashIntensity > 0.01f)
        {
            flashIntensity *= 0.85f;
            needsRepaint = true;
        }

        if (needsRepaint) repaint();
    }

private:
    void toggleMetronome()
    {
        if (! engineMessageBus) return;

        EngineMessageBus::Command cmd;
        cmd.type = EngineMessageBus::CommandType::ToggleMetronomeEnabled;
        cmd.payload = std::monostate {};
        engineMessageBus->pushCommand (cmd);
    }

    void setMetronomeBPM (int bpm)
    {
        if (! engineMessageBus) return;

        EngineMessageBus::Command cmd;
        cmd.type = EngineMessageBus::CommandType::SetMetronomeBPM;
        cmd.payload = bpm;
        engineMessageBus->pushCommand (cmd);
    }

    void handleTap()
    {
        metronome->handleTap();
        engineMessageBus->broadcastEvent ({ EngineMessageBus::EventType::MetronomeBPMChanged, -1, metronome->getBpm() });

        repaint();
    }

    Metronome* metronome;
    EngineMessageBus* engineMessageBus;
    juce::Rectangle<float> ledBounds;

    int lastBeat = -1;
    bool strongBeat = false;
    float flashIntensity = 0.0f;
    uint32_t mouseDownTime = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatIndicatorComponent)
};
