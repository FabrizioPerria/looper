#pragma once
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class DawLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DawLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, LooperTheme::Colors::backgroundDark);
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
            auto bounds = juce::Rectangle<float> (x, y, width, height);
            auto trackHeight = juce::jmin (6.0f, height * 0.5f);
            auto trackBounds = bounds.withSizeKeepingCentre (width, trackHeight);

            g.setColour (LooperTheme::Colors::backgroundDark);
            g.fillRoundedRectangle (trackBounds, trackHeight / 2.0f);

            auto filledWidth = sliderPos - bounds.getX();
            auto filledBounds = trackBounds.removeFromLeft (filledWidth);

            juce::ColourGradient gradient (LooperTheme::Colors::primary,
                                           filledBounds.getX(),
                                           filledBounds.getCentreY(),
                                           LooperTheme::Colors::cyan,
                                           filledBounds.getRight(),
                                           filledBounds.getCentreY(),
                                           false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (filledBounds, trackHeight / 2.0f);

            auto thumbWidth = 16.0f;
            auto thumbHeight = juce::jmin (height - 4.0f, 20.0f);
            auto thumbBounds = juce::Rectangle<float> (thumbWidth, thumbHeight)
                                   .withCentre (juce::Point<float> (sliderPos, bounds.getCentreY()));

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
                               const juce::Colour& backgroundColour,
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

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
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
        // if (shouldDrawButtonAsHighlighted)
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

        if (svg != nullptr)
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

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return LooperTheme::Fonts::getBoldFont (13.0f);
    }

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
};
