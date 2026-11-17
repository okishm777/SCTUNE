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
