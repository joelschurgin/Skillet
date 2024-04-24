/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class SkilletAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Slider::Listener
{
public:
    SkilletAudioProcessorEditor (SkilletAudioProcessor&);
    ~SkilletAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void sliderValueChanged(juce::Slider *s) override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SkilletAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkilletAudioProcessorEditor)

    juce::Slider slider;
    juce::SliderParameterAttachment sliderAttachment;
};
