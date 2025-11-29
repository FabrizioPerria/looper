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
        LinearRamp,
        Plateau
    };

    PresetType preset = PresetType::Flat;
    float durationMinutes = 10.0f;
    float startSpeed = 0.7f;
    float endSpeed = 1.0f;
    float stepSize = 0.03f;
    float baseSpeedOffset = 0.0f;

    std::vector<juce::Point<float>> breakpoints; // x = loop repetition number, y = speed multiplier

    bool isActive = false;
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
        const float speeds[] = { 0.5f, 0.75f, 1.0f, 1.25f, 1.5f };
        for (auto speed : speeds)
        {
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
        // Map speed range 0.5-1.5 to vertical space
        return juce::jmap (speed, 0.5f, 1.5f, bounds.getBottom() - 20.0f, bounds.getY() + 20.0f);
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

        plateauButton.setButtonText ("PLATEAU");
        plateauButton.onClick = [this]() { selectPreset (ProgressiveSpeedCurve::PresetType::Plateau); };
        addAndMakeVisible (plateauButton);

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
        plateauButton.setBounds (presetRow.removeFromLeft (buttonWidth).reduced (2));

        dialogBounds.removeFromTop (10);

        // Duration control
        auto durationRow = dialogBounds.removeFromTop (25);
        durationLabel.setBounds (durationRow.removeFromLeft (120));
        durationSlider.setBounds (durationRow);

        dialogBounds.removeFromTop (15);

        // Parameter knobs row
        auto knobsRow = dialogBounds.removeFromTop (90);
        int knobWidth = knobsRow.getWidth() / 3;

        auto startCol = knobsRow.removeFromLeft (knobWidth).reduced (5);
        startSpeedLabel.setBounds (startCol.removeFromTop (15));
        startSpeedKnob.setBounds (startCol);

        auto endCol = knobsRow.removeFromLeft (knobWidth).reduced (5);
        endSpeedLabel.setBounds (endCol.removeFromTop (15));
        endSpeedKnob.setBounds (endCol);

        auto stepCol = knobsRow.removeFromLeft (knobWidth).reduced (5);
        stepSizeLabel.setBounds (stepCol.removeFromTop (15));
        stepSizeKnob.setBounds (stepCol);

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

    juce::TextButton flatButton, twoFBButton, linearButton, plateauButton;
    juce::Label durationLabel;
    juce::Slider durationSlider;
    juce::Slider startSpeedKnob, endSpeedKnob, stepSizeKnob;
    juce::Label startSpeedLabel, endSpeedLabel, stepSizeLabel;
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
        plateauButton.setToggleState (preset == ProgressiveSpeedCurve::PresetType::Plateau, juce::dontSendNotification);

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
                for (int i = 0; i < numLoops; ++i)
                {
                    currentCurve.breakpoints.push_back ({ (float) i, currentSpeed });

                    int posInCycle = i % 3;
                    if (posInCycle == 0 || posInCycle == 1)
                        currentSpeed = juce::jmin (currentSpeed + currentCurve.stepSize, currentCurve.endSpeed);
                    else
                        currentSpeed = juce::jmax (currentSpeed - currentCurve.stepSize, currentCurve.startSpeed);
                }
                break;
            }

            case ProgressiveSpeedCurve::PresetType::LinearRamp:
            {
                for (int i = 0; i < numLoops; ++i)
                {
                    float progress = (float) i / (float) (numLoops - 1);
                    float speed = juce::jmap (progress, currentCurve.startSpeed, currentCurve.endSpeed);
                    currentCurve.breakpoints.push_back ({ (float) i, speed });
                }
                break;
            }

            case ProgressiveSpeedCurve::PresetType::Plateau:
            {
                int numPlateaus = 4;
                int loopsPerPlateau = numLoops / numPlateaus;
                for (int i = 0; i < numLoops; ++i)
                {
                    int plateau = i / loopsPerPlateau;
                    float progress = (float) plateau / (float) (numPlateaus - 1);
                    float speed = juce::jmap (progress, currentCurve.startSpeed, currentCurve.endSpeed);
                    currentCurve.breakpoints.push_back ({ (float) i, speed });
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
