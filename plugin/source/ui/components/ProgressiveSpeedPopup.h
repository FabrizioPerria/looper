// ProgressiveSpeedPopup.h
#pragma once
#include "audio/EngineCommandBus.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

// Data structure to store the speed automation curve
struct ProgressiveSpeedCurve
{
    enum class PresetType
    {
        Flat,
        TwoForwardOneBack,
        LinearRamp
    };

    PresetType preset = PresetType::Flat;
    float durationMinutes = 10.0f;
    float startSpeed = 0.7f;
    float endSpeed = 1.0f;
    float stepSize = 0.03f;
    int repsPerStep = 2.0f;
    float baseSpeedOffset = 0.0f;

    std::vector<juce::Point<float>> breakpoints; // x = loop repetition number, y = speed multiplier

    bool isActive = false;

private:
    int currentStep = 0;
};

class ProgressiveSpeedGraph : public juce::Component
{
public:
    ProgressiveSpeedGraph() { setInterceptsMouseClicks (true, true); }

    void setCurve (const std::vector<juce::Point<float>>& points)
    {
        breakpoints = points;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Draw background
        g.setColour (LooperTheme::Colors::backgroundDark);
        g.fillRoundedRectangle (bounds, 4.0f);

        // Draw grid lines
        g.setColour (LooperTheme::Colors::border);
        for (int i = 1; i < 4; ++i)
        {
            float y = bounds.getY() + (bounds.getHeight() * i / 4.0f);
            g.drawLine (bounds.getX(), y, bounds.getRight(), y, 0.5f);
        }

        // Draw axis labels
        g.setColour (LooperTheme::Colors::textDim);
        g.setFont (LooperTheme::Fonts::getRegularFont (10.0f));

        // Y-axis speed labels
        int numSpeedLabels = 5;
        const float speedStep = (MAX_SPEED - MIN_SPEED) / (numSpeedLabels - 1);
        for (int i = 0; i < numSpeedLabels; ++i)
        {
            float speed = MIN_SPEED + i * speedStep;
            float y = speedToY (speed, bounds);
            g.drawText (juce::String (speed, 2) + "x", juce::Rectangle<float> (2, y - 8, 35, 16), juce::Justification::centredLeft);
        }

        // const float speeds[] = { MIN_SPEED, 0.75f, 1.0f, 1.25f, MAX_SPEED };
        // for (auto speed : speeds)
        // {
        //     float y = speedToY (speed, bounds);
        //     g.drawText (juce::String (speed, 2) + "x", juce::Rectangle<float> (2, y - 8, 35, 16), juce::Justification::centredLeft);
        // }

        // Draw curve
        if (breakpoints.size() >= 2)
        {
            juce::Path curvePath;
            bool first = true;

            for (const auto& point : breakpoints)
            {
                float x = juce::jmap (point.x, 0.0f, (float) (breakpoints.size() - 1), bounds.getX() + 40, bounds.getRight() - 10);
                float y = speedToY (point.y, bounds);

                if (first)
                {
                    curvePath.startNewSubPath (x, y);
                    first = false;
                }
                else
                {
                    curvePath.lineTo (x, y);
                }
            }

            // Draw the line
            g.setColour (LooperTheme::Colors::cyan);
            g.strokePath (curvePath, juce::PathStrokeType (2.0f));

            // Draw points
            g.setColour (LooperTheme::Colors::cyan);
            for (const auto& point : breakpoints)
            {
                float x = juce::jmap (point.x, 0.0f, (float) (breakpoints.size() - 1), bounds.getX() + 40, bounds.getRight() - 10);
                float y = speedToY (point.y, bounds);
                g.fillEllipse (x - 3, y - 3, 6, 6);
            }
        }
    }

private:
    std::vector<juce::Point<float>> breakpoints;
    float MIN_SPEED = 0.5f;
    float MAX_SPEED = 1.25f;

