#pragma once
#include "audio/AudioToUIBridge.h"
#include "audio/EngineCommandBus.h"
#include "engine/Constants.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

// Data structure to store the speed automation curve
struct ProgressiveMetronomeCurve
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

    std::vector<juce::Point<float>> breakpoints;

private:
    int currentStep = 0;
};

class ProgressiveMetronomeGraph : public juce::Component
{
public:
    ProgressiveMetronomeGraph() { setInterceptsMouseClicks (true, true); }

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
        int numSpeedLabels = 10;
        const float speedStep = (METRONOME_MAX_BPM - METRONOME_MIN_BPM) / (numSpeedLabels - 1);
        for (int i = 0; i < numSpeedLabels; ++i)
        {
            float speed = METRONOME_MIN_BPM + i * speedStep;
            float y = speedToY (speed, bounds);
            g.drawText (juce::String (speed, 2) + "x", juce::Rectangle<float> (2, y - 8, 35, 16), juce::Justification::centredLeft);
        }

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

    float speedToY (float speed, juce::Rectangle<float> bounds)
    {
        return juce::jmap (speed, METRONOME_MIN_BPM, METRONOME_MAX_BPM, bounds.getBottom() - 20.0f, bounds.getY() + 20.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressiveMetronomeGraph)
};

class ProgressiveMetronomePopup : public juce::Component
{
public:
    ProgressiveMetronomePopup (ProgressiveMetronomeCurve curve, EngineMessageBus* messageBus) : uiToEngineBus (messageBus)
    {
        // Preset buttons
        flatButton.setComponentID ("flatButton");
        flatButton.setButtonText ("FLAT");
        flatButton.onClick = [this]() { selectPreset (ProgressiveMetronomeCurve::PresetType::Flat); };
        addAndMakeVisible (flatButton);

        twoFBButton.setComponentID ("twoFBButton");
        twoFBButton.setButtonText ("2F-1B");
        twoFBButton.onClick = [this]() { selectPreset (ProgressiveMetronomeCurve::PresetType::TwoForwardOneBack); };
        addAndMakeVisible (twoFBButton);

        linearButton.setComponentID ("linearButton");
        linearButton.setButtonText ("LINEAR");
        linearButton.onClick = [this]() { selectPreset (ProgressiveMetronomeCurve::PresetType::LinearRamp); };
        addAndMakeVisible (linearButton);

        // Duration control
        durationLabel.setText ("Duration (min):", juce::dontSendNotification);
        durationLabel.setFont (LooperTheme::Fonts::getBoldFont (11.0f));
        durationLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (durationLabel);

        durationSlider.setRange (1.0, 60.0, 1.0);
        durationSlider.setValue (10.0);
        durationSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        durationSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 50, 20);
        durationSlider.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (durationSlider);
        durationSlider.setValue (curve.durationMinutes);

        // Parameter knobs
        startSpeedKnob.setRange (METRONOME_MIN_BPM, METRONOME_MAX_BPM, 1.0);
        startSpeedKnob.setValue (METRONOME_DEFAULT_BPM);
        startSpeedKnob.setSliderStyle (juce::Slider::LinearHorizontal);
        startSpeedKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 60, 20);
        startSpeedKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (startSpeedKnob);
        startSpeedKnob.setValue (curve.startSpeed);

        startSpeedLabel.setText ("Start Speed", juce::dontSendNotification);
        startSpeedLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        startSpeedLabel.setJustificationType (juce::Justification::centred);
        startSpeedLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (startSpeedLabel);

