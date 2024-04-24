/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
SkilletAudioProcessorEditor::SkilletAudioProcessorEditor (SkilletAudioProcessor& p)
    : AudioProcessorEditor {&p}
    , audioProcessor {p} 
    , sliderAttachment{ *audioProcessor.getHeightParam(), slider}  {
    setSize(200, 400);

    const double sliderAngle = 2.75;

    addAndMakeVisible(slider);
    slider.setRange(-1.0, 1.0);
    slider.setValue(0.0);
    slider.setSliderStyle(Slider::LinearBarVertical);
    slider.addListener(this);

    sliderAttachment.sendInitialUpdate();
}

SkilletAudioProcessorEditor::~SkilletAudioProcessorEditor() {
}

//==============================================================================
void SkilletAudioProcessorEditor::paint (Graphics& g) {
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));

    const int width = getWidth();
    const int height = getHeight();

    const int padding = 20;
    slider.setBounds(padding, padding, width - padding * 2, height - padding * 2);
}

void SkilletAudioProcessorEditor::resized() {

}

void SkilletAudioProcessorEditor::sliderValueChanged(Slider* s) {
    if (s == &slider) {
        audioProcessor.setHeight(); // not sure if I need this
    }
}
