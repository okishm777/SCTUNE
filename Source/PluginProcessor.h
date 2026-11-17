#pragma once
#include <JuceHeader.h>
#include <array>
class SCTUNEProcessor final : public juce::AudioProcessor {
public:
 SCTUNEProcessor();
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override { return true; }
 const juce::String getName() const override { return "SCTUNE"; }
 bool acceptsMidi() const override { return false; }
 bool producesMidi() const override { return false; }
 bool isMidiEffect() const override { return false; }
 double getTailLengthSeconds() const override { return 0; }
 int getNumPrograms() override { return 1; }
 int getCurrentProgram() override { return 0; }
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override { return {}; }
 void changeProgramName(int,const juce::String&) override {}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 juce::AudioProcessorValueTreeState state;
 std::atomic<float> inputMeter{0}, outputMeter{0}, detectedPitch{0};
private:
 static juce::AudioProcessorValueTreeState::ParameterLayout layout();
 float detectPitch() const;
 float targetFrequency(float,int,int) const;
 float shiftedSample(int,float);
 static constexpr int analysisSize=4096;
 std::array<float,analysisSize> analysis{};
 int analysisWrite=0, analysisCounter=0;
 juce::AudioBuffer<float> ring;
 int ringWrite=0;
 float phase=0,ratio=1,desiredRatio=1;
 double sr=44100;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SCTUNEProcessor)
};