        endSpeedKnob.setRange (METRONOME_MIN_BPM, METRONOME_MAX_BPM, 1.0);
        endSpeedKnob.setValue (METRONOME_DEFAULT_BPM);
        endSpeedKnob.setSliderStyle (juce::Slider::LinearHorizontal);
        endSpeedKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 60, 20);
        endSpeedKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (endSpeedKnob);
        endSpeedKnob.setValue (curve.endSpeed);

        endSpeedLabel.setText ("End Speed", juce::dontSendNotification);
        endSpeedLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        endSpeedLabel.setJustificationType (juce::Justification::centred);
        endSpeedLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (endSpeedLabel);

        stepSizeKnob.setRange (1.0, 10.0, 1.0);
        stepSizeKnob.setValue (1.0);
        stepSizeKnob.setSliderStyle (juce::Slider::LinearHorizontal);
        stepSizeKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 60, 20);
        stepSizeKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (stepSizeKnob);
        stepSizeKnob.setValue (curve.stepSize);

        stepSizeLabel.setText ("Step Size", juce::dontSendNotification);
        stepSizeLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        stepSizeLabel.setJustificationType (juce::Justification::centred);
        stepSizeLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (stepSizeLabel);

        repsPerLevelKnob.setRange (1, 40, 1);
        repsPerLevelKnob.setValue (1);
        repsPerLevelKnob.setSliderStyle (juce::Slider::LinearHorizontal);
        repsPerLevelKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 60, 20);
        repsPerLevelKnob.onValueChange = [this]() { updateCurve(); };
        addAndMakeVisible (repsPerLevelKnob);
        repsPerLevelKnob.setValue (curve.repsPerStep);

        repsPerLevelLabel.setText ("Reps/Step", juce::dontSendNotification);
        repsPerLevelLabel.setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        repsPerLevelLabel.setJustificationType (juce::Justification::centred);
        repsPerLevelLabel.setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        addAndMakeVisible (repsPerLevelLabel);

        // Graph
        addAndMakeVisible (graph);

        // Action buttons
        cancelButton.setComponentID ("cancelButton");
        cancelButton.setButtonText ("Cancel");
        cancelButton.onClick = [this]() { closePopup (false); };
        addAndMakeVisible (cancelButton);

        startButton.setComponentID ("startButton");
        startButton.setButtonText ("Start Practice");
        startButton.onClick = [this]() { closePopup (true); };
        addAndMakeVisible (startButton);

        selectPreset (curve.preset);
        setWantsKeyboardFocus (true);
        grabKeyboardFocus();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closePopup (false);
        }
        return true;
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

        juce::FlexBox mainFlex;
        mainFlex.flexDirection = juce::FlexBox::Direction::column;
        mainFlex.alignItems = juce::FlexBox::AlignItems::stretch;

        // Preset buttons row
        juce::FlexBox presetRow;
        presetRow.flexDirection = juce::FlexBox::Direction::row;
        mainFlex.alignItems = juce::FlexBox::AlignItems::stretch;
        presetRow.items.add (juce::FlexItem (flatButton).withFlex (1).withMargin (2));
        presetRow.items.add (juce::FlexItem (twoFBButton).withFlex (1).withMargin (2));
        presetRow.items.add (juce::FlexItem (linearButton).withFlex (1).withMargin (2));
        mainFlex.items.add (juce::FlexItem (presetRow).withHeight (35));

        mainFlex.items.add (juce::FlexItem().withHeight (10));

        // Duration control
        juce::FlexBox durationRow;
        durationRow.flexDirection = juce::FlexBox::Direction::row;
        durationRow.items.add (juce::FlexItem (durationLabel).withWidth (120));
        durationRow.items.add (juce::FlexItem (durationSlider).withFlex (1));
        mainFlex.items.add (juce::FlexItem (durationRow).withHeight (25));

        mainFlex.items.add (juce::FlexItem().withHeight (15));

        // Parameter knobs row
        juce::FlexBox knobsRow;
        knobsRow.flexDirection = juce::FlexBox::Direction::row;

        juce::FlexBox startCol;
        startCol.flexDirection = juce::FlexBox::Direction::column;
        startCol.items.add (juce::FlexItem (startSpeedLabel).withHeight (15));
        startCol.items.add (juce::FlexItem (startSpeedKnob).withFlex (1));
        knobsRow.items.add (juce::FlexItem (startCol).withFlex (1).withMargin (5));

        juce::FlexBox endCol;
        endCol.flexDirection = juce::FlexBox::Direction::column;
        endCol.items.add (juce::FlexItem (endSpeedLabel).withHeight (15));
        endCol.items.add (juce::FlexItem (endSpeedKnob).withFlex (1));
        knobsRow.items.add (juce::FlexItem (endCol).withFlex (1).withMargin (5));

        juce::FlexBox stepCol;
        stepCol.flexDirection = juce::FlexBox::Direction::column;
        stepCol.items.add (juce::FlexItem (stepSizeLabel).withHeight (15));
        stepCol.items.add (juce::FlexItem (stepSizeKnob).withFlex (1));
        knobsRow.items.add (juce::FlexItem (stepCol).withFlex (1).withMargin (5));

        juce::FlexBox repsCol;
        repsCol.flexDirection = juce::FlexBox::Direction::column;
        repsCol.items.add (juce::FlexItem (repsPerLevelLabel).withHeight (15));
        repsCol.items.add (juce::FlexItem (repsPerLevelKnob).withFlex (1));
        knobsRow.items.add (juce::FlexItem (repsCol).withFlex (1).withMargin (5));

        mainFlex.items.add (juce::FlexItem (knobsRow).withHeight (90));

        mainFlex.items.add (juce::FlexItem().withHeight (10));

        mainFlex.items.add (juce::FlexItem (graph).withHeight (200));

        mainFlex.items.add (juce::FlexItem().withHeight (15));

        // Action buttons
        juce::FlexBox buttonRow;
        buttonRow.flexDirection = juce::FlexBox::Direction::row;
        buttonRow.alignItems = juce::FlexBox::AlignItems::stretch;
        buttonRow.items.add (juce::FlexItem (startButton).withFlex (1).withMargin (2));
        buttonRow.items.add (juce::FlexItem (cancelButton).withFlex (1).withMargin (2));
        mainFlex.items.add (juce::FlexItem (buttonRow).withHeight (35));

        mainFlex.performLayout (dialogBounds.toFloat());
    }

    std::function<void (const ProgressiveMetronomeCurve&)> onStart;
    std::function<void()> onCancel;

