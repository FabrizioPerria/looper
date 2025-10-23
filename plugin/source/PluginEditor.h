#pragma once
#include "PluginProcessor.h"
#include "ui/ThemeFactory.h"
#include "ui/windows/CpuMonitorWindow.h"
#include <JuceHeader.h>

//==============================================================================
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void openCPUMonitor()
    {
        if (cpuMonitorWindow == nullptr)
        {
            cpuMonitorWindow = std::make_unique<CPUMonitorWindow> (processorRef);
        }
        else
        {
            cpuMonitorWindow->toFront (true);
            cpuMonitorWindow->setVisible (true);
        }
    }

    juce::TextButton cpuMonitorButton;
    std::unique_ptr<CPUMonitorWindow> cpuMonitorWindow;
    AudioPluginAudioProcessor& processorRef;
    std::unique_ptr<Theme> theme;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