    float speedToY (float speed, juce::Rectangle<float> bounds)
    {
        // Map speed range 0.5-1.5 to vertical space
        return juce::jmap (speed, MIN_SPEED, MAX_SPEED, bounds.getBottom() - 20.0f, bounds.getY() + 20.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressiveSpeedGraph)
};

class ProgressiveSpeedPopup : public juce::Component
{
public:
    ProgressiveSpeedPopup (int trackIdx, EngineMessageBus* messageBus) : trackIndex (trackIdx), uiToEngineBus (messageBus)
    {
        // Preset buttons
        flatButton.setButtonText ("FLAT");
        flatButton.onClick = [this]() { selectPreset (ProgressiveSpeedCurve::PresetType::Flat); };
        addAndMakeVisible (flatButton);

        twoFBButton.setButtonText ("2F-1B");
        twoFBButton.onClick = [this]() { selectPreset (ProgressiveSpeedCurve::PresetType::TwoForwardOneBack); };
        addAndMakeVisible (twoFBButton);

        linearButton.setButtonText ("LINEAR");
        linearButton.onClick = [this]() { selectPreset (ProgressiveSpeedCurve::PresetType::LinearRamp); };
        addAndMakeVisible (linearButton);

        // Duration control
        durationLabel.setText ("Duration (min):", juce::dontSendNotification);
        durationLabel.setFont (LooperTheme::Fonts::getBoldFont (11.0f));
        durationLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (durationLabel);

        durationSlider.setRange (1.0, 60.0, 1.0);
        durationSlider.setValue (10.0);
        durationSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        durationSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
        durationSlider.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (durationSlider);

        // Parameter knobs
        startSpeedKnob.setRange (0.5, 2.0, 0.01);
        startSpeedKnob.setValue (0.7);
        startSpeedKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        startSpeedKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        startSpeedKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (startSpeedKnob);

        startSpeedLabel.setText ("Start Speed", juce::dontSendNotification);
        startSpeedLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        startSpeedLabel.setJustificationType (juce::Justification::centred);
        startSpeedLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (startSpeedLabel);

        endSpeedKnob.setRange (0.5, 2.0, 0.01);
        endSpeedKnob.setValue (1.0);
        endSpeedKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        endSpeedKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        endSpeedKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (endSpeedKnob);

        endSpeedLabel.setText ("End Speed", juce::dontSendNotification);
        endSpeedLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        endSpeedLabel.setJustificationType (juce::Justification::centred);
        endSpeedLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (endSpeedLabel);

        stepSizeKnob.setRange (0.01, 0.10, 0.01);
        stepSizeKnob.setValue (0.03);
        stepSizeKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        stepSizeKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        stepSizeKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (stepSizeKnob);

        stepSizeLabel.setText ("Step Size", juce::dontSendNotification);
        stepSizeLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        stepSizeLabel.setJustificationType (juce::Justification::centred);
        stepSizeLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (stepSizeLabel);

        repsPerLevelKnob.setRange (1, 10, 1);
        repsPerLevelKnob.setValue (1);
        repsPerLevelKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        repsPerLevelKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        repsPerLevelKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (repsPerLevelKnob);

        repsPerLevelLabel.setText ("Reps/Level", juce::dontSendNotification);
        repsPerLevelLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        repsPerLevelLabel.setJustificationType (juce::Justification::centred);
        repsPerLevelLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (repsPerLevelLabel);

        // Graph
        addAndMakeVisible (graph);

        // Action buttons
        cancelButton.setButtonText ("Cancel");
        cancelButton.onClick = [this]() { closePopup (false); };
        addAndMakeVisible (cancelButton);

        startButton.setButtonText ("Start Practice");
        startButton.onClick = [this]() { closePopup (true); };
        addAndMakeVisible (startButton);

        // Initialize with flat preset
        selectPreset (ProgressiveSpeedCurve::PresetType::Flat);
    }

