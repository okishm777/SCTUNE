#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout SCTUNEProcessor::layout()
{
    using F = juce::AudioParameterFloat;
    using I = juce::AudioParameterInt;
    using B = juce::AudioParameterBool;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto addFloat = [&p] (const char* id, const char* name, float min, float max, float initial)
    {
        juce::NormalisableRange<float> range (min, max, 1.0f);
        p.push_back (std::make_unique<F> (
            juce::ParameterID (id, 1), name, range, initial,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float value, int)
                {
                    return juce::String (juce::roundToInt (value));
                })
                .withValueFromStringFunction ([] (const juce::String& text)
                {
                    return text.getFloatValue();
                })));
    };
    addFloat ("retune", "Retune Speed", 0, 100, 90);
    addFloat ("humanize", "Humanize", 0, 100, 0);
    addFloat ("formant", "Formant Color", -100, 100, 0);
    addFloat ("mix", "Mix", 0, 100, 100);
    p.push_back (std::make_unique<I> ("key", "Key", 0, 11, 0));
    p.push_back (std::make_unique<I> ("scale", "Scale", 0, 2, 1));
    p.push_back (std::make_unique<B> ("bypass", "Bypass", false));
    return { p.begin(), p.end() };
}

SCTUNEProcessor::SCTUNEProcessor()
    : AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "SCTUNE", layout())
{
}

bool SCTUNEProcessor::isBusesLayoutSupported (const BusesLayout& b) const
{
    const auto s = b.getMainOutputChannelSet();
    return s == b.getMainInputChannelSet()
        && (s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo());
}

void SCTUNEProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    ring.setSize (2, ringSize);
    ring.clear();
    analysis.fill (0.0f);
    analysisWrite = analysisCounter = ringWrite = missingFrames = 0;
    phase = 0.0f;
    ratio = desiredRatio = 1.0f;
    stablePitch = 0.0f;
    wetBlend = 0.0f;
    detectedPitch.store (0.0f);
    inputMeter.store (0.0f);
    outputMeter.store (0.0f);
    setLatencySamples (static_cast<int> (dryDelay));
}

// Difference-function pitch estimator with cumulative mean normalization (YIN).
// Runs every analysisHop samples; uses a bounded 2048-sample window.
float SCTUNEProcessor::detectPitch() const
{
    constexpr int window = 2048;
    constexpr int stride = 2;
    const int minLag = juce::jmax (2, static_cast<int> (sr / 850.0));
    const int maxLag = juce::jmin (window / 2, static_cast<int> (sr / 80.0));
    if (minLag >= maxLag)
        return 0.0f;

    double energy = 0.0;
    for (int j = 0; j < window; j += stride)
    {
        const float s = analysis[(analysisWrite - window + j + analysisSize) & (analysisSize - 1)];
        energy += static_cast<double> (s) * s;
    }
    if (energy < 0.000002)
        return 0.0f;

    // Fixed-size stack array: no allocations on the audio thread.
    std::array<float, 1025> differences {};
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        double difference = 0.0;
        for (int j = 0; j < window - maxLag; j += stride)
        {
            const float a = analysis[(analysisWrite - window + j + analysisSize) & (analysisSize - 1)];
            const float b = analysis[(analysisWrite - window + j + lag + analysisSize) & (analysisSize - 1)];
            const double d = static_cast<double> (a) - b;
            difference += d * d;
        }
        differences[static_cast<size_t> (lag)] = static_cast<float> (difference);
    }

    float cumulative = 0.0f;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        cumulative += differences[static_cast<size_t> (lag)];
        differences[static_cast<size_t> (lag)] =
            differences[static_cast<size_t> (lag)] * lag
            / juce::jmax (cumulative, 1.0e-12f);
    }

    int candidate = 0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        if (differences[static_cast<size_t> (lag)] < 0.16f)
        {
            candidate = lag;
            while (candidate + 1 <= maxLag
                   && differences[static_cast<size_t> (candidate + 1)]
                      < differences[static_cast<size_t> (candidate)])
                ++candidate;
            break;
        }
    }
    if (candidate == 0)
        return 0.0f;

    float refinedLag = static_cast<float> (candidate);
    if (candidate > 1 && candidate < maxLag)
    {
        const float left = differences[static_cast<size_t> (candidate - 1)];
        const float mid = differences[static_cast<size_t> (candidate)];
        const float right = differences[static_cast<size_t> (candidate + 1)];
        const float denom = left - 2.0f * mid + right;
        if (std::abs (denom) > 1.0e-6f)
            refinedLag += juce::jlimit (-0.5f, 0.5f, 0.5f * (left - right) / denom);
    }
    return static_cast<float> (sr / refinedLag);
}

