#pragma once

#include "engine/PerformanceMonitor.h"
#include "ui/colors/TokyoNight.h"
#include <JuceHeader.h>

class PerformanceMonitorPopup : public juce::DocumentWindow
{
public:
    PerformanceMonitorPopup (PerformanceMonitor* monitor)
        : DocumentWindow ("Performance Monitor", LooperTheme::Colors::background, DocumentWindow::allButtons), monitor (monitor)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);

        contentComponent = std::make_unique<ContentComponent> (monitor);
        setContentOwned (contentComponent.get(), true);

        centreWithSize (400, 300);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
        if (onClose) onClose();
    }

    std::function<void()> onClose;

private:
    class ContentComponent : public juce::Component, public juce::Timer
    {
    public:
        ContentComponent (PerformanceMonitor* mon) : monitor (mon)
        {
            resetButton.setButtonText ("Reset Peaks");
            resetButton.onClick = [this]() { monitor->resetPeaks(); };
            addAndMakeVisible (resetButton);

            startTimerHz (10); // Update 10 times per second
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (LooperTheme::Colors::background);

            if (! monitor) return;

            float cpuLoad = monitor->getCpuLoad() * 100.0f;
            float peakCpu = monitor->getPeakCpuLoad() * 100.0f;
            float avgBlockTime = monitor->getAverageBlockTimeMs();
            float peakBlockTime = monitor->getPeakBlockTimeMs();
            double expectedBlockTime = monitor->getExpectedBlockTimeMs();
            int xruns = monitor->getXrunCount();
            int totalBlocks = monitor->getTotalBlocksProcessed();

            int y = 20;
            int lineHeight = 25;
            int leftMargin = 20;

            g.setFont (14.0f);

            // CPU Load
            g.setColour (LooperTheme::Colors::text);
            g.drawText ("CPU Load:", leftMargin, y, 150, 20, juce::Justification::left);

            juce::Colour cpuColor = cpuLoad < 70.0f   ? LooperTheme::Colors::green
                                    : cpuLoad < 85.0f ? LooperTheme::Colors::yellow
                                                      : LooperTheme::Colors::red;
            g.setColour (cpuColor);
            g.drawText (juce::String (cpuLoad, 1) + "%", leftMargin + 150, y, 100, 20, juce::Justification::left);
            y += lineHeight;

            // Peak CPU Load
            g.setColour (LooperTheme::Colors::text);
            g.drawText ("Peak CPU:", leftMargin, y, 150, 20, juce::Justification::left);

            juce::Colour peakColor = peakCpu < 70.0f   ? LooperTheme::Colors::green
                                     : peakCpu < 85.0f ? LooperTheme::Colors::yellow
                                                       : LooperTheme::Colors::red;
            g.setColour (peakColor);
            g.drawText (juce::String (peakCpu, 1) + "%", leftMargin + 150, y, 100, 20, juce::Justification::left);
            y += lineHeight;

            y += 10; // Spacing

            // Block processing time
            g.setColour (LooperTheme::Colors::text);
            g.drawText ("Avg Block Time:", leftMargin, y, 150, 20, juce::Justification::left);
            g.drawText (juce::String (avgBlockTime, 2) + " ms", leftMargin + 150, y, 100, 20, juce::Justification::left);
            y += lineHeight;

            g.drawText ("Peak Block Time:", leftMargin, y, 150, 20, juce::Justification::left);

            juce::Colour blockColor = peakBlockTime < expectedBlockTime ? LooperTheme::Colors::green : LooperTheme::Colors::red;
            g.setColour (blockColor);
            g.drawText (juce::String (peakBlockTime, 2) + " ms", leftMargin + 150, y, 100, 20, juce::Justification::left);
            y += lineHeight;

            g.setColour (LooperTheme::Colors::textDim);
            g.drawText ("Expected:", leftMargin, y, 150, 20, juce::Justification::left);
            g.drawText (juce::String (expectedBlockTime, 2) + " ms", leftMargin + 150, y, 100, 20, juce::Justification::left);
            y += lineHeight;

            y += 10; // Spacing

            // Buffer overruns
            g.setColour (LooperTheme::Colors::text);
            g.drawText ("Buffer Overruns:", leftMargin, y, 150, 20, juce::Justification::left);

            juce::Colour xrunColor = xruns == 0 ? LooperTheme::Colors::green : LooperTheme::Colors::red;
            g.setColour (xrunColor);
            g.drawText (juce::String (xruns), leftMargin + 150, y, 100, 20, juce::Justification::left);
            y += lineHeight;

            // Statistics
            g.setColour (LooperTheme::Colors::textDim);
            g.drawText ("Total Blocks:", leftMargin, y, 150, 20, juce::Justification::left);
            g.drawText (juce::String (totalBlocks), leftMargin + 150, y, 100, 20, juce::Justification::left);
            y += lineHeight;

            double xrunRate = totalBlocks > 0 ? (double) xruns / totalBlocks * 100.0 : 0.0;
            g.drawText ("Overrun Rate:", leftMargin, y, 150, 20, juce::Justification::left);
            g.drawText (juce::String (xrunRate, 3) + "%", leftMargin + 150, y, 100, 20, juce::Justification::left);
        }

        void resized() override { resetButton.setBounds (getWidth() - 120, getHeight() - 40, 100, 30); }

        void timerCallback() override { repaint(); }

    private:
        PerformanceMonitor* monitor;
        juce::TextButton resetButton;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContentComponent)
    };

    PerformanceMonitor* monitor;
    std::unique_ptr<ContentComponent> contentComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformanceMonitorPopup)
};