    void paint (juce::Graphics& g) override
    {
        // Semi-transparent overlay
        g.fillAll (juce::Colours::black.withAlpha (0.7f));

        // Popup dialog background
        auto dialogBounds = getDialogBounds();
        g.setColour (LooperTheme::Colors::surface);
        g.fillRoundedRectangle (dialogBounds.toFloat(), 8.0f);

        g.setColour (LooperTheme::Colors::cyan);
        g.drawRoundedRectangle (dialogBounds.toFloat(), 8.0f, 2.0f);

        // Title
        g.setColour (LooperTheme::Colors::cyan);
        g.setFont (LooperTheme::Fonts::getBoldFont (16.0f));
        g.drawText ("Progressive Speed Practice", dialogBounds.removeFromTop (40), juce::Justification::centred);
    }

    void resized() override
    {
        auto dialogBounds = getDialogBounds();
        dialogBounds.removeFromTop (40); // Title space
        dialogBounds.reduce (20, 10);

        // Preset buttons row
        auto presetRow = dialogBounds.removeFromTop (35);
        int buttonWidth = presetRow.getWidth() / 4;
        flatButton.setBounds (presetRow.removeFromLeft (buttonWidth).reduced (2));
        twoFBButton.setBounds (presetRow.removeFromLeft (buttonWidth).reduced (2));
        linearButton.setBounds (presetRow.removeFromLeft (buttonWidth).reduced (2));

        dialogBounds.removeFromTop (10);

        // Duration control
        auto durationRow = dialogBounds.removeFromTop (25);
        durationLabel.setBounds (durationRow.removeFromLeft (120));
        durationSlider.setBounds (durationRow);

        dialogBounds.removeFromTop (15);

        // Parameter knobs row
        auto knobsRow = dialogBounds.removeFromTop (90);
        int knobWidth = knobsRow.getWidth() / 4;

        auto startCol = knobsRow.removeFromLeft (knobWidth).reduced (5);
        startSpeedLabel.setBounds (startCol.removeFromTop (15));
        startSpeedKnob.setBounds (startCol);

        auto endCol = knobsRow.removeFromLeft (knobWidth).reduced (5);
        endSpeedLabel.setBounds (endCol.removeFromTop (15));
        endSpeedKnob.setBounds (endCol);

        auto stepCol = knobsRow.removeFromLeft (knobWidth).reduced (5);
        stepSizeLabel.setBounds (stepCol.removeFromTop (15));
        stepSizeKnob.setBounds (stepCol);

        auto repsCol = knobsRow.reduced (5);
        repsPerLevelLabel.setBounds (repsCol.removeFromTop (15));
        repsPerLevelKnob.setBounds (repsCol);

        dialogBounds.removeFromTop (10);

        // Graph
        auto graphArea = dialogBounds.removeFromTop (200);
        graph.setBounds (graphArea);

        dialogBounds.removeFromTop (15);

        // Action buttons
        auto buttonRow = dialogBounds.removeFromTop (35);
        cancelButton.setBounds (buttonRow.removeFromLeft (120).reduced (2));
        buttonRow.removeFromLeft (10);
        startButton.setBounds (buttonRow.removeFromLeft (150).reduced (2));
    }

    std::function<void (const ProgressiveSpeedCurve&)> onStart;
    std::function<void()> onCancel;

private:
    int trackIndex;
    EngineMessageBus* uiToEngineBus;
    ProgressiveSpeedCurve currentCurve;

    juce::TextButton flatButton, twoFBButton, linearButton;
    juce::Label durationLabel;
    juce::Slider durationSlider;
    juce::Slider startSpeedKnob, endSpeedKnob, stepSizeKnob, repsPerLevelKnob;
    juce::Label startSpeedLabel, endSpeedLabel, stepSizeLabel, repsPerLevelLabel;
    ProgressiveSpeedGraph graph;
    juce::TextButton cancelButton, startButton;

    juce::Rectangle<int> getDialogBounds()
    {
        auto bounds = getLocalBounds();
        int width = 600;
        int height = 550;
        return bounds.withSizeKeepingCentre (width, height);
    }