float SCTUNEProcessor::targetFrequency (float hz, int key, int scale) const
{
    if (hz <= 0.0f)
        return 0.0f;
    const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    float nearest = std::round (note);
    if (scale != 2)
    {
        static constexpr int major[] { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int minor[] { 0, 2, 3, 5, 7, 8, 10 };
        const int* degrees = scale == 0 ? major : minor;
        float error = 100.0f;
        for (int octave = -1; octave <= 11; ++octave)
            for (int degree = 0; degree < 7; ++degree)
            {
                const float candidate = static_cast<float> (12 * octave + key + degrees[degree]);
                const float distance = std::abs (candidate - note);
                if (distance < error)
                {
                    error = distance;
                    nearest = candidate;
                }
            }
    }
    return 440.0f * std::pow (2.0f, (nearest - 69.0f) / 12.0f);
}

float SCTUNEProcessor::readRing (int channel, float delay) const
{
    float position = static_cast<float> (ringWrite) - delay;
    if (position < 0.0f)
        position += static_cast<float> (ringSize);
    const int a = static_cast<int> (position) % ringSize;
    const int b = (a + 1) % ringSize;
    const float fraction = position - std::floor (position);
    const float* samples = ring.getReadPointer (channel);
    return samples[a] + (samples[b] - samples[a]) * fraction;
}

float SCTUNEProcessor::shiftedSample (int channel) const
{
    // Two overlapping Hann-windowed read heads. When ratio=1, both read heads
    // must produce the same fixed-latency signal to avoid modulation.
    const float p1 = phase;
    const float p2 = phase < 0.5f ? phase + 0.5f : phase - 0.5f;
    const float d1 = baseDelay + shiftWindow * p1;
    const float d2 = baseDelay + shiftWindow * p2;
    const float w1 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p1);
    const float w2 = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p2);
    return (readRing (channel, d1) * w1 + readRing (channel, d2) * w2)
           / (w1 + w2 + 1.0e-6f);
}

float SCTUNEProcessor::delayedDrySample (int channel) const
{
    return readRing (channel, dryDelay);
}

void SCTUNEProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;
    const int count = buffer.getNumSamples();
    const int channels = juce::jmin (2, buffer.getNumChannels());
    if (count == 0 || channels == 0)
        return;

    const float retune = state.getRawParameterValue ("retune")->load() / 100.0f;
    const float humanize = state.getRawParameterValue ("humanize")->load() / 100.0f;
    const float mix = state.getRawParameterValue ("mix")->load() / 100.0f;
    const int key = static_cast<int> (state.getRawParameterValue ("key")->load());
    const int scale = static_cast<int> (state.getRawParameterValue ("scale")->load());
    const bool bypass = state.getRawParameterValue ("bypass")->load() > 0.5f;
    // Formant Color remains in saved presets, but is deliberately neutral until
    // a real formant-preserving stage is implemented.

    // Calculate the one-pole smoothing coefficient once per audio block.
    const float responseMs = 180.0f - 174.0f * retune;
    const float smoothing = 1.0f - std::exp (-1000.0f
                            / (static_cast<float> (sr) * responseMs));
    const float blendSmoothing = 1.0f - std::exp (-1.0f
                            / (0.005f * static_cast<float> (sr)));

    float inPeak = 0.0f, outPeak = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float sample = buffer.getSample (ch, i);
            mono += sample;
            inPeak = juce::jmax (inPeak, std::abs (sample));
            ring.setSample (ch, ringWrite, sample);
        }
        mono /= static_cast<float> (channels);
        analysis[static_cast<size_t> (analysisWrite)] = mono;
        analysisWrite = (analysisWrite + 1) & (analysisSize - 1);

        if (++analysisCounter >= analysisHop)
        {
            analysisCounter = 0;
            const float hz = detectPitch();
            if (hz > 0.0f)
            {
                // Reject sudden octave errors when an established pitch exists.
                const bool octaveJump = stablePitch > 0.0f
                    && std::abs (12.0f * std::log2 (hz / stablePitch)) > 10.5f;
                if (! octaveJump || missingFrames >= 3)
                {
                    stablePitch = hz;
                    missingFrames = 0;
                    detectedPitch.store (hz);
                    const float target = targetFrequency (hz, key, scale);
                    const float fullRatio = target / hz;
                    const float strength = 1.0f - 0.65f * humanize;
                    desiredRatio = juce::jlimit (0.75f, 1.33f,
                                                std::pow (fullRatio, strength));
                }
                else
                {
                    ++missingFrames;
                }
            }
            else if (++missingFrames > 5)
            {
                stablePitch = 0.0f;
                detectedPitch.store (0.0f);
                desiredRatio = 1.0f;
            }
        }

        // Retune controls transition time only; Humanize controls correction depth.
        ratio += (desiredRatio - ratio) * smoothing;

        // At ratio=1 the moving heads have different read delays and produce
        // comb filtering. Crossfade to the fixed-latency read head near unity.
        // This also avoids unnecessary coloration on already-correct notes.
        const float deviation = std::abs (ratio - 1.0f);
        const float targetBlend = juce::jlimit (0.0f, 1.0f,
                                               (deviation - 0.002f) / 0.025f);
        wetBlend += (targetBlend - wetBlend) * blendSmoothing;

        for (int ch = 0; ch < channels; ++ch)
        {
            const float dry = delayedDrySample (ch);
            const float shifted = wetBlend > 0.00001f ? shiftedSample (ch) : dry;
            const float wet = dry + (shifted - dry) * wetBlend;
            const float output = bypass ? dry : (dry * (1.0f - mix) + wet * mix);
            buffer.setSample (ch, i, output);
            outPeak = juce::jmax (outPeak, std::abs (output));
        }
        phase += (1.0f - ratio) / shiftWindow;
        phase -= std::floor (phase);
        ringWrite = (ringWrite + 1) % ringSize;
    }
    inputMeter.store (inPeak);
    outputMeter.store (outPeak);
}

juce::AudioProcessorEditor* SCTUNEProcessor::createEditor()
{
    return new SCTUNEEditor (*this);
}

void SCTUNEProcessor::getStateInformation (juce::MemoryBlock& data)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, data);
}

void SCTUNEProcessor::setStateInformation (const void* data, int bytes)
{
    auto xml = getXmlFromBinary (data, bytes);
    if (xml && xml->hasTagName (state.state.getType()))
        state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SCTUNEProcessor();
}
