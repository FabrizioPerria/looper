#pragma once
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class StudioMixerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StudioMixerLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, LooperTheme::Colors::backgroundDark);
    }

    // Vertical fader style
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
        if (style == juce::Slider::LinearVertical)
        {
            auto bounds = juce::Rectangle<float> (x, y, width, height);

            // Fader track - much narrower
            auto trackWidth = juce::jmin (6.0f, width * 0.3f);
            auto trackBounds = bounds.withSizeKeepingCentre (trackWidth, height);

            g.setColour (LooperTheme::Colors::backgroundDark);
            g.fillRoundedRectangle (trackBounds, trackWidth / 2.0f);

            // Filled portion (from bottom to slider pos)
            auto filledHeight = bounds.getBottom() - sliderPos;
            auto filledBounds = trackBounds.removeFromBottom (filledHeight);

            juce::ColourGradient gradient (LooperTheme::Colors::cyan,
                                           filledBounds.getCentreX(),
                                           filledBounds.getBottom(),
                                           LooperTheme::Colors::primary,
                                           filledBounds.getCentreX(),
                                           filledBounds.getY(),
                                           false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (filledBounds, trackWidth / 2.0f);

            // Fader thumb - reasonable size
            auto thumbWidth = juce::jmin (width - 4.0f, 28.0f);
            auto thumbHeight = 16.0f;
            auto thumbBounds = juce::Rectangle<float> (thumbWidth, thumbHeight)
                                   .withCentre (juce::Point<float> (bounds.getCentreX(), sliderPos));

            // Thumb shadow
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (thumbBounds.translated (0, 1), 2.0f);

            // Thumb body
            g.setColour (LooperTheme::Colors::surface);
            g.fillRoundedRectangle (thumbBounds, 2.0f);

            // Thumb border
            g.setColour (LooperTheme::Colors::primary.withAlpha (0.5f));
            g.drawRoundedRectangle (thumbBounds, 2.0f, 1.0f);

            // Thumb grip lines
            g.setColour (LooperTheme::Colors::textDim);
            auto gripY = thumbBounds.getCentreY();
            for (int i = -3; i <= 3; i += 3)
            {
                g.drawLine (thumbBounds.getX() + 6, gripY + i, thumbBounds.getRight() - 6, gripY + i, 1.0f);
            }
        }
        else
        {
            // Fallback to default for other styles
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1);

        juce::Colour bgColour = LooperTheme::Colors::backgroundDark;
        juce::Colour borderColour = LooperTheme::Colors::border;

        if (button.getToggleState())
        {
            bgColour = LooperTheme::Colors::primary.withAlpha (0.2f);
            borderColour = LooperTheme::Colors::cyan;
        }
        else if (shouldDrawButtonAsDown)
        {
            bgColour = LooperTheme::Colors::backgroundDark.darker (0.3f);
            borderColour = LooperTheme::Colors::primary;
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            bgColour = LooperTheme::Colors::surface;
            borderColour = LooperTheme::Colors::borderLight;
        }

        // Background
        g.setColour (bgColour);
        g.fillRoundedRectangle (bounds, 3.0f);

        // Border
        g.setColour (borderColour);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        // Subtle inner glow when toggled
        if (button.getToggleState())
        {
            auto innerBounds = bounds.reduced (2);
            g.setColour (LooperTheme::Colors::cyan.withAlpha (0.1f));
            g.fillRoundedRectangle (innerBounds, 2.0f);
        }
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
    {
        juce::Colour textColour = LooperTheme::Colors::text;

        if (button.getToggleState())
            textColour = LooperTheme::Colors::cyan;
        else if (! button.isEnabled())
            textColour = LooperTheme::Colors::textDisabled;
        else if (shouldDrawButtonAsHighlighted)
            textColour = LooperTheme::Colors::text.brighter (0.2f);

        g.setColour (textColour);
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return LooperTheme::Fonts::getBoldFont (10.0f);
    }
};
