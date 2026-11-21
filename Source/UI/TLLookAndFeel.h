#pragma once

#include <JuceHeader.h>
#include <BinaryData.h>

namespace tl
{
    // Paleta: base oscura + detalles en varias tonalidades de rojo
    inline const juce::Colour redHi   { 0xffff4a3d };
    inline const juce::Colour red     { 0xffd92b2b };
    inline const juce::Colour redMid  { 0xffb02226 };
    inline const juce::Colour redDark { 0xff7a1518 };
    inline const juce::Colour redDeep { 0xff3a0c0e };
    inline const juce::Colour cream   { 0xffe9e4d6 };

    //==========================================================================
    // Fuentes incrustadas en el plugin (Acidic: titulos; Nexa: resto del texto)
    struct FontHolder
    {
        FontHolder()
        {
            acidic     = juce::Typeface::createSystemTypefaceFor (BinaryData::acidic_ttf,
                                                                  (size_t) BinaryData::acidic_ttfSize);
            extraLight = juce::Typeface::createSystemTypefaceFor (BinaryData::nexa_extralight_ttf,
                                                                  (size_t) BinaryData::nexa_extralight_ttfSize);
            heavy      = juce::Typeface::createSystemTypefaceFor (BinaryData::nexa_heavy_ttf,
                                                                  (size_t) BinaryData::nexa_heavy_ttfSize);
            instance = this;
        }

        ~FontHolder()
        {
            if (instance == this) instance = nullptr;
        }

        juce::Typeface::Ptr acidic, extraLight, heavy;
        static inline FontHolder* instance = nullptr;
    };

    inline juce::Font makeFont (const juce::Typeface::Ptr& tf, float height)
    {
        if (tf != nullptr)
        {
            juce::Font f (tf);
            return f.withHeight (height);
        }

        juce::Font f { juce::FontOptions {} };
        return f.withHeight (height);
    }

    // Nexa: bold = true -> Heavy, bold = false -> Extra Light
    inline juce::Font font (float height, bool bold = false)
    {
        juce::Typeface::Ptr tf;
        if (auto* h = FontHolder::instance)
            tf = bold ? h->heavy : h->extraLight;
        return makeFont (tf, height);
    }

    // Acidic: solo para "TL TRIGGER", "DRUM TRIGGER" y "SAMPLE REPLACER"
    inline juce::Font titleFont (float height)
    {
        juce::Typeface::Ptr tf;
        if (auto* h = FontHolder::instance)
            tf = h->acidic;
        return makeFont (tf, height);
    }

    //==========================================================================
    inline void drawLED (juce::Graphics& g, juce::Point<float> c, float r, bool on,
                         juce::Colour col = redHi)
    {
        if (on)
        {
            juce::ColourGradient glow (col.withAlpha (0.55f), c.x, c.y,
                                       col.withAlpha (0.0f), c.x + r * 2.6f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - r * 2.6f, c.y - r * 2.6f, r * 5.2f, r * 5.2f);
        }

        juce::ColourGradient body (on ? col.brighter (0.5f) : juce::Colour (0xff2a1010),
                                   c.x - r * 0.3f, c.y - r * 0.4f,
                                   on ? col.darker (0.5f) : juce::Colour (0xff080303),
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
        setColour (juce::Slider::textBoxTextColourId,        juce::Colour (0xffd9d4c6));
        setColour (juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId,     juce::Colour (0xff0b0b0d));
        setColour (juce::TextEditor::textColourId,           tl::cream);
        setColour (juce::TextEditor::outlineColourId,        tl::redDeep);
        setColour (juce::TextEditor::focusedOutlineColourId, tl::redHi);
        setColour (juce::TextEditor::highlightColourId,      tl::redDark);
        setColour (juce::CaretComponent::caretColourId,      tl::redHi);
    }

    //--------------------------------------------------------------------------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float pos, float startAngle, float endAngle, juce::Slider&) override
    {
        constexpr float pi = juce::MathConstants<float>::pi;

        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const float dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto c = bounds.getCentre();
        const float R = dia * 0.5f - 1.5f;
        const bool big = dia > 120.f;
        const float lightA = juce::degreesToRadians (-40.f);   // luz desde arriba a la izquierda
        const float aNow = startAngle + pos * (endAngle - startAngle);

        auto pt = [&c] (float r, float a)
        {
            return juce::Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a));
        };

