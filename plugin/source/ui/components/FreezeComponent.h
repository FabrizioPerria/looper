#pragma once

#include "audio/EngineCommandBus.h"
#include "engine/GranularFreeze.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class FreezeComponent : public juce::Component, public EngineMessageBus::Listener, public juce::MouseListener, public juce::Timer
{
public:
    FreezeComponent (EngineMessageBus* engineMessageBus, GranularFreeze* freezer) : uiToEngineBus (engineMessageBus), freezeSynth (freezer)
    {
        uiToEngineBus->addListener (this);
        startTimerHz (30); // 30 fps for animation
    }

    ~FreezeComponent() override
    {
        stopTimer();
        uiToEngineBus->removeListener (this);
    }

    void timerCallback() override
    {
        rotation += 0.05f;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        uiToEngineBus->pushCommand (EngineMessageBus::Command { EngineMessageBus::CommandType::ToggleFreeze, -1, {} });
    }

    void paint (Graphics& g) override
    {
        auto grains = freezeSynth->getActiveGrains();
        int activeCount = 0;
        for (auto& grain : grains)
        {
            if (grain.isPlaying()) activeCount++;
        }

        // auto center = getLocalBounds().getCentre().toFloat();
        float scale = 0.5f;

        // Draw rotating snowflake
        // paintSnowflake (g, center.x, center.y, scale, 0.0f);
        auto dataSvg = BinaryData::freeze_svg;
        auto sizeSvg = BinaryData::freeze_svgSize;

        juce::MemoryInputStream stream (dataSvg, static_cast<size_t> (sizeSvg), false);
        auto svg = juce::Drawable::createFromImageDataStream (stream);

        auto currentBounds = getLocalBounds().toFloat();
        auto size = juce::jmin (currentBounds.getWidth(), currentBounds.getHeight()) / 2.0f;
        auto bounds = currentBounds.withSizeKeepingCentre (size, size).reduced (5.0f);
        auto center = currentBounds.getCentre(); // Use the main bounds center for everything

        svg->replaceColour (juce::Colours::black, LooperTheme::Colors::cyan.withAlpha (freezeSynth->isEnabled() ? 1.0f : 0.3f));
        svg->drawWithin (g, bounds, juce::RectanglePlacement::centred, 1.0f);

        // if (freezeSynth->isEnabled())
        // {
        //     float radius = 10.0f + (static_cast<float> (activeCount) * 0.5f);
        //
        //     for (int i = 0; i < activeCount; i++)
        //     {
        //         auto color = LooperTheme::Colors::cyan.withAlpha (static_cast<float> (i) / static_cast<float> (activeCount));
        //         g.setColour (color);
        //         float angle = (static_cast<float> (i) / static_cast<float> (activeCount)) + rotation;
        //         float x = std::cos (angle * juce::MathConstants<float>::twoPi) * radius;
        //         float y = std::sin (angle * juce::MathConstants<float>::twoPi) * radius;
        //         g.drawEllipse (center.x + x, center.y + y, 2, 2, 1);
        //     }
        // }
        // else
        // {
        //     float radius = 14.0f;
        //
        //     g.setColour (LooperTheme::Colors::cyan.withAlpha (0.3f));
        //     float angle = rotation;
        //     float x = std::cos (angle * juce::MathConstants<float>::twoPi) * radius;
        //     float y = std::sin (angle * juce::MathConstants<float>::twoPi) * radius;
        //     g.drawEllipse (center.x + x, center.y + y, 2, 2, 1);
        // }
    }

private:
    EngineMessageBus* uiToEngineBus;
    GranularFreeze* freezeSynth;
    float rotation = 0.0f; // member variable!
    void paintSnowflake (Graphics& g, float centerX, float centerY, float scale, float rotation)
    {
        g.setColour (juce::Colours::white);

        auto drawArm = [&] (float angle)
        {
            float rad = angle * juce::MathConstants<float>::twoPi;
            float cos_a = std::cos (rad);
            float sin_a = std::sin (rad);

            // Main branch
            float tipX = centerX + cos_a * 15.0f * scale;
            float tipY = centerY + sin_a * 15.0f * scale;
            g.drawLine (centerX, centerY, tipX, tipY, 1.5f);

            // Left sub-branch
            float subAngle1 = rad + (juce::MathConstants<float>::pi / 3.0f);
            float sub1X = centerX + cos_a * 10.0f * scale + std::cos (subAngle1) * 5.0f * scale;
            float sub1Y = centerY + sin_a * 10.0f * scale + std::sin (subAngle1) * 5.0f * scale;
            g.drawLine (centerX + cos_a * 10.0f * scale, centerY + sin_a * 10.0f * scale, sub1X, sub1Y, 1.0f);

            // Right sub-branch
            float subAngle2 = rad - (juce::MathConstants<float>::pi / 3.0f);
            float sub2X = centerX + cos_a * 10.0f * scale + std::cos (subAngle2) * 5.0f * scale;
            float sub2Y = centerY + sin_a * 10.0f * scale + std::sin (subAngle2) * 5.0f * scale;
            g.drawLine (centerX + cos_a * 10.0f * scale, centerY + sin_a * 10.0f * scale, sub2X, sub2Y, 1.0f);
        };

        // Draw 6 arms at 60-degree intervals, rotated by animation phase
        for (int i = 0; i < 6; i++)
        {
            drawArm ((i / 6.0f) + rotation);
        }

        // Center circle
        g.drawEllipse (centerX - 2.0f, centerY - 2.0f, 4.0f, 4.0f, 1.0f);
    }

    constexpr static EngineMessageBus::EventType subscribedEvents[] = { EngineMessageBus::EventType::FreezeStateChanged };

    void handleEngineEvent (const EngineMessageBus::Event& event) override
    {
        bool isSubscribed = std::find (std::begin (subscribedEvents), std::end (subscribedEvents), event.type)
                            != std::end (subscribedEvents);
        if (! isSubscribed) return;

        switch (event.type)
        {
            case EngineMessageBus::EventType::FreezeStateChanged:
            {
                bool isFrozen = std::get<bool> (event.data);
                // freezeButton.setToggleState (isFrozen, juce::dontSendNotification);
                break;
            }
            default:
                throw juce::String ("Unhandled event type in FreezeComponent" + juce::String (static_cast<int> (event.type)));
        }
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreezeComponent);
};
