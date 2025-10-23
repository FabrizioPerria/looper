#pragma once

#include "engine/LooperEngine.h"
#include <JuceHeader.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
class AudioPluginAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    LooperEngine* getLooperEngine() { return looperEngine.get(); }

    AudioToUIBridge& getUIBridge() { return uiBridge; }

    double getCPULoad() const { return currentCPULoad.load(); }
    int getUnderrunCount() const { return underrunCount.load(); }
    void resetUnderrunCount() { underrunCount = 0; }

private:
    std::atomic<double> currentCPULoad { 0.0 };
    std::atomic<int> underrunCount { 0 };
    juce::int64 lastWarningTime = 0;
    std::unique_ptr<LooperEngine> looperEngine = std::make_unique<LooperEngine>();
    AudioToUIBridge uiBridge;
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
