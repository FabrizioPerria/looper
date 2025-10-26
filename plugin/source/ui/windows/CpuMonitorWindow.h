// CPUMonitorWindow.h
#pragma once
#include "PluginProcessor.h"
#include <JuceHeader.h>

class CPUMonitorWindow : public juce::DocumentWindow
{
public:
    CPUMonitorWindow (juce::AudioProcessor& proc)
        : DocumentWindow ("CPU Monitor",
                          juce::Desktop::getInstance().getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::allButtons)
        , processor (proc)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new CPUMonitorComponent (processor), true);

        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        setResizable (true, false);
    }

    void closeButtonPressed() override { setVisible (false); }

private:
    juce::AudioProcessor& processor;

    // Inner component that displays the actual CPU info
    class CPUMonitorComponent : public juce::Component, private juce::Timer
    {
    public:
        CPUMonitorComponent (juce::AudioProcessor& proc) : processor (proc)
        {
            cpuLabel.setText ("CPU Usage:", juce::dontSendNotification);
            juce::FontOptions options = juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold);
            cpuLabel.setFont (juce::Font (options));
            addAndMakeVisible (cpuLabel);

            cpuValueLabel.setFont (juce::Font (options));
            cpuValueLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (cpuValueLabel);

            underrunLabel.setText ("Buffer Overruns:", juce::dontSendNotification);
            addAndMakeVisible (underrunLabel);

            underrunValueLabel.setFont (juce::Font (options));
            underrunValueLabel.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (underrunValueLabel);

            resetButton.setButtonText ("Reset Counter");
            resetButton.onClick = [this]
            {
                if (auto* yourProc = dynamic_cast<AudioPluginAudioProcessor*> (&processor)) yourProc->resetUnderrunCount();
            };
            addAndMakeVisible (resetButton);

            setSize (300, 250);
            startTimerHz (30); // Update 30 times per second
        }

        void paint (juce::Graphics& g) override { g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId)); }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (20);

            cpuLabel.setBounds (bounds.removeFromTop (30));
            cpuValueLabel.setBounds (bounds.removeFromTop (50));

            bounds.removeFromTop (20); // Spacing

            underrunLabel.setBounds (bounds.removeFromTop (30));
            underrunValueLabel.setBounds (bounds.removeFromTop (40));

            bounds.removeFromTop (20); // Spacing
            resetButton.setBounds (bounds.removeFromTop (30).reduced (40, 0));
        }

        void timerCallback() override
        {
            if (auto* yourProc = dynamic_cast<AudioPluginAudioProcessor*> (&processor))
            {
                double cpuLoad = yourProc->getCPULoad();
                cpuValueLabel.setText (juce::String (cpuLoad, 1) + "%", juce::dontSendNotification);

                // Color code based on load
                if (cpuLoad > 90.0)
                    cpuValueLabel.setColour (juce::Label::textColourId, juce::Colours::red);
                else if (cpuLoad > 70.0)
                    cpuValueLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
                else
                    cpuValueLabel.setColour (juce::Label::textColourId, juce::Colours::green);

                int underruns = yourProc->getUnderrunCount();
                underrunValueLabel.setText (juce::String (underruns), juce::dontSendNotification);

                if (underruns > 0)
                    underrunValueLabel.setColour (juce::Label::textColourId, juce::Colours::red);
                else
                    underrunValueLabel.setColour (juce::Label::textColourId, juce::Colours::green);
            }
        }

    private:
        juce::AudioProcessor& processor;
        juce::Label cpuLabel;
        juce::Label cpuValueLabel;
        juce::Label underrunLabel;
        juce::Label underrunValueLabel;
        juce::TextButton resetButton;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CPUMonitorComponent)
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CPUMonitorWindow)
};
