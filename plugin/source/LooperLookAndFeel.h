#pragma once
#include "themes/tokyonight.h"
#include <JuceHeader.h>

class LooperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LooperLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, LooperTheme::Colors::background);
        setColour (juce::TextButton::buttonColourId, LooperTheme::Colors::surface);
        setColour (juce::TextButton::textColourOffId, LooperTheme::Colors::text);
        setColour (juce::TextButton::textColourOnId, LooperTheme::Colors::cyan);

        setColour (juce::Slider::backgroundColourId, LooperTheme::Colors::backgroundDark);
        setColour (juce::Slider::thumbColourId, LooperTheme::Colors::cyan);
        setColour (juce::Slider::trackColourId, LooperTheme::Colors::primary);

        setColour (juce::Label::textColourId, LooperTheme::Colors::text);
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1);
        auto cornerSize = LooperTheme::Dimensions::smallCornerRadius;

        // Determine color based on state
        juce::Colour bgColour = LooperTheme::Colors::surface;

        if (shouldDrawButtonAsDown)
            bgColour = LooperTheme::Colors::backgroundDark;
        else if (shouldDrawButtonAsHighlighted)
            bgColour = LooperTheme::Colors::surfaceHighlight;

        // Special handling for toggle buttons
        if (button.getToggleState())
        {
            bgColour = LooperTheme::Colors::primary.withAlpha (0.2f);
        }

        // Background
        g.setColour (bgColour);
        g.fillRoundedRectangle (bounds, cornerSize);

        // Border with glow on toggle
        if (button.getToggleState())
        {
            // Outer glow
            g.setColour (LooperTheme::Colors::cyan.withAlpha (0.3f));
            g.drawRoundedRectangle (bounds, cornerSize, 2.0f);

            // Inner border
            g.setColour (LooperTheme::Colors::cyan);
            g.drawRoundedRectangle (bounds.reduced (1.5f), cornerSize - 0.5f, 1.5f);
        }
        else
        {
            g.setColour (LooperTheme::Colors::border);
            g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
        }
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x,
                           int y,
                           int width,
                           int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto trackBounds = bounds.reduced (0, height * 0.35f);

        // Track background
        g.setColour (LooperTheme::Colors::backgroundDark);
        g.fillRoundedRectangle (trackBounds, trackBounds.getHeight() / 2.0f);

        // Filled track with gradient
        auto filledTrack = trackBounds.withWidth (sliderPos - trackBounds.getX());

        juce::ColourGradient gradient (LooperTheme::Colors::primary,
                                       filledTrack.getX(),
                                       filledTrack.getCentreY(),
                                       LooperTheme::Colors::cyan,
                                       filledTrack.getRight(),
                                       filledTrack.getCentreY(),
                                       false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (filledTrack, filledTrack.getHeight() / 2.0f);

        // Thumb
        auto thumbRadius = height * 0.4f;
        auto thumbBounds = juce::Rectangle<float> (thumbRadius * 2, thumbRadius * 2)
                               .withCentre (juce::Point<float> (sliderPos, bounds.getCentreY()));

        // Thumb glow
        g.setColour (LooperTheme::Colors::cyan.withAlpha (0.4f));
        g.fillEllipse (thumbBounds.expanded (4));

        // Thumb
        g.setColour (LooperTheme::Colors::cyan);
        g.fillEllipse (thumbBounds);

        // Thumb inner highlight
        g.setColour (LooperTheme::Colors::cyan.brighter (0.5f));
        g.fillEllipse (thumbBounds.reduced (thumbRadius * 0.4f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return LooperTheme::Fonts::getBoldFont (buttonHeight * 0.45f);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return LooperTheme::Fonts::getRegularFont (14.0f);
    }
};
