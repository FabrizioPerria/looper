#pragma once
#include "PluginProcessor.h"
#include "ui/editor/DawLookAndFeel.h"
#include "ui/editor/LooperEditor.h"
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
    std::unique_ptr<LooperEditor> looperEditor;
    std::unique_ptr<LooperLookAndFeel> lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
