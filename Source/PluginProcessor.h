#pragma once
#include <JuceHeader.h>
#include <array>

class SCTUNEProcessor final : public juce::AudioProcessor
{
public:
    SCTUNEProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SCTUNE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    std::atomic<float> inputMeter { 0.0f }, outputMeter { 0.0f }, detectedPitch { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    float detectPitch() const;
    float targetFrequency (float hz, int key, int scale) const;
    float shiftedSample (int channel) const;
    float delayedDrySample (int channel) const;
    float readRing (int channel, float delay) const;

    static constexpr int analysisSize = 4096;
    static constexpr int ringSize = 16384;
    static constexpr int analysisHop = 512;
    static constexpr float shiftWindow = 2048.0f;
    static constexpr float baseDelay = 256.0f;
    static constexpr float dryDelay = baseDelay + shiftWindow * 0.5f;

    std::array<float, analysisSize> analysis {};
    int analysisWrite = 0, analysisCounter = 0;
    juce::AudioBuffer<float> ring;
    int ringWrite = 0;
    float phase = 0.0f, ratio = 1.0f, desiredRatio = 1.0f;
    float stablePitch = 0.0f;
    int missingFrames = 0;
    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SCTUNEProcessor)
};

