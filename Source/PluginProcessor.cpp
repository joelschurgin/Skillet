/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
Bounce::Bounce(double defaultDelay, double minDelay, double currSampleRate)
    : defaultDelayMS{ defaultDelay }
    , minDelayMS{ minDelay }
    , delayMS{ defaultDelayMS }
    , sampleRate{ currSampleRate } 
    , bounceVolume{ 0.0 }
    , blockSize{0} {}

void Bounce::prepare(dsp::ProcessSpec spec) {
    if (sampleRate != spec.sampleRate) {
        sampleRate = spec.sampleRate;

        wet.setMaximumDelayInSamples(getMaxDelaySamples());
        wet.setDelay(calcNumSamples(delayMS));
    }

    if (blockSize != spec.maximumBlockSize) {
        blockSize = spec.maximumBlockSize;
        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    }

    wet.prepare(spec);
}

template<typename ProcessContext>
void Bounce::pushSamples(const ProcessContext& context) {
    const auto& inputBlock = context.getInputBlock();
    const auto numChannels = inputBlock.getNumChannels();
    const auto numSamples = inputBlock.getNumSamples();

    for (int channel = 0; channel < numChannels; ++channel) {
        auto* inChannelPtr = inputBlock.getChannelPointer(channel);
        auto* wetBufferWritePtr = wetBuffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample) {
            wet.pushSample(channel, inChannelPtr[sample]);
            wetBufferWritePtr[sample] = wet.popSample((int)channel);
        }
    }
}

template<typename ProcessContext>
void Bounce::popSamples(const ProcessContext& context) {
    const auto& inputBlock = context.getInputBlock();
    auto& outputBlock = context.getOutputBlock();
    const auto numChannels = outputBlock.getNumChannels();
    const auto numSamples = outputBlock.getNumSamples();

    jassert(inputBlock.getNumChannels() == numChannels);
    jassert(inputBlock.getNumSamples() == numSamples);

    if (context.isBypassed) {
        outputBlock.copyFrom(inputBlock);
        return;
    }

    for (int channel = 0; channel < numChannels; ++channel) {
        auto* inChannelPtr = inputBlock.getChannelPointer(channel);
        auto* wetBufferReadPtr = wetBuffer.getReadPointer(channel);
        auto* outChannelPtr = outputBlock.getChannelPointer(channel);
        for (int sample = 0; sample < numSamples; ++sample) {
            outChannelPtr[sample] = inChannelPtr[sample] + bounceVolume * wetBufferReadPtr[sample];
        }
    }
}

FilteredBounce::FilteredBounce(double defaultDelay, double minDelay, double currSampleRate, double highpassFreq, double lowpassFreq, double res)
    : Bounce(defaultDelay, minDelay, currSampleRate) 
    , lpfFreq{ lowpassFreq }
    , hpfFreq{ highpassFreq } 
    , Q{ res }
    , sampleRate{ currSampleRate }
    , lpf{ dsp::IIR::Coefficients<float>::makeLowPass(currSampleRate, lpfFreq, Q) }
    , hpf{ dsp::IIR::Coefficients<float>::makeHighPass(currSampleRate, hpfFreq, Q) } {

}


void FilteredBounce::prepare(dsp::ProcessSpec spec) {
    Bounce::prepare(spec);

    if (sampleRate != spec.sampleRate) {
        sampleRate = spec.sampleRate;

        lpf.state = dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpfFreq, Q);
        hpf.state = dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, hpfFreq, Q);
    }

    lpf.prepare(spec);
    hpf.prepare(spec);
}

void FilteredBounce::filter() {
    dsp::AudioBlock<float> wetBlock {wetBuffer};
    dsp::ProcessContextReplacing<float> context{ wetBlock };
    lpf.process(context);
    hpf.process(context);
    //wetBlock.multiplyBy(2.0);
}

//==============================================================================
SkilletAudioProcessor::SkilletAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", AudioChannelSet::stereo(), true)
#endif
    )
#endif 
    , currSampleRate{ 44100 } 
    , blockSize{0} 
    , floorBounce(10.3, 0.1, currSampleRate) 
    , chestBounce(2.0, 0.0, currSampleRate, 760, 2600, 0.707) {

    addParameter(height = new AudioParameterFloat("height", "Height", NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    pFilter.reserve(3);
    pFilter.push_back({ 8000, 1.94, 9.76 });
    pFilter.push_back({ 10000, 15.3, 4.83 });
    pFilter.push_back({ 3450, 0.71, 2.6 });

    calcFilterCoefficients();
}

SkilletAudioProcessor::~SkilletAudioProcessor()
{
}

//==============================================================================
const String SkilletAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SkilletAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SkilletAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SkilletAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SkilletAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SkilletAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SkilletAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SkilletAudioProcessor::setCurrentProgram (int index)
{
}

const String SkilletAudioProcessor::getProgramName (int index)
{
    return {};
}

void SkilletAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void SkilletAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
    if (currSampleRate != sampleRate) {
        currSampleRate = sampleRate;
        calcFilterCoefficients();
    }

    dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    if (samplesPerBlock != blockSize) {
        blockSize = samplesPerBlock;
    }

    for (int i = 0; i < pFilter.size(); ++i) {
        pFilter[i].filter.prepare(spec);
    }

    floorBounce.prepare(spec);
    chestBounce.prepare(spec);
}

void SkilletAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SkilletAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SkilletAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages) {
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    dsp::AudioBlock<float> block{ buffer };
    dsp::ProcessContextReplacing<float> context{ block };

    for (int i = 0; i < pFilter.size(); ++i) {
        pFilter[i].filter.process(context);
    }

    floorBounce.pushSamples(context);
    chestBounce.pushSamples(context);

    chestBounce.filter();

    floorBounce.popSamples(context);
    chestBounce.popSamples(context);
}

//==============================================================================
bool SkilletAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* SkilletAudioProcessor::createEditor()
{
    return new SkilletAudioProcessorEditor (*this);
}

//==============================================================================
void SkilletAudioProcessor::getStateInformation (MemoryBlock& destData) {
    juce::MemoryOutputStream(destData, true).writeFloat(*height);
}

void SkilletAudioProcessor::setStateInformation (const void* data, int sizeInBytes) {
    *height = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readFloat();
    setHeight();
}

void SkilletAudioProcessor::setHeight() {
    calcFilterCoefficients();

    const float heightVal = height->get();
    floorBounce.setHeight(heightVal);
    chestBounce.setHeight(heightVal, true);
}

void SkilletAudioProcessor::calcFilterCoefficients() {
    const float heightVal = height->get();
    *pFilter[0].filter.state = *dsp::IIR::Coefficients<float>::makePeakFilter(currSampleRate, pFilter[0].freq, pFilter[0].Q, Decibels::decibelsToGain(heightVal * pFilter[0].gainDB));
    *pFilter[1].filter.state = *dsp::IIR::Coefficients<float>::makePeakFilter(currSampleRate, pFilter[1].freq, pFilter[1].Q, Decibels::decibelsToGain(heightVal * pFilter[1].gainDB));
    *pFilter[2].filter.state = *dsp::IIR::Coefficients<float>::makeHighShelf(currSampleRate, pFilter[2].freq, pFilter[2].Q, Decibels::decibelsToGain(heightVal * pFilter[2].gainDB));
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkilletAudioProcessor();
}