    void selectPreset (ProgressiveSpeedCurve::PresetType preset)
    {
        currentCurve.preset = preset;

        // Update button states
        flatButton.setToggleState (preset == ProgressiveSpeedCurve::PresetType::Flat, juce::dontSendNotification);
        twoFBButton.setToggleState (preset == ProgressiveSpeedCurve::PresetType::TwoForwardOneBack, juce::dontSendNotification);
        linearButton.setToggleState (preset == ProgressiveSpeedCurve::PresetType::LinearRamp, juce::dontSendNotification);

        // Show/hide relevant parameters
        bool show2FB = (preset == ProgressiveSpeedCurve::PresetType::TwoForwardOneBack);
        stepSizeKnob.setVisible (show2FB);
        stepSizeLabel.setVisible (show2FB);

        updateCurve();
    }

    void updateCurve()
    {
        currentCurve.durationMinutes = (float) durationSlider.getValue();
        currentCurve.startSpeed = (float) startSpeedKnob.getValue();
        currentCurve.endSpeed = (float) endSpeedKnob.getValue();
        currentCurve.stepSize = (float) stepSizeKnob.getValue();
        currentCurve.repsPerStep = (int) repsPerLevelKnob.getValue();

        generateBreakpoints();
        graph.setCurve (currentCurve.breakpoints);
    }

    void generateBreakpoints()
    {
        currentCurve.breakpoints.clear();

        // For now, assume 16-second loops (we'll need actual loop length from backend)
        float loopLengthSeconds = 16.0f;
        int numLoops = (int) ((currentCurve.durationMinutes * 60.0f) / loopLengthSeconds);

        switch (currentCurve.preset)
        {
            case ProgressiveSpeedCurve::PresetType::Flat:
            {
                for (int i = 0; i < numLoops; ++i)
                    currentCurve.breakpoints.push_back ({ (float) i, 1.0f });
                break;
            }

            case ProgressiveSpeedCurve::PresetType::TwoForwardOneBack:
            {
                float currentSpeed = currentCurve.startSpeed;
                int loopIndex = 0;

                while (loopIndex < numLoops)
                {
                    // Calculate what the next speed should be based on pattern
                    int patternPos = (loopIndex / currentCurve.repsPerStep) % 3;

                    // Determine speed change
                    if (patternPos == 0 || patternPos == 1)
                    {
                        // Going forward
                        if (loopIndex > 0 && (loopIndex / currentCurve.repsPerStep) > 0)
                            currentSpeed = juce::jmin (currentSpeed + currentCurve.stepSize, currentCurve.endSpeed);
                    }
                    else // patternPos == 2
                    {
                        // Going back
                        currentSpeed = juce::jmax (currentSpeed - currentCurve.stepSize, currentCurve.startSpeed);
                    }

                    // Repeat this speed repsPerLevel times
                    for (int rep = 0; rep < currentCurve.repsPerStep && loopIndex < numLoops; ++rep, ++loopIndex)
                    {
                        currentCurve.breakpoints.push_back ({ (float) loopIndex, currentSpeed });
                    }
                }
                break;
            }

            case ProgressiveSpeedCurve::PresetType::LinearRamp:
            {
                // Calculate number of unique speed levels
                int numLevels = numLoops / currentCurve.repsPerStep;
                if (numLoops % currentCurve.repsPerStep != 0) numLevels++; // Round up

                int loopIndex = 0;
                for (int level = 0; level < numLevels && loopIndex < numLoops; ++level)
                {
                    float progress = (float) level / (float) (numLevels - 1);
                    float speed = juce::jmap (progress, currentCurve.startSpeed, currentCurve.endSpeed);

                    // Repeat this speed level repsPerLevel times
                    for (int rep = 0; rep < currentCurve.repsPerStep && loopIndex < numLoops; ++rep, ++loopIndex)
                    {
                        currentCurve.breakpoints.push_back ({ (float) loopIndex, speed });
                    }
                }
                break;
            }
        }
    }

    void closePopup (bool shouldStart)
    {
        if (shouldStart && onStart)
        {
            currentCurve.isActive = true;
            onStart (currentCurve);
        }
        else if (onCancel)
        {
            onCancel();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressiveSpeedPopup)
};
