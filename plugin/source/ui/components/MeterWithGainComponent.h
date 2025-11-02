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

        labelBounds = bounds.removeFromTop (JUCE_LIVE_CONSTANT (16));

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
        auto bounds = getLocalBounds().toFloat();

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

        // Map dB range (-60 to +12) to pixel width
        auto dbToWidth = [&] (float db)
        {
            float normalized = juce::jmap (db, -60.0f, 12.0f, 0.0f, 1.0f);
            return juce::jlimit (0.0f, 1.0f, normalized) * bounds.getWidth();
        };

        float peakWidth = dbToWidth (peakDb);
        float rmsWidth = dbToWidth (rmsDb);

        // Draw RMS bar first (full height, SOLID darker color)
        if (rmsWidth > 0)
        {
            auto rmsRect = bounds.withWidth (static_cast<int> (rmsWidth));
            g.setColour (getMeterColor (rmsDb).withMultipliedBrightness (0.5f)); // 50% darker
            g.fillRect (rmsRect);
        }

        // Draw PEAK bar on top (75% height, centered, bright gradient)
        if (peakWidth > 0)
        {
            int peakHeight = bounds.getHeight() * 0.75f;
            int peakY = bounds.getCentreY() - peakHeight / 2;
            auto peakRect = juce::Rectangle<int> (bounds.getX(), peakY, static_cast<int> (peakWidth), peakHeight);

            juce::ColourGradient
                gradient (getMeterColor (-20.0f), peakRect.getX(), 0, getMeterColor (peakDb), peakRect.getRight(), 0, false);
            gradient.addColour (0.5, getMeterColor (-10.0f));
            g.setGradientFill (gradient);
            g.fillRect (peakRect);
        }

        // Draw 0dB marker
        float zeroDB = dbToWidth (0.0f);
        g.setColour (LooperTheme::Colors::text.withAlpha (0.3f));
        g.drawVerticalLine (bounds.getX() + static_cast<int> (zeroDB), bounds.getY(), bounds.getBottom());

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
