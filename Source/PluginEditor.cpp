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

    key.addItemList ({ "C", "C#", "D", "D#", "E", "F",
                       "F#", "G", "G#", "A", "A#", "B" }, 1);
    scale.addItemList ({ "MAJOR", "MINOR", "CHROMATIC" }, 1);
    preset.addItemList ({ "HARD SC", "SOFT CLOUD", "ROBOT MODE" }, 1);
    preset.setSelectedId (1, juce::dontSendNotification);

    for (auto* combo : { &key, &scale, &preset })
    {
        combo->setColour (juce::ComboBox::backgroundColourId, background);
        combo->setColour (juce::ComboBox::textColourId, white);
        combo->setColour (juce::ComboBox::outlineColourId, accent);
        combo->setColour (juce::ComboBox::arrowColourId, white);
