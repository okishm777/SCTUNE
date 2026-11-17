
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

void drawPanel (juce::Graphics& g,
                juce::Rectangle<float> area,
                float radius)
{
    g.setColour (panel);
    g.fillRoundedRectangle (area, radius);
    g.setColour (accent.withAlpha (0.25f));
    g.drawRoundedRectangle (area.reduced (0.5f),
                            radius, 1.0f);
}

void drawLabel (juce::Graphics& g,
                const juce::String& text,
                juce::Rectangle<int> area,
                float size,
                juce::Justification alignment,
                juce::Colour colour = muted,
                bool isBold = false)
{
    g.setColour (colour);
    g.setFont (juce::Font (
        juce::FontOptions (
            size,
            isBold ? juce::Font::bold
                   : juce::Font::plain)));
    g.drawFittedText (text, area, alignment, 1);
}

class PurpleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (
        juce::Graphics& g,
        int x, int y, int width, int height,
        float position,
        float startAngle,
        float endAngle,
        juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> (
            (float) x, (float) y,
            (float) width, (float) height);

        auto centre = bounds.getCentre();
        float radius = juce::jmin (
            bounds.getWidth(),
            bounds.getHeight()) * 0.38f;

        juce::Path track;
        track.addCentredArc (
            centre.x, centre.y,
            radius, radius, 0.0f,
            startAngle, endAngle, true);

        juce::Path active;
        float angle = startAngle
            + position * (endAngle - startAngle);

        active.addCentredArc (
            centre.x, centre.y,
            radius, radius, 0.0f,
            startAngle, angle, true);

        g.setColour (juce::Colour (0xff483453));
        g.strokePath (
            track, juce::PathStrokeType (8.0f));

        g.setColour (accent);
        g.strokePath (
            active, juce::PathStrokeType (8.0f));

        auto dot = centre.getPointOnCircumference (
            radius, angle);

        g.setColour (white);
        g.fillEllipse (
            dot.x - 5.0f, dot.y - 5.0f,
            10.0f, 10.0f);
    }
};

PurpleLookAndFeel purpleLook;
}

SCTUNEEditor::SCTUNEEditor (SCTUNEProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (980, 580);

    setupKnob (humanize, "HUMANIZE");
    setupKnob (retune, "RETUNE SPEED");
    setupKnob (formant, "FORMANT COLOR");
    setupKnob (mix, "MIX");

    key.addItemList (
        {"C", "C#", "D", "D#", "E", "F",
         "F#", "G", "G#", "A", "A#", "B"}, 1);

    scale.addItemList (
        {"MAJOR", "MINOR", "CHROMATIC"}, 1);

    for (auto* combo : {&key, &scale, &preset})
    {
        combo->setColour (
            juce::ComboBox::backgroundColourId,
            background);
        combo->setColour (
            juce::ComboBox::textColourId, white);
        combo->setColour (
            juce::ComboBox::outlineColourId, accent);
        addAndMakeVisible (*combo);
    }

    preset.addItemList (
        {"HARD SC", "SOFT CLOUD", "ROBOT MODE"}, 1);
    preset.setSelectedId (
        1, juce::dontSendNotification);

    preset.onChange = [this]
    {
        auto set = [this] (
            const char* id, float value)
        {
            if (auto* param =
                    processor.state.getParameter (id))
                param->setValueNotifyingHost (
                    param->convertTo0to1 (value));
        };

        switch (preset.getSelectedId())
        {
            case 1:
                set ("retune", 100);
                set ("humanize", 0);
                set ("mix", 100);
                set ("formant", 0);
                break;
            case 2:
                set ("retune", 45);
                set ("humanize", 70);
                set ("mix", 75);
                set ("formant", 0);
                break;
            case 3:
                set ("retune", 100);
                set ("humanize", 0);
                set ("mix", 100);
                set ("formant", -40);
                break;
            default:
                break;
        }
    };

    bypass.setColour (
        juce::ToggleButton::textColourId, white);
    bypass.setColour (
        juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (bypass);

    aRetune = std::make_unique<SA> (
        processor.state, "retune", retune);
    aHumanize = std::make_unique<SA> (
        processor.state, "humanize", humanize);
    aFormant = std::make_unique<SA> (
        processor.state, "formant", formant);
    aMix = std::make_unique<SA> (
        processor.state, "mix", mix);
    aKey = std::make_unique<CA> (
        processor.state, "key", key);
    aScale = std::make_unique<CA> (
        processor.state, "scale", scale);
    aBypass = std::make_unique<BA> (
        processor.state, "bypass", bypass);

    startTimerHz (30);
}

void SCTUNEEditor::setupKnob (
    juce::Slider& slider,
    const juce::String& name)
{
    slider.setName (name);
    slider.setSliderStyle (
        juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (
        juce::Slider::TextBoxBelow,
        false, 80, 24);
    slider.setNumDecimalPlacesToDisplay (0);
    slider.textFromValueFunction = [] (double value)
    {
        return juce::String (
            juce::roundToInt (value));
    };
