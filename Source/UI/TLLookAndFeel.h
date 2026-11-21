#pragma once

#include <JuceHeader.h>

namespace tl
{
    inline const juce::Colour teal  { 0xff2fe6d0 };
    inline const juce::Colour cream { 0xffe9e4d6 };

    inline juce::Font font (float height, bool bold = false)
    {
        juce::Font f { juce::FontOptions {} };
        f = f.withHeight (height);
        return bold ? f.boldened() : f;
    }

    inline void drawLED (juce::Graphics& g, juce::Point<float> c, float r, bool on,
                         juce::Colour col = teal)
    {
        if (on)
        {
            juce::ColourGradient glow (col.withAlpha (0.55f), c.x, c.y,
                                       col.withAlpha (0.0f), c.x + r * 2.6f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - r * 2.6f, c.y - r * 2.6f, r * 5.2f, r * 5.2f);
        }

        juce::ColourGradient body (on ? col.brighter (0.6f) : juce::Colour (0xff1a2a2a),
                                   c.x - r * 0.3f, c.y - r * 0.4f,
                                   on ? col.darker (0.4f) : juce::Colour (0xff050808),
                                   c.x + r, c.y + r, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);

        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 1.2f);

        g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.15f));
        g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.65f, r * 0.5f, r * 0.35f);
    }
}

//==============================================================================
class TLLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TLLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffd9d4c6));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId,    juce::Colour (0xff0b0b0d));
        setColour (juce::TextEditor::textColourId,          tl::cream);
        setColour (juce::TextEditor::outlineColourId,       juce::Colour (0xff3a3c42));
        setColour (juce::TextEditor::focusedOutlineColourId, tl::teal);
        setColour (juce::CaretComponent::caretColourId,     tl::teal);
    }

    //--------------------------------------------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto c = bounds.getCentre();
        const float R = dia * 0.5f - 1.5f;
        const bool big = dia > 120.f;

        auto pt = [&c] (float r, float a) { return juce::Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a)); };

        // disco de la escala
        g.setColour (juce::Colour (0xff09090b));
        g.fillEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f, 1.3f);

        // marcas
        const int numTicks = 41;
        for (int i = 0; i < numTicks; ++i)
        {
            const bool major = (i % 4) == 0;
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (numTicks - 1);
            g.setColour (juce::Colours::white.withAlpha (major ? 0.95f : 0.55f));
            g.drawLine (juce::Line<float> (pt (R * 0.96f, a), pt (R * (major ? 0.84f : 0.90f), a)),
                        major ? 1.4f : 0.9f);
        }

        // numeros 0..10 (solo perillas grandes)
        if (big)
        {
            g.setFont (tl::font (11.f));
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = startAngle + (endAngle - startAngle) * (float) i / 10.f;
                g.drawText (juce::String (i),
                            juce::Rectangle<float> (24.f, 14.f).withCentre (pt (R * 0.74f, a)).toNearestInt(),
                            juce::Justification::centred, false);
            }
        }

        // cuerpo de la perilla
        const float bodyR = R * (big ? 0.60f : 0.66f);
        juce::Path body;
        body.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.85f), (int) (R * 0.18f),
                          { 0, (int) (R * 0.08f) }).drawForPath (g, body);

        juce::ColourGradient fill (juce::Colour (0xff3b3e45), c.x - bodyR * 0.45f, c.y - bodyR * 0.55f,
                                   juce::Colour (0xff060607), c.x + bodyR * 0.7f,  c.y + bodyR * 0.8f, true);
        g.setGradientFill (fill);
        g.fillPath (body);

        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f, 1.5f);

        // brillo
        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.22f), c.x, c.y - bodyR * 0.85f,
                                    juce::Colours::white.withAlpha (0.0f),  c.x, c.y - bodyR * 0.05f, false);
        g.setGradientFill (gloss);
        g.fillEllipse (c.x - bodyR * 0.62f, c.y - bodyR * 0.92f, bodyR * 1.24f, bodyR * 0.80f);

        // indicador blanco
        const float a = startAngle + pos * (endAngle - startAngle);
        const auto p0 = pt (bodyR * 0.12f, a), p1 = pt (bodyR * 0.96f, a);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (p0.x + 1.f, p0.y + 1.f, p1.x + 1.f, p1.y + 1.f, big ? 3.6f : 2.8f);
        g.setColour (juce::Colours::white);
        g.drawLine (juce::Line<float> (p0, p1), big ? 3.2f : 2.5f);
    }

    //--------------------------------------------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();

        juce::ColourGradient grad (down ? juce::Colour (0xff101114) : juce::Colour (0xff2b2d33), 0.f, r.getY(),
                                   juce::Colour (0xff0c0c0e), 0.f, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 5.f);

        g.setColour (on ? tl::teal.withAlpha (0.8f)
                        : juce::Colour (0xff3a3c42).withAlpha (highlighted ? 1.f : 0.8f));
        g.drawRoundedRectangle (r, 5.f, 1.2f);

        if (b.getProperties().contains ("led"))
            tl::drawLED (g, { r.getX() + 14.f, r.getCentreY() }, 4.f, on);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (juce::Colour (b.getToggleState() ? 0xfff0ece0 : 0xffb9b5aa));
        auto area = b.getLocalBounds();
        if (b.getProperties().contains ("led")) area.removeFromLeft (22);
        g.drawText (b.getButtonText(), area, juce::Justification::centred, false);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return tl::font (11.5f, true); }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        tl::drawLED (g, { r.getX() + 12.f, r.getCentreY() }, 5.f, b.getToggleState());
        g.setColour (juce::Colour (0xffd9d4c6));
        g.setFont (tl::font (12.f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (28.f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }

    juce::Font getLabelFont (juce::Label&) override { return tl::font (12.f); }
};
