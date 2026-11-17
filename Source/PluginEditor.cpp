#include "PluginEditor.h"
#include "SCTUNELogo.h"
#include <cmath>

namespace
{
const juce::Colour accent (0xffae36ff);
const juce::Colour bright (0xffd58aff);
const juce::Colour background (0xff0c0715);
const juce::Colour panel (0xff1b1029);
const juce::Colour muted (0xffb7a4ca);
const juce::Colour white (0xfff9f4ff);

void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (accent.withAlpha (0.40f));
    g.drawRoundedRectangle (bounds.reduced (0.8f), radius, 1.2f);
}

void drawLabel (juce::Graphics& g, const juce::String& text,
                juce::Rectangle<int> bounds, float fontSize,
                juce::Justification alignment = juce::Justification::centred,
                juce::Colour colour = muted, bool isBold = false)
{
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (fontSize,
                    isBold ? juce::Font::bold : juce::Font::plain)));
    g.drawFittedText (text, bounds, alignment, 1);
}

class SCTUNELookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float proportion, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                              (float) width, (float) height);
        auto centre = bounds.getCentre();
        float radius = juce::jmax (8.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 13.0f);
        float angle = startAngle + proportion * (endAngle - startAngle);

        juce::Path track, active;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             startAngle, endAngle, true);
        active.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                              startAngle, angle, true);
        auto stroke = juce::PathStrokeType (7.0f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded);
        g.setColour (juce::Colour (0xff483453));
        g.strokePath (track, stroke);
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (active, juce::PathStrokeType (13.0f,
                         juce::PathStrokeType::curved,
                         juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (active, stroke);
        g.setColour (juce::Colour (0xff241630));
        g.fillEllipse (centre.x - radius + 10.0f, centre.y - radius + 10.0f,
                       (radius - 10.0f) * 2.0f, (radius - 10.0f) * 2.0f);
        auto point = centre.getPointOnCircumference (radius, angle);
        g.setColour (white);
        g.fillEllipse (point.x - 5.5f, point.y - 5.5f, 11.0f, 11.0f);
    }
};

// Lives for the entire application lifetime; JUCE controls never retain a dangling look-and-feel pointer.
SCTUNELookAndFeel purpleLook;
}

SCTUNEEditor::SCTUNEEditor (SCTUNEProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (980, 580);

    setupKnob (humanize, "HUMANIZE");
    setupKnob (retune, "RETUNE SPEED");
    setupKnob (formant, "FORMANT COLOR");
    setupKnob (mix, "MIX");

    key.addItemList ({ "C", "C# / Db", "D", "D# / Eb", "E", "F",
                       "F# / Gb", "G", "G# / Ab", "A", "A# / Bb", "B" }, 1);
    scale.addItemList ({ "MAJOR", "MINOR", "CHROMATIC" }, 1);
    preset.addItemList ({ "HARD SC", "SOFT CLOUD", "ROBOT MODE" }, 1);
    preset.setSelectedId (1, juce::dontSendNotification);

    for (auto* combo : { &key, &scale, &preset })
    {
        combo->setColour (juce::ComboBox::backgroundColourId, background);
        combo->setColour (juce::ComboBox::textColourId, white);
        combo->setColour (juce::ComboBox::outlineColourId, accent);
        combo->setColour (juce::ComboBox::arrowColourId, white);
        addAndMakeVisible (*combo);
    }

    preset.onChange = [this]
    {
        auto setParameter = [this] (const char* id, float value)
        {
            if (auto* parameter = processor.state.getParameter (id))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        switch (preset.getSelectedId())
        {
            case 1:
                setParameter ("retune", 100.0f);
                setParameter ("humanize", 0.0f);
                setParameter ("mix", 100.0f);
                setParameter ("formant", 0.0f);
                break;
            case 2:
                setParameter ("retune", 45.0f);
                setParameter ("humanize", 70.0f);
                setParameter ("mix", 75.0f);
                setParameter ("formant", 0.0f);
                break;
            case 3:
                setParameter ("retune", 100.0f);
                setParameter ("humanize", 0.0f);
                setParameter ("mix", 100.0f);
                setParameter ("formant", -40.0f);
                break;
            default: break;
        }
    };

    bypass.setButtonText ("BYPASS");
    bypass.setColour (juce::ToggleButton::textColourId, white);
    bypass.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (bypass);

    aRetune = std::make_unique<SA> (processor.state, "retune", retune);
    aHumanize = std::make_unique<SA> (processor.state, "humanize", humanize);
    aFormant = std::make_unique<SA> (processor.state, "formant", formant);
    aMix = std::make_unique<SA> (processor.state, "mix", mix);
    aKey = std::make_unique<CA> (processor.state, "key", key);
    aScale = std::make_unique<CA> (processor.state, "scale", scale);
    aBypass = std::make_unique<BA> (processor.state, "bypass", bypass);

    startTimerHz (30);
}

void SCTUNEEditor::setupKnob (juce::Slider& slider, const juce::String& name)
{
    slider.setName (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 25);
    slider.setNumDecimalPlacesToDisplay (0);
    slider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value));
    };
    slider.setColour (juce::Slider::textBoxTextColourId, white);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, background);
    slider.setColour (juce::Slider::textBoxOutlineColourId, accent.withAlpha (0.5f));
    slider.setLookAndFeel (&purpleLook);
    addAndMakeVisible (slider);
}

void SCTUNEEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    drawPanel (g, { 10.0f, 10.0f, 960.0f, 78.0f }, 17.0f);
    drawPanel (g, { 10.0f, 98.0f, 960.0f, 274.0f }, 17.0f);
    drawPanel (g, { 10.0f, 382.0f, 960.0f, 188.0f }, 17.0f);

    // Reuse the existing purple logo artwork from SCTUNELogo.h.
    SCTUNELogo::draw (g, juce::Rectangle<float> (26.0f, 25.0f, 90.0f, 46.0f));
    drawLabel (g, "SCTUNE", { 134, 19, 275, 47 }, 38.0f,
               juce::Justification::centredLeft, white, true);
    drawLabel (g, "VOCAL TUNE & FORMANT", { 137, 61, 245, 16 }, 11.0f,
               juce::Justification::centredLeft, muted);
    drawLabel (g, "PRESET", { 515, 19, 100, 20 }, 12.0f,
               juce::Justification::centredLeft);

    auto display = juce::Rectangle<float> (28.0f, 113.0f, 924.0f, 244.0f);
    g.setColour (juce::Colour (0xff110a1d));
    g.fillRoundedRectangle (display, 13.0f);
    g.setColour (accent.withAlpha (0.20f));
    g.drawRoundedRectangle (display.reduced (0.5f), 13.0f, 1.0f);

    // Subtle display grid.
    g.setColour (accent.withAlpha (0.055f));
    for (int x = 55; x < 940; x += 35)
        g.drawVerticalLine (x, 130.0f, 309.0f);
    for (int y = 149; y < 307; y += 35)
        g.drawHorizontalLine (y, 48.0f, 932.0f);

    float level = juce::jlimit (0.0f, 1.0f, processor.inputMeter.load());
    float output = juce::jlimit (0.0f, 1.0f, processor.outputMeter.load());
    float pitch = processor.detectedPitch.load();

    for (int i = 0; i < 155; ++i)
    {
        float x = 50.0f + i * 5.68f;
        float envelope = std::abs (std::sin (i * 0.32f) * std::cos (i * 0.12f));
        float h = 10.0f + envelope * (22.0f + 60.0f * level);
        g.setColour (accent.withAlpha (0.22f + 0.55f * envelope));
        g.fillRoundedRectangle (x, 235.0f - h, 2.7f, 2.0f * h, 1.2f);
    }

    juce::Path waveform;
    for (int i = 0; i < 300; ++i)
    {
        float x = 50.0f + i * 2.93f;
        float y = 235.0f + 12.0f * std::sin (i * 0.12f)
                          + 4.0f * std::sin (i * 0.39f);
        if (i == 0) waveform.startNewSubPath (x, y);
        else waveform.lineTo (x, y);
    }
    g.setColour (accent.withAlpha (0.22f));
    g.strokePath (waveform, juce::PathStrokeType (7.0f));
    g.setColour (bright);
    g.strokePath (waveform, juce::PathStrokeType (1.8f));

    drawLabel (g, "INPUT", { 48, 324, 65, 19 }, 11.0f,
               juce::Justification::centredLeft);
    drawLabel (g, "OUTPUT", { 840, 324, 90, 19 }, 11.0f,
               juce::Justification::centredRight);
    drawLabel (g, pitch > 0.0f ? juce::String (pitch, 1) + " Hz" : "NO PITCH",
               { 419, 321, 142, 23 }, 12.0f, juce::Justification::centred, white, true);

    g.setColour (juce::Colour (0xff49305c));
    g.fillRoundedRectangle (116.0f, 332.0f, 142.0f, 5.0f, 2.5f);
    g.fillRoundedRectangle (694.0f, 332.0f, 142.0f, 5.0f, 2.5f);
    g.setColour (accent);
    if (level > 0.0f)
        g.fillRoundedRectangle (116.0f, 332.0f, 142.0f * level, 5.0f, 2.5f);
    if (output > 0.0f)
        g.fillRoundedRectangle (694.0f, 332.0f, 142.0f * output, 5.0f, 2.5f);

    g.setColour (accent.withAlpha (0.35f));
    g.drawVerticalLine (389, 408.0f, 552.0f);

    drawLabel (g, "KEY", { 42, 410, 110, 20 }, 13.0f,
               juce::Justification::centredLeft, white, true);
    drawLabel (g, "SCALE", { 212, 410, 110, 20 }, 13.0f,
               juce::Justification::centredLeft, white, true);

    drawLabel (g, "HUMANIZE", { 398, 401, 133, 22 }, 12.0f,
               juce::Justification::centred, muted, true);
    drawLabel (g, "RETUNE SPEED", { 542, 401, 133, 22 }, 12.0f,
               juce::Justification::centred, muted, true);
    drawLabel (g, "FORMANT COLOR", { 686, 401, 133, 22 }, 12.0f,
               juce::Justification::centred, muted, true);
    drawLabel (g, "MIX", { 830, 401, 133, 22 }, 12.0f,
               juce::Justification::centred, muted, true);
}

void SCTUNEEditor::resized()
{
    preset.setBounds (618, 38, 190, 34);
    bypass.setBounds (835, 40, 112, 30);

    key.setBounds (42, 450, 145, 38);
    scale.setBounds (212, 450, 155, 38);

    humanize.setBounds (404, 428, 121, 125);
    retune.setBounds (548, 428, 121, 125);
    formant.setBounds (692, 428, 121, 125);
    mix.setBounds (836, 428, 121, 125);
}
