#pragma once
#include "ui/colors/TokyoNight.h"
#include "ui/daw/PlaybackSlider.h"
#include <JuceHeader.h>

class DawLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DawLookAndFeel() { setColour (juce::ResizableWindow::backgroundColourId, LooperTheme::Colors::backgroundDark); }

    juce::Label* createSliderTextBox (juce::Slider& /**/) override
    {
        auto* label = new juce::Label();

        label->setJustificationType (juce::Justification::centred);
        label->setFont (LooperTheme::Fonts::getBoldFont (12.0f));
        label->setColour (juce::Label::textColourId, LooperTheme::Colors::cyan);
        label->setColour (juce::Label::backgroundColourId, LooperTheme::Colors::surface);
        label->setColour (juce::Label::outlineColourId, LooperTheme::Colors::cyan.withAlpha (0.5f));
        label->setBorderSize (juce::BorderSize<int> (1));

        return label;
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& /**/, juce::Point<int> screenPos, juce::Rectangle<int> /**/) override
    {
        const int tooltipWidth = 60;
        const int tooltipHeight = 24;

        return juce::Rectangle<int> (screenPos.x - tooltipWidth / 2, screenPos.y - tooltipHeight - 10, tooltipWidth, tooltipHeight);
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
        if (style == juce::Slider::LinearHorizontal)
        {
            slider.setPopupDisplayEnabled (true, true, nullptr);
            auto bounds = juce::Rectangle<int> (x, y, width, height);

            // Check if this is a PlaybackSpeedSlider
            bool isSpeedSlider = (dynamic_cast<PlaybackSpeedSlider*> (&slider) != nullptr);

            // Reserve space for tick marks if speed slider
            int bottomSpace = isSpeedSlider ? 18 : 0;
            auto sliderBounds = bounds.withHeight (bounds.getHeight() - bottomSpace);

            auto trackHeight = juce::jmin (6, sliderBounds.getHeight() / 2);
            auto trackBounds = sliderBounds.withSizeKeepingCentre (sliderBounds.getWidth(), trackHeight);

            // Save original for tick marks
            auto originalTrackBounds = trackBounds;

            // Draw track background
            g.setColour (LooperTheme::Colors::backgroundDark);
            auto trackBoundsF = trackBounds.toFloat();
            g.fillRoundedRectangle (trackBoundsF, (float) trackHeight / 2.0f);

            // Draw filled portion
            auto filledWidth = sliderPos - trackBoundsF.getX();
            auto filledBounds = trackBoundsF.removeFromLeft (filledWidth); // This modifies trackBounds!

            juce::ColourGradient gradient (LooperTheme::Colors::primary,
                                           filledBounds.getX(),
                                           filledBounds.getCentreY(),
                                           LooperTheme::Colors::cyan,
                                           filledBounds.getRight(),
                                           filledBounds.getCentreY(),
                                           false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (filledBounds, (float) trackHeight / 2.0f);

            // Draw tick marks using the ORIGINAL trackBounds
            if (isSpeedSlider)
            {
                drawSpeedTickMarks (g, originalTrackBounds.toFloat(), slider);
            }

            // Draw thumb
            auto thumbWidth = 16.0f;
            auto sliderBoundsF = sliderBounds.toFloat();
            auto thumbHeight = juce::jmin (sliderBoundsF.getHeight() - 4.0f, 20.0f);
            auto thumbBounds = juce::Rectangle<float> (thumbWidth, thumbHeight)
                                   .withCentre (juce::Point<float> (sliderPos, sliderBoundsF.getCentreY()));

            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (thumbBounds.translated (0, 1), 2.0f);

            g.setColour (LooperTheme::Colors::surface);
            g.fillRoundedRectangle (thumbBounds, 2.0f);

            g.setColour (LooperTheme::Colors::primary.withAlpha (0.5f));
            g.drawRoundedRectangle (thumbBounds, 2.0f, 1.0f);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& /**/,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto componentId = button.getComponentID();

        juce::Colour glowColour;
        bool isMute = (componentId == "mute");
        bool isSolo = (componentId == "solo");
        bool isClear = (componentId == "clear");

        if (isMute)
            glowColour = LooperTheme::Colors::red;
        else if (isSolo)
            glowColour = LooperTheme::Colors::yellow;
        else if (isClear)
            glowColour = LooperTheme::Colors::magenta;
        else
            glowColour = LooperTheme::Colors::cyan;

        // All icon buttons get circular backgrounds
        bool isIconButton = ! componentId.isEmpty();
        float cornerRadius = isIconButton ? bounds.getHeight() / 2.0f : 3.0f;

        if (button.getToggleState())
        {
            g.setColour (glowColour.withAlpha (0.15f));
            g.fillRoundedRectangle (bounds, cornerRadius);

            g.setColour (glowColour.withAlpha (0.3f));
            g.drawRoundedRectangle (bounds, cornerRadius, 1.5f);
        }
        else if (shouldDrawButtonAsDown)
        {
            g.setColour (glowColour.withAlpha (0.2f));
            g.fillRoundedRectangle (bounds, cornerRadius);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour (glowColour.withAlpha (0.1f));
            g.fillRoundedRectangle (bounds, cornerRadius);

            g.setColour (glowColour.withAlpha (0.4f));
            g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool /**/) override
    {
        auto componentId = button.getComponentID();

        // Determine color
        juce::Colour colour = LooperTheme::Colors::textDim;

        if (button.getToggleState())
        {
            if (componentId == "mute")
                colour = LooperTheme::Colors::red;
            else if (componentId == "solo")
                colour = LooperTheme::Colors::yellow;
            else if (componentId == "clear")
                colour = LooperTheme::Colors::magenta;
            else
                colour = LooperTheme::Colors::cyan;
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            if (componentId == "mute")
                colour = LooperTheme::Colors::red.brighter (0.4f);
            else if (componentId == "solo")
                colour = LooperTheme::Colors::yellow.brighter (0.4f);
            else if (componentId == "clear")
                colour = LooperTheme::Colors::magenta.brighter (0.4f);
            else
                colour = LooperTheme::Colors::cyan.brighter (0.4f);
        }

        // Try to load SVG
        auto svg = loadSvg (componentId);

        if (svg != nullptr && false) //svg are nice, but don't care now
        {
            // Draw SVG
            auto bounds = button.getLocalBounds().toFloat().reduced (12);
            svg->replaceColour (juce::Colours::black, colour);
            svg->drawWithin (g, bounds, juce::RectanglePlacement::centred, 1.0f);
        }
        else
        {
            // Fallback to text if no SVG
            g.setColour (colour);
            g.setFont (LooperTheme::Fonts::getBoldFont (13.0f));
            g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
        }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return LooperTheme::Fonts::getBoldFont (13.0f); }

private:
    std::map<juce::String, std::unique_ptr<juce::Drawable>> svgCache;

    juce::Drawable* loadSvg (const juce::String& componentId)
    {
        // Check cache first
        auto it = svgCache.find (componentId);
        if (it != svgCache.end()) return it->second.get();

        // Load from binary data (only happens once per icon)
        const void* data = nullptr;
        size_t size = 0;

        if (componentId == "undo")
        {
            data = BinaryData::undo_svg;
            size = BinaryData::undo_svgSize;
        }
        else if (componentId == "redo")
        {
            data = BinaryData::redo_svg;
            size = BinaryData::redo_svgSize;
        }
        else if (componentId == "clear")
        {
            data = BinaryData::clear_svg;
            size = BinaryData::clear_svgSize;
        }
        else if (componentId == "mute")
        {
            data = BinaryData::mute_svg;
            size = BinaryData::mute_svgSize;
        }
        else if (componentId == "solo")
        {
            data = BinaryData::solo_svg;
            size = BinaryData::solo_svgSize;
        }

        if (data != nullptr)
        {
            juce::MemoryInputStream stream (data, size, false);
            auto svg = juce::Drawable::createFromImageDataStream (stream);
            auto* ptr = svg.get();
            svgCache[componentId] = std::move (svg);
            return ptr;
        }

        return nullptr;
    }

    void drawSpeedTickMarks (juce::Graphics& g, juce::Rectangle<float> trackBounds, juce::Slider& slider)
    {
        auto drawTickMark = [&] (double value, const juce::String& label, bool isMajor)
        {
            // Calculate position based on slider's value range
            double proportion = (value - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum());

            // Apply the same skew that the slider uses
            proportion = slider.proportionOfLengthToValue (proportion);
            proportion = slider.valueToProportionOfLength (value);

            float tickX = trackBounds.getX() + (float) proportion * trackBounds.getWidth();

            // Draw tick line
            g.setColour (isMajor ? LooperTheme::Colors::cyan.withAlpha (0.6f) : LooperTheme::Colors::textDim.withAlpha (0.3f));
            float tickHeight = isMajor ? 8.0f : 4.0f;
            float tickY = trackBounds.getBottom() + 2.0f;

            g.drawLine (tickX, tickY, tickX, tickY + tickHeight, isMajor ? 1.5f : 1.5f);

            // Draw label for major ticks
            if (isMajor && label.isNotEmpty())
            {
                g.setFont (LooperTheme::Fonts::getRegularFont (9.0f));
                g.setColour (LooperTheme::Colors::textDim);
                auto labelWidth = 30;
                auto labelBounds = juce::Rectangle<int> ((int) tickX - labelWidth / 2, (int) (tickY + tickHeight + 1), labelWidth, 12);
                g.drawText (label, labelBounds, juce::Justification::centred);
            }
        };

        // Draw major tick marks at snap points
        drawTickMark (0.2f, "0.2x", true);
        drawTickMark (0.5f, "0.5x", true);
        drawTickMark (1.0f, "1.0x", true);
        drawTickMark (2.0f, "2.0x", true);

        // Optional: Draw minor tick marks
        drawTickMark (0.75f, "", false);
        drawTickMark (1.5f, "", false);
    }
};
