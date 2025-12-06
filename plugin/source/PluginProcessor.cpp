#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "profiler/PerfettoProfiler.h"
#include <JuceHeader.h>

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
#endif
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
{
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    // PerfettoProfiler::getInstance().writeTraceFile (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("trace.json"));
    while (processingBlockCount.load() > 0)
    {
        std::this_thread::yield();
    }

    looperEngine->releaseResources();
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const { return JucePlugin_Name; }

bool AudioPluginAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

void AudioPluginAudioProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    if (sampleRate != currentSampleRate || samplesPerBlock != currentBlockSize || getTotalNumInputChannels() != currentNumChannels)
    {
        if (currentSampleRate > 0)
        {
            while (processingBlockCount.load() > 0)
            {
                std::this_thread::yield();
            }
            looperEngine->releaseResources();
        }

        currentSampleRate = sampleRate;
        currentBlockSize = samplesPerBlock;
        currentNumChannels = getTotalNumInputChannels();
        looperEngine->prepareToPlay (sampleRate, samplesPerBlock, getTotalNumInputChannels());
    }
}

void AudioPluginAudioProcessor::releaseResources() {}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) return false;
#endif

    return true;
#endif
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    PERFETTO_FUNCTION();
    processingBlockCount++;
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // MIX INPUTS BEFORE PROCESSING
    if (totalNumInputChannels > 2)
    {
        int activePairs = 1;

        for (int ch = 2; ch < totalNumInputChannels; ch += 2)
        {
            int leftCh = ch;
            int rightCh = ch + 1;

            if (rightCh < totalNumInputChannels)
            {
                if (buffer.getMagnitude (leftCh, 0, buffer.getNumSamples()) > 0.0001f
                    || buffer.getMagnitude (rightCh, 0, buffer.getNumSamples()) > 0.0001f)
                {
                    activePairs++;
                }
            }
        }

        float scale = 1.0f / activePairs;

        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            buffer.applyGain (ch, 0, buffer.getNumSamples(), scale);
        }

        for (int ch = 2; ch < totalNumInputChannels; ch += 2)
        {
            int leftCh = ch;
            int rightCh = ch + 1;

            if (rightCh < totalNumInputChannels)
            {
                if (buffer.getMagnitude (leftCh, 0, buffer.getNumSamples()) > 0.0001f
                    || buffer.getMagnitude (rightCh, 0, buffer.getNumSamples()) > 0.0001f)
                {
                    juce::FloatVectorOperations::add (buffer.getWritePointer (0, 0),
                                                      buffer.getReadPointer (leftCh, 0),
                                                      buffer.getNumSamples());
                    juce::FloatVectorOperations::add (buffer.getWritePointer (1, 0),
                                                      buffer.getReadPointer (rightCh, 0),
                                                      buffer.getNumSamples());
                }
            }
        }
    }

    // Only process first 2 channels
    juce::AudioBuffer<float> stereoBuffer (buffer.getArrayOfWritePointers(), 2, buffer.getNumSamples());
    looperEngine->processBlock (stereoBuffer, midiMessages);

    midiMessages.clear();
    processingBlockCount--;
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() { return new AudioPluginAudioProcessorEditor (*this); }

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AudioPluginAudioProcessor(); }
