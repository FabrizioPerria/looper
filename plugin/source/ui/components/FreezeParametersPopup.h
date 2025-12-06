// FreezeParametersPopup.h
#pragma once

#include "audio/EngineCommandBus.h"
#include "engine/Constants.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

struct FreezeParameters
{
    float grainLengthMs = 150.0f;
    int grainSpacing = 256;
    int maxGrains = 64;
    float positionSpread = 0.3f;
    float modRate = 0.04f;
    float pitchModDepth = 0.005f;
    float ampModDepth = 0.01f;
    float grainRandomness = 0.3f;
};

class FreezeParametersPopup : public juce::Component
{
public:
    FreezeParametersPopup (EngineMessageBus* bus, const FreezeParameters& currentParams) : messageBus (bus), currentParams (currentParams)
    {
        setupSlider (grainLengthSlider, "Grain Length (ms)", 50.0, 500.0, currentParams.grainLengthMs);
        setupSlider (grainSpacingSlider, "Grain Spacing", 64.0, 2048.0, currentParams.grainSpacing);
        setupSlider (maxGrainsSlider, "Max Grains", 16.0, 128.0, currentParams.maxGrains);
        setupSlider (positionSpreadSlider, "Position Spread", 0.0, 1.0, currentParams.positionSpread);
        setupSlider (modRateSlider, "Mod Rate (Hz)", 0.01, 0.2, currentParams.modRate);
        setupSlider (pitchModDepthSlider, "Pitch Mod Depth", 0.0, 0.1, currentParams.pitchModDepth);
        setupSlider (ampModDepthSlider, "Amp Mod Depth", 0.0, 0.05, currentParams.ampModDepth);
        setupSlider (grainRandomnessSlider, "Grain Randomness", 0.0, 0.8, currentParams.grainRandomness);

        grainSpacingSlider.setSkewFactorFromMidPoint (512.0);
        maxGrainsSlider.setSkewFactorFromMidPoint (48.0);

        applyButton.setButtonText ("Apply");
        applyButton.onClick = [this]() { closePopup (true); };
        addAndMakeVisible (applyButton);

        cancelButton.setButtonText ("Cancel");
        cancelButton.onClick = [this]() { closePopup (false); };
        addAndMakeVisible (cancelButton);

        setWantsKeyboardFocus (true);
        grabKeyboardFocus();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closePopup (false);
            return true;
        }
        return false;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.7f));

        auto dialogBounds = getDialogBounds();
        g.setColour (LooperTheme::Colors::surface);
        g.fillRoundedRectangle (dialogBounds.toFloat(), 8.0f);

        g.setColour (LooperTheme::Colors::cyan);
        g.drawRoundedRectangle (dialogBounds.toFloat(), 8.0f, 2.0f);

        g.setColour (LooperTheme::Colors::cyan);
        g.setFont (LooperTheme::Fonts::getBoldFont (16.0f));
        g.drawText ("Freeze Parameters", dialogBounds.removeFromTop (40), juce::Justification::centred);
    }

    void resized() override
    {
        auto dialogBounds = getDialogBounds();
        auto contentBounds = dialogBounds;
        contentBounds.removeFromTop (40);
        contentBounds.reduce (20, 10);

        int sliderHeight = 30;
        int spacing = 5;

        auto bounds = contentBounds;

        for (int i = 0; i < 8; ++i)
        {
            auto row = bounds.removeFromTop (sliderHeight);
            if (i < labels.size()) labels[i]->setBounds (row.removeFromLeft (150));

            juce::Slider* slider = nullptr;
            switch (i)
            {
                case 0:
                    slider = &grainLengthSlider;
                    break;
                case 1:
                    slider = &grainSpacingSlider;
                    break;
                case 2:
                    slider = &maxGrainsSlider;
                    break;
                case 3:
                    slider = &positionSpreadSlider;
                    break;
                case 4:
                    slider = &modRateSlider;
                    break;
                case 5:
                    slider = &pitchModDepthSlider;
                    break;
                case 6:
                    slider = &ampModDepthSlider;
                    break;
                case 7:
                    slider = &grainRandomnessSlider;
                    break;
            }

            if (slider) slider->setBounds (row);

            bounds.removeFromTop (spacing);
        }

        bounds.removeFromTop (10);
        auto buttonBounds = bounds.removeFromTop (35);
        applyButton.setBounds (buttonBounds.removeFromLeft (buttonBounds.getWidth() / 2 - 5));
        cancelButton.setBounds (buttonBounds.removeFromRight (buttonBounds.getWidth() - 5));
    }

    std::function<void (const FreezeParameters&)> onApply;
    std::function<void()> onCancel;

private:
    EngineMessageBus* messageBus;
    FreezeParameters currentParams;

    juce::Slider grainLengthSlider;
    juce::Slider grainSpacingSlider;
    juce::Slider maxGrainsSlider;
    juce::Slider positionSpreadSlider;
    juce::Slider modRateSlider;
    juce::Slider pitchModDepthSlider;
    juce::Slider ampModDepthSlider;
    juce::Slider grainRandomnessSlider;

    juce::TextButton applyButton;
    juce::TextButton cancelButton;

    juce::OwnedArray<juce::Label> labels;

    juce::Rectangle<int> getDialogBounds()
    {
        auto bounds = getLocalBounds();
        return bounds.withSizeKeepingCentre (500, 500);
    }

    void setupSlider (juce::Slider& slider, const juce::String& name, double min, double max, double value)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 80, 20);
        slider.setRange (min, max);
        slider.setValue (value);
        slider.setComponentID (name);
        addAndMakeVisible (slider);

        auto* label = new juce::Label (name, name);
        label->setJustificationType (juce::Justification::centredLeft);
        label->setColour (juce::Label::textColourId, LooperTheme::Colors::textDim);
        label->setFont (LooperTheme::Fonts::getBoldFont (10.0f));
        addAndMakeVisible (label);
        labels.add (label);
    }

    void addSliderRow (juce::FlexBox& mainFlex, juce::Slider& slider)
    {
        juce::FlexBox row;
        row.flexDirection = juce::FlexBox::Direction::row;

        // Find the matching label by comparing component IDs
        juce::Label* matchingLabel = nullptr;
        for (auto* label : labels)
        {
            if (label->getText() == slider.getComponentID())
            {
                matchingLabel = label;
                break;
            }
        }

        if (matchingLabel)
        {
            row.items.add (juce::FlexItem (*matchingLabel).withWidth (150));
            row.items.add (juce::FlexItem (slider).withFlex (1));
            mainFlex.items.add (juce::FlexItem (row).withHeight (30));
        }
    }

    void closePopup (bool shouldApply)
    {
        if (shouldApply && onApply)
        {
            FreezeParameters params;
            params.grainLengthMs = (float) grainLengthSlider.getValue();
            params.grainSpacing = (int) grainSpacingSlider.getValue();
            params.maxGrains = (int) maxGrainsSlider.getValue();
            params.positionSpread = (float) positionSpreadSlider.getValue();
            params.modRate = (float) modRateSlider.getValue();
            params.pitchModDepth = (float) pitchModDepthSlider.getValue();
            params.ampModDepth = (float) ampModDepthSlider.getValue();
            params.grainRandomness = (float) grainRandomnessSlider.getValue();

            onApply (params);
        }
        else if (onCancel)
        {
            onCancel();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeParametersPopup)
};
