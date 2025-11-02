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
        if (tapTempoActive && (juce::Time::getMillisecondCounter() - lastTapTime) < 500)
        {
            // Show ring around LED during active tapping
            g.setColour (LooperTheme::Colors::yellow.withAlpha (0.5f));
            g.drawEllipse (ledBounds.expanded (2.0f), 2.0f);
        }
    }

    void resized() override
    {
        auto center = getLocalBounds().toFloat().getCentre();
        float diameter = 20.0f;
        ledBounds = juce::Rectangle<float> (center.x - diameter / 2, center.y - diameter / 2, diameter, diameter);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! metronome || ! engineMessageBus) return;

        bool enabled = metronome->isEnabled();

        if (! enabled)
        {
            // Metronome is off - enable it
            enableMetronome (true);

            // Reset tap tempo state
            tapTimes.clear();
            tapTempoActive = false;
            return;
        }

        // Metronome is on - handle tap tempo
        handleTap();
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

        // Check if tap tempo has timed out
        if (tapTempoActive)
        {
            auto now = juce::Time::getMillisecondCounter();
            if (now - lastTapTime > TAP_TIMEOUT_MS)
            {
                // Timeout - reset tap tempo
                tapTimes.clear();
                tapTempoActive = false;
                needsRepaint = true;
            }
            else if (now - lastTapTime < 500)
            {
                // Still in feedback window
                needsRepaint = true;
            }
        }

        if (needsRepaint) repaint();
    }

private:
    void enableMetronome (bool enable)
    {
        if (! engineMessageBus) return;

        EngineMessageBus::Command cmd;
        cmd.type = EngineMessageBus::CommandType::SetMetronomeEnabled;
        cmd.payload = enable;
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
        auto now = juce::Time::getMillisecondCounter();

        // Check if this tap is too close to previous (debounce)
        if (! tapTimes.empty() && (now - lastTapTime) < MIN_TAP_INTERVAL_MS) return;

        lastTapTime = now;
        tapTempoActive = true;

        // Add this tap time
        tapTimes.push_back (now);

        // Remove taps older than timeout
        tapTimes.erase (std::remove_if (tapTimes.begin(),
                                        tapTimes.end(),
                                        [now] (juce::uint32 tapTime) { return (now - tapTime) > TAP_TIMEOUT_MS; }),
                        tapTimes.end());

        // Need at least 2 taps to calculate BPM
        if (tapTimes.size() < 2)
        {
            repaint();
            return;
        }

        // Calculate average interval between taps
        double totalInterval = 0.0;
        int numIntervals = 0;

        for (size_t i = 1; i < tapTimes.size(); ++i)
        {
            double interval = tapTimes[i] - tapTimes[i - 1];
            totalInterval += interval;
            numIntervals++;
        }

        double averageInterval = totalInterval / numIntervals;

        // Convert interval (milliseconds) to BPM
        // BPM = 60000 / interval_ms
        int newBPM = juce::roundToInt (60000.0 / averageInterval);

        // Clamp to valid range
        newBPM = juce::jlimit (MIN_BPM, MAX_BPM, newBPM);

        // Set the new BPM
        setMetronomeBPM (newBPM);

        DBG ("Tap Tempo: " << tapTimes.size() << " taps, "
                           << "avg interval: " << juce::String (averageInterval, 1) << "ms, "
                           << "BPM: " << newBPM);

        repaint();
    }

    Metronome* metronome;
    EngineMessageBus* engineMessageBus;
    juce::Rectangle<float> ledBounds;

    // Visual state
    int lastBeat = -1;
    bool strongBeat = false;
    float flashIntensity = 0.0f;

    // Tap tempo state
    std::vector<juce::uint32> tapTimes;
    juce::uint32 lastTapTime = 0;
    bool tapTempoActive = false;

    // Tap tempo configuration
    static constexpr int MIN_BPM = 30;
    static constexpr int MAX_BPM = 300;
    static constexpr int TAP_TIMEOUT_MS = 3000;     // Reset after 3 seconds of no taps
    static constexpr int MIN_TAP_INTERVAL_MS = 100; // Debounce - ignore taps closer than 100ms

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatIndicatorComponent)
};