        // ---- disco de la escala, con borde rojo oscuro --------------------------
        {
            juce::ColourGradient disc (juce::Colour (0xff131417), c.x, c.y - R * 0.3f,
                                       juce::Colour (0xff040405), c.x, c.y + R * 1.0f, true);
            g.setGradientFill (disc);
            g.fillEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f);

            g.setColour (tl::redDark);
            g.drawEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f, 1.6f);
            g.setColour (tl::redHi.withAlpha (0.16f));
            g.drawEllipse (c.x - R + 1.8f, c.y - R + 1.8f, R * 2.f - 3.6f, R * 2.f - 3.6f, 0.8f);
        }

        // ---- marcas -------------------------------------------------------------
        const int numTicks = 41;
        for (int i = 0; i < numTicks; ++i)
        {
            const bool major = (i % 4) == 0;
            const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (numTicks - 1);
            g.setColour (juce::Colours::white.withAlpha (major ? 0.92f : 0.5f));
            g.drawLine (juce::Line<float> (pt (R * 0.97f, a),
                                           pt (R * (major ? (big ? 0.89f : 0.88f) : 0.93f), a)),
                        major ? 1.4f : 0.9f);
        }

        // ---- numeros 0..10 (solo perillas grandes) --------------------------------
        if (big)
        {
            g.setFont (tl::font (13.f, false));
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = startAngle + (endAngle - startAngle) * (float) i / 10.f;
                g.drawText (juce::String (i),
                            juce::Rectangle<float> (26.f, 15.f).withCentre (pt (R * 0.70f, a)).toNearestInt(),
                            juce::Justification::centred, false);
            }
        }

        // ---- arco de valor en rojo, con resplandor ---------------------------------
        {
            const float arcR = R * (big ? 0.855f : 0.80f);
            if (pos > 0.002f)
            {
                juce::Path v;
                v.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, aNow, true);

                g.setColour (tl::red.withAlpha (0.28f));
                g.strokePath (v, juce::PathStrokeType (big ? 6.5f : 5.f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
                g.setColour (tl::redHi);
                g.strokePath (v, juce::PathStrokeType (big ? 2.2f : 1.8f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
            }
        }

        // ---- cuerpo de la perilla ----------------------------------------------------
        const float bodyR = R * (big ? 0.60f : 0.64f);
        juce::Path bodyPath;
        bodyPath.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f);

        // sombra proyectada + sombra de contacto
        juce::DropShadow (juce::Colours::black.withAlpha (0.95f), (int) (R * 0.30f),
                          { 0, (int) (R * 0.14f) }).drawForPath (g, bodyPath);
        juce::DropShadow (juce::Colours::black.withAlpha (0.8f), (int) (R * 0.08f),
                          { 0, (int) (R * 0.02f) }).drawForPath (g, bodyPath);

        // faldon moleteado (estrias alrededor del borde)
        {
            juce::ColourGradient skirt (juce::Colour (0xff2a2b30), c.x - bodyR * 0.5f, c.y - bodyR * 0.6f,
                                        juce::Colour (0xff060607), c.x + bodyR * 0.8f, c.y + bodyR * 0.9f, true);
            g.setGradientFill (skirt);
            g.fillPath (bodyPath);

            const int ridges = big ? 56 : 40;
            for (int i = 0; i < ridges; ++i)
            {
                const float a  = 2.f * pi * (float) i / (float) ridges;
                const float a2 = a + pi / (float) ridges;
                const float lit = juce::jmax (0.f, std::cos (a - lightA));

                g.setColour (juce::Colours::white.withAlpha (0.05f + 0.30f * lit * lit));
                g.drawLine (juce::Line<float> (pt (bodyR * 0.90f, a), pt (bodyR * 0.995f, a)), 1.0f);
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.drawLine (juce::Line<float> (pt (bodyR * 0.90f, a2), pt (bodyR * 0.995f, a2)), 1.0f);
            }

            g.setColour (juce::Colours::black);
            g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.f, bodyR * 2.f, 1.5f);
        }

        // tapa superior con acabado cepillado
        const float capR = bodyR * 0.80f;
        {
            juce::ColourGradient cap (juce::Colour (0xff3c3e45), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                      juce::Colour (0xff09090a), c.x + capR * 0.9f, c.y + capR * 1.0f, true);
            g.setGradientFill (cap);
            g.fillEllipse (c.x - capR, c.y - capR, capR * 2.f, capR * 2.f);

            // anillos concentricos finos
            for (float r = capR * 0.14f; r < capR; r += 1.9f)
            {
                const float t = 0.5f + 0.5f * std::sin (r * 2.7f);
                g.setColour (juce::Colours::white.withAlpha (0.015f + 0.05f * t));
                g.drawEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f, 0.6f);
            }

            // reflejos anisotropicos opuestos (efecto metal cepillado)
            juce::Path wedge;
            wedge.addPieSegment (c.x - capR, c.y - capR, capR * 2.f, capR * 2.f,
                                 lightA - 0.38f, lightA + 0.38f, 0.f);
            wedge.addPieSegment (c.x - capR, c.y - capR, capR * 2.f, capR * 2.f,
                                 lightA + pi - 0.38f, lightA + pi + 0.38f, 0.f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillPath (wedge);

            // brillo superior
            juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.18f), c.x, c.y - capR * 0.9f,
                                        juce::Colours::white.withAlpha (0.0f),  c.x, c.y - capR * 0.1f, false);
            g.setGradientFill (gloss);
            g.fillEllipse (c.x - capR * 0.66f, c.y - capR * 0.94f, capR * 1.32f, capR * 0.82f);

            // borde de la tapa
            g.setColour (juce::Colours::black);
            g.drawEllipse (c.x - capR, c.y - capR, capR * 2.f, capR * 2.f, 1.6f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawEllipse (c.x - capR + 1.5f, c.y - capR + 1.5f, capR * 2.f - 3.f, capR * 2.f - 3.f, 0.8f);
        }

        // ---- indicador: surco con linea roja -------------------------------------------
        {
            const auto p0 = pt (capR * 0.30f, aNow);
            const auto p1 = pt (capR * 0.94f, aNow);

            g.setColour (tl::red.withAlpha (0.16f));
            g.drawLine (juce::Line<float> (p0, p1), big ? 9.f : 7.f);

            g.setColour (juce::Colours::black.withAlpha (0.9f));
            g.drawLine (juce::Line<float> (p0, p1), big ? 5.2f : 4.2f);

            g.setColour (tl::redMid);
            g.drawLine (juce::Line<float> (p0, p1), big ? 3.2f : 2.6f);

            g.setColour (tl::redHi);
            g.drawLine (juce::Line<float> (pt (capR * 0.34f, aNow), pt (capR * 0.90f, aNow)),
                        big ? 1.6f : 1.3f);

            // tornillo central
            const float sr = capR * 0.10f;
            g.setColour (juce::Colour (0xff0a0a0b));
            g.fillEllipse (c.x - sr, c.y - sr, sr * 2.f, sr * 2.f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawEllipse (c.x - sr, c.y - sr, sr * 2.f, sr * 2.f, 0.8f);
        }
    }

    //--------------------------------------------------------------------------
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();

        juce::Colour top    = on ? juce::Colour (0xff3a1316) : juce::Colour (0xff2b2d33);
        juce::Colour bottom = on ? juce::Colour (0xff140708) : juce::Colour (0xff0c0c0e);
        if (down) top = top.darker (0.5f);

        juce::ColourGradient grad (top, 0.f, r.getY(), bottom, 0.f, r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 5.f);

        g.setColour (on ? tl::redHi.withAlpha (0.85f)
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

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return tl::font (12.5f, true); }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        tl::drawLED (g, { r.getX() + 12.f, r.getCentreY() }, 5.f, b.getToggleState());
        g.setColour (juce::Colour (0xffd9d4c6));
        g.setFont (tl::font (13.5f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (28.f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }

    juce::Font getLabelFont (juce::Label&) override { return tl::font (13.f, true); }
};
