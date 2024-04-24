/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================

struct PinnaBand {
    double freq;
    double Q;
    double gainDB;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> filter;
};

class Bounce {
public:
    Bounce(double defaultDelay, double minDelay, double currSampleRate);

    void prepare(juce::dsp::ProcessSpec spec);

    template <typename ProcessContext>
    void pushSamples(const ProcessContext& context);

    template <typename ProcessContext>
    void popSamples(const ProcessContext& context);

    void setHeight(double height, bool fromAbove = false) {
        const int direction = 1 - static_cast<int>(fromAbove) * 2; // flip height if direction is different
        delayMS = static_cast<double>(direction) * height * (defaultDelayMS - minDelayMS) + defaultDelayMS;
        wet.setDelay(calcNumSamples(delayMS));

        // -50 chosen so height = 1 is basically -inf dB, but we can still hear the bounce at head level (height = 0)
        bounceVolume = juce::Decibels::decibelsToGain(-50.0 * (height + 1.0) - 6.0);
    }

private:
    const double getMaxDelaySamples() {
        return calcNumSamples(defaultDelayMS * 2 - minDelayMS);
    }

    double calcNumSamples(double delay) {
        return delay * 0.001 * sampleRate;
    }

private:
    double defaultDelayMS; // what delay should be when height = 0
    double minDelayMS;
    double delayMS;
    double sampleRate;
    int blockSize;
    double bounceVolume;

    juce::dsp::DelayLine<float> wet;

public:
    juce::AudioBuffer<float> wetBuffer;
};

class FilteredBounce : public Bounce {
public:
    FilteredBounce(double defaultDelay, double minDelay, double currSampleRate, double highpassFreq, double lowpassFreq, double res);

    void prepare(juce::dsp::ProcessSpec spec);
    void filter();

private:
    double lpfFreq;
    double hpfFreq;
    double Q;
    double sampleRate;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> lpf, hpf;
};

class SkilletAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SkilletAudioProcessor();
    ~SkilletAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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

    juce::RangedAudioParameter* getHeightParam() { return dynamic_cast<juce::RangedAudioParameter*>(height); }
    void setHeight();

private:
    void calcFilterCoefficients();

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkilletAudioProcessor)

    juce::AudioParameterFloat* height;
    double currSampleRate;
    int blockSize;

    std::vector<PinnaBand> pFilter;

    Bounce floorBounce;
    FilteredBounce chestBounce;
};