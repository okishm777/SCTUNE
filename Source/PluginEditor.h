#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class SCTUNEEditor final: public juce::AudioProcessorEditor,private juce::Timer {
public:
 explicit SCTUNEEditor(SCTUNEProcessor&);
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 void timerCallback() override { repaint(); }
 SCTUNEProcessor& processor;
 juce::Slider retune,humanize,formant,mix;
 juce::ComboBox key,scale,preset;
 juce::ToggleButton bypass{"BYPASS"};
 using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
 using CA=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
 using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
 std::unique_ptr<SA> aRetune,aHumanize,aFormant,aMix;
 std::unique_ptr<CA> aKey,aScale;
 std::unique_ptr<BA> aBypass;
 void setupKnob(juce::Slider&,const juce::String&);
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SCTUNEEditor)
};