private:
    EngineMessageBus* uiToEngineBus;
    ProgressiveMetronomeCurve currentCurve;

    juce::TextButton flatButton, twoFBButton, linearButton;
    juce::Label durationLabel;
    juce::Slider durationSlider;
    juce::Slider startSpeedKnob, endSpeedKnob, stepSizeKnob, repsPerLevelKnob;
    juce::Label startSpeedLabel, endSpeedLabel, stepSizeLabel, repsPerLevelLabel;
    ProgressiveMetronomeGraph graph;
    juce::TextButton cancelButton, startButton;

    juce::Rectangle<int> getDialogBounds()
    {
        auto bounds = getLocalBounds();
        int width = 600;
        int height = 550;
        return bounds.withSizeKeepingCentre (width, height);
    }

    void selectPreset (ProgressiveMetronomeCurve::PresetType preset)
    {
        currentCurve.preset = preset;

        // Update button states
        flatButton.setToggleState (preset == ProgressiveMetronomeCurve::PresetType::Flat, juce::dontSendNotification);
        twoFBButton.setToggleState (preset == ProgressiveMetronomeCurve::PresetType::TwoForwardOneBack, juce::dontSendNotification);
        linearButton.setToggleState (preset == ProgressiveMetronomeCurve::PresetType::LinearRamp, juce::dontSendNotification);

        // Show/hide relevant parameters
        bool show2FB = (preset == ProgressiveMetronomeCurve::PresetType::TwoForwardOneBack);
        stepSizeKnob.setVisible (show2FB);
        stepSizeLabel.setVisible (show2FB);

        bool showFlat = (preset == ProgressiveMetronomeCurve::PresetType::Flat);
        startSpeedKnob.setVisible (! showFlat);
        startSpeedLabel.setVisible (! showFlat);
        repsPerLevelKnob.setVisible (! showFlat);
        repsPerLevelLabel.setVisible (! showFlat);
        endSpeedLabel.setText (showFlat ? "Speed" : "End Speed", juce::dontSendNotification);

        updateCurve();
        repaint();
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

        float loopLengthSeconds = 60;
        int numLoops = (int) ((currentCurve.durationMinutes * 60.0f) / loopLengthSeconds);

        switch (currentCurve.preset)
        {
            case ProgressiveMetronomeCurve::PresetType::Flat:
            {
                for (int i = 0; i < numLoops; ++i)
                    currentCurve.breakpoints.push_back ({ (float) i, currentCurve.endSpeed });
                break;
            }

            case ProgressiveMetronomeCurve::PresetType::TwoForwardOneBack:
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

            case ProgressiveMetronomeCurve::PresetType::LinearRamp:
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
            updateCurve();
            onStart (currentCurve);
        }
        else if (onCancel)
        {
            onCancel();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressiveMetronomePopup)
};
