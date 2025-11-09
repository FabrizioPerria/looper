#pragma once

#include "audio/EngineCommandBus.h"
#include "audio/EngineStateToUIBridge.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class MeterWithGainComponent : public juce::Component, private juce::Timer
{
public:
    MeterWithGainComponent (const juce::String& labelText, EngineMessageBus* messageBus, EngineStateToUIBridge* bridge)
        : label (labelText), uiToEngineBus (messageBus), engineToUIBridge (bridge)
    {
        isInputMeter = (labelText == "IN");
        // Setup gain slider
        gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        gainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        gainSlider.setRange (-60.0, 12.0, 0.1);     // -60dB to +12dB
        gainSlider.setValue (0.0);                  // 0dB = unity gain
        gainSlider.setSkewFactorFromMidPoint (0.0); // Make 0dB centered
        gainSlider.onValueChange = [this]()
        {
            float gainDb = static_cast<float> (gainSlider.getValue());
            float gainLinear = juce::Decibels::decibelsToGain (gainDb);
            auto commandType = isInputMeter ? EngineMessageBus::CommandType::SetInputGain : EngineMessageBus::CommandType::SetOutputGain;
            uiToEngineBus->pushCommand (EngineMessageBus::Command ({ commandType, -1, gainLinear }));
        };
        addAndMakeVisible (gainSlider);

        startTimerHz (30);
    }

    ~MeterWithGainComponent() override { stopTimer(); }

    void resized() override
    {
        auto bounds = getLocalBounds();

        int channelLabelWidth = 12;
        int meterHeight = 14;
        int spacing = 2;

        labelBounds = bounds.removeFromTop (16);

        bounds = bounds.removeFromBottom (43);

        auto meterArea = bounds.reduced (2, 2);

        // Left channel meter (top)
        leftChannelBounds = meterArea.removeFromTop (meterHeight);
        leftLabelBounds = leftChannelBounds.removeFromLeft (channelLabelWidth);

        meterArea.removeFromTop (spacing);

        // Right channel meter (bottom)
        rightChannelBounds = meterArea.removeFromTop (meterHeight);
        rightLabelBounds = rightChannelBounds.removeFromLeft (channelLabelWidth);

        // Gain slider overlays the meter area (invisible but functional)
        gainSlider.setBounds (leftChannelBounds.getUnion (rightChannelBounds));
        gainSlider.setAlpha (0.3f); // Semi-transparent so meters show through
    }

    void paint (juce::Graphics& g) override
    {
        // Draw main label (IN/OUT)
        g.setColour (LooperTheme::Colors::cyan);
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::bold));
        g.drawText (label, labelBounds, juce::Justification::centred);

        // Draw channel labels (L/R)
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
        g.setColour (LooperTheme::Colors::text);
        g.drawText ("L", leftLabelBounds, juce::Justification::centred);
        g.drawText ("R", rightLabelBounds, juce::Justification::centred);

        // Draw meters
        drawMeter (g, leftChannelBounds, leftPeak, leftRms);
        drawMeter (g, rightChannelBounds, rightPeak, rightRms);

        // Draw gain value indicator
        drawGainIndicator (g);
    }

