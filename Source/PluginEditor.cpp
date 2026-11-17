#include "PluginEditor.h"
#include "SCTUNELogo.h"
#include <array>
#include <cmath>

namespace {
const juce::Colour accent(0xffae36ff), bright(0xffd58aff);
const juce::Colour background(0xff0c0715), panel(0xff1b1029);
const juce::Colour muted(0xffb7a4ca), white(0xfff9f4ff);

void roundedPanel(juce::Graphics& g, juce::Rectangle<float> r, float radius) {
    g.setColour(panel); g.fillRoundedRectangle(r, radius);
    g.setColour(accent.withAlpha(0.48f)); g.drawRoundedRectangle(r.reduced(0.8f), radius, 1.3f);
}

void label(juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r,
           float size, juce::Justification align = juce::Justification::centred,
           juce::Colour colour = muted, bool bold = false) {
    g.setColour(colour);
    g.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)));
    g.drawFittedText(s, r, align, 1);
}

class PurpleLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float proportion, float startAngle, float endAngle, juce::Slider&) override {
        const auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) - 18.0f;
        const float radius = diameter * 0.5f;
        const auto centre = bounds.getCentre();
        const float stroke = 8.0f;
        juce::Path track, active;
        track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                            startAngle, endAngle, true);
        active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f,
                             startAngle, startAngle + proportion * (endAngle - startAngle), true);
        g.setColour(juce::Colour(0xff483453));
        g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
        g.setColour(accent.withAlpha(0.18f));
        g.strokePath(active, juce::PathStrokeType(stroke + 7.0f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.setColour(accent);
        g.strokePath(active, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.setColour(juce::Colour(0xff241630));
        g.fillEllipse(centre.x - radius + 13.0f, centre.y - radius + 13.0f,
                      (radius - 13.0f) * 2.0f, (radius - 13.0f) * 2.0f);
        const float angle = startAngle + proportion * (endAngle - startAngle);
        const auto dot = centre.getPointOnCircumference(radius, angle);
        g.setColour(white); g.fillEllipse(dot.x - 6.0f, dot.y - 6.0f, 12.0f, 12.0f);
    }
};
PurpleLookAndFeel purpleLook;
}

SCTUNEEditor::SCTUNEEditor(SCTUNEProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    setSize(980, 580);
    setupKnob(humanize, "HUMANIZE");
    setupKnob(retune, "RETUNE SPEED");
    setupKnob(formant, "FORMANT COLOR");
    setupKnob(mix, "MIX");

    key.addItemList({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    scale.addItemList({"MAJOR", "MINOR", "CHROMATIC"}, 1);
    for (auto* c : {&key, &scale, &preset}) {
        c->setColour(juce::ComboBox::backgroundColourId, background);
        c->setColour(juce::ComboBox::textColourId, white);
        c->setColour(juce::ComboBox::outlineColourId, accent);
        c->setColour(juce::ComboBox::arrowColourId, white);
        addAndMakeVisible(*c);
    }
    preset.addItemList({"HARD SC", "SOFT CLOUD", "ROBOT MODE"}, 1);
    preset.setSelectedId(1, juce::dontSendNotification);
    preset.onChange = [this] {
        auto set = [this](const char* id, float value) {
            if (auto* parameter = processor.state.getParameter(id))
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        switch (preset.getSelectedId()) {
            case 1: set("retune", 100); set("humanize", 0); set("mix", 100); set("formant", 0); break;
            case 2: set("retune", 45); set("humanize", 70); set("mix", 75); set("formant", 0); break;
            case 3: set("retune", 100); set("humanize", 0); set("mix", 100); set("formant", -40); break;
            default: break;
        }
    };
    bypass.setColour(juce::ToggleButton::textColourId, white);
    bypass.setColour(juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible(bypass);
    aRetune = std::make_unique<SA>(processor.state, "retune", retune);
    aHumanize = std::make_unique<SA>(processor.state, "humanize", humanize);
    aFormant = std::make_unique<SA>(processor.state, "formant", formant);
    aMix = std::make_unique<SA>(processor.state, "mix", mix);