private:
    juce::String label;
    juce::Slider gainSlider;
    EngineMessageBus* uiToEngineBus;
    EngineStateToUIBridge* engineToUIBridge;

    // Meter levels (0.0 to 1.0+)
    float leftPeak = 0.0f;
    float leftRms = 0.0f;
    float rightPeak = 0.0f;
    float rightRms = 0.0f;

    bool isInputMeter = false;

    // Layout bounds
    juce::Rectangle<int> labelBounds;
    juce::Rectangle<int> leftChannelBounds;
    juce::Rectangle<int> leftLabelBounds;
    juce::Rectangle<int> rightChannelBounds;
    juce::Rectangle<int> rightLabelBounds;

    void timerCallback() override
    {
        if (isInputMeter)
            engineToUIBridge->getMeterInputLevels (leftPeak, leftRms, rightPeak, rightRms);
        else
            engineToUIBridge->getMeterOutputLevels (leftPeak, leftRms, rightPeak, rightRms);

        repaint();
    }

    void drawMeter (juce::Graphics& g, juce::Rectangle<int> bounds, float peak, float rms)
    {
        // Background
        g.setColour (LooperTheme::Colors::background);
        g.fillRect (bounds);

        // Convert linear to dB for display
        float peakDb = juce::Decibels::gainToDecibels (peak);
        float rmsDb = juce::Decibels::gainToDecibels (rms);

        // Define segments (dB thresholds and colors)
        struct Segment
        {
            float startDb;
            float endDb;
            juce::Colour color;
        };

        std::vector<Segment> segments = { { -60.0f, -18.0f, juce::Colours::green },
                                          { -18.0f, -6.0f, juce::Colours::yellow },
                                          { -6.0f, 0.0f, juce::Colours::orange },
                                          { 0.0f, 12.0f, juce::Colours::red } };

        int numDots = 60;                 // Number of LED dots
        float dbPerDot = 72.0f / numDots; // 72dB range (-60 to +12)
        float spacing = (float) bounds.getWidth() / numDots;
        float dotRadius = juce::jmin (spacing * 0.4f, (float) bounds.getHeight() * 0.35f); // Adaptive size

        // Draw each dot
        for (int i = 0; i < numDots; ++i)
        {
            float dotDb = -60.0f + (i * dbPerDot);
            float dotCenterDb = dotDb + (dbPerDot / 2.0f);

            // Determine dot color
            juce::Colour dotColor = juce::Colours::darkgrey;
            for (const auto& seg : segments)
            {
                if (dotCenterDb >= seg.startDb && dotCenterDb < seg.endDb)
                {
                    dotColor = seg.color;
                    break;
                }
            }

            // Check if this dot should be lit
            bool peakLit = peakDb >= dotCenterDb;
            bool rmsLit = rmsDb >= dotCenterDb;

            float dotX = bounds.getX() + (i * spacing) + (spacing / 2.0f);
            float dotY = bounds.getCentreY();

            // Draw RMS indicator (smaller, dimmer circle behind)
            if (rmsLit)
            {
                g.setColour (dotColor.withMultipliedBrightness (0.4f));
                g.fillEllipse (dotX - dotRadius * 0.7f, dotY - dotRadius * 0.7f, dotRadius * 1.4f, dotRadius * 1.4f);
            }

            // Draw peak dot (main, bright)
            if (peakLit)
            {
                g.setColour (dotColor);
                g.fillEllipse (dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);

                // Optional: add bright center/glow for active dots
                g.setColour (dotColor.brighter (0.3f));
                g.fillEllipse (dotX - dotRadius * 0.5f, dotY - dotRadius * 0.5f, dotRadius, dotRadius);
            }
            else
            {
                // Draw dim outline to show inactive dot
                g.setColour (dotColor.withAlpha (0.15f));
                g.fillEllipse (dotX - dotRadius * 0.6f, dotY - dotRadius * 0.6f, dotRadius * 1.2f, dotRadius * 1.2f);
            }
        }

        // Draw border
        g.setColour (LooperTheme::Colors::text.withAlpha (0.2f));
        g.drawRect (bounds, 1);
    }

    juce::Colour getMeterColor (float db)
    {
        if (db > 0.0f)
            return juce::Colours::red; // Clipping
        else if (db > -6.0f)
            return juce::Colours::orange; // Hot
        else if (db > -18.0f)
            return juce::Colours::yellow; // Good
        else
            return juce::Colours::green; // Low
    }

    void drawGainIndicator (juce::Graphics& g)
    {
        // Draw small gain value text below meters
        float gainDb = static_cast<float> (gainSlider.getValue());
        juce::String gainText = juce::String (gainDb, 1) + "dB";

        auto textBounds = getLocalBounds().removeFromBottom (12);
        g.setFont (juce::Font (juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.setColour (LooperTheme::Colors::text.withAlpha (0.6f));
        g.drawText (gainText, textBounds, juce::Justification::centred);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterWithGainComponent)
};
