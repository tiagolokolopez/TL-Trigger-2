#pragma once

#include <JuceHeader.h>
#include "TLLookAndFeel.h"

//==============================================================================
// Vumetro analogico de cara crema. 0 VU = -14 dBFS (RMS).
class VUMeter : public juce::Component
{
public:
    explicit VUMeter (juce::String labelText) : label (std::move (labelText))
    {
        setInterceptsMouseClicks (false, false);
    }

    void setLevel (float amplitude)
    {
        const float target = juce::jlimit (0.f, 1.1f, amplitude / (refAmp * 1.4125f));
        const float prev = needle;
        needle += 0.55f * (target - needle);
        if (std::abs (needle - prev) > 0.0005f) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // bisel
        g.setColour (juce::Colour (0xff0b0b0d));
        g.fillRoundedRectangle (b, 16.f);
        juce::ColourGradient bez (juce::Colour (0xff2a2c31), b.getX(), b.getY(),
                                  juce::Colour (0xff0a0a0c), b.getRight(), b.getBottom(), false);
        g.setGradientFill (bez);
        g.drawRoundedRectangle (b.reduced (1.f), 16.f, 2.f);
        g.setColour (tl::redDeep.withAlpha (0.9f));
        g.drawRoundedRectangle (b.reduced (5.f), 12.f, 1.f);

        // cara
        auto f = b.reduced (12.f);
        juce::Path facePath;
        facePath.addRoundedRectangle (f, 9.f);

        juce::ColourGradient cream (juce::Colour (0xfff4e8bf), f.getCentreX(), f.getCentreY() + f.getHeight() * 0.1f,
                                    juce::Colour (0xffbfa968), f.getX(), f.getY(), true);
        g.setGradientFill (cream);
        g.fillPath (facePath);

        g.saveState();
        g.reduceClipRegion (facePath);

        const float h = f.getHeight();
        const juce::Point<float> pivot { f.getCentreX(), f.getY() + h * 1.12f };
        const float r = h * 0.86f;
        const float halfSweep = juce::degreesToRadians (40.f);
        auto angleFor = [halfSweep] (float pos) { return -halfSweep + 2.f * halfSweep * pos; };
        auto pointAt  = [&pivot] (float rad, float a) { return juce::Point<float> (pivot.x + rad * std::sin (a),
                                                                                 pivot.y - rad * std::cos (a)); };

        // arco negro hasta 0 VU y zona roja
        const float zeroPos = vuToPos (0.f);
        const juce::Colour ink (0xff15130f);

        juce::Path arc;
        arc.addCentredArc (pivot.x, pivot.y, r, r, 0.f, angleFor (vuToPos (-20.f)), angleFor (zeroPos), true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (1.4f));

        juce::Path red;
        red.addCentredArc (pivot.x, pivot.y, r, r, 0.f, angleFor (zeroPos), angleFor (1.f), true);
        g.setColour (juce::Colour (0xffc9252a));
        g.strokePath (red, juce::PathStrokeType (5.f));

        // marcas y numeros
        const float vals[] = { -20.f, -10.f, -7.f, -5.f, -3.f, -2.f, -1.f, 0.f, 1.f, 2.f, 3.f };
        g.setFont (tl::font (12.f, true));
        for (float v : vals)
        {
            const float a = angleFor (vuToPos (v));
            const bool isRed = v > 0.f;
            g.setColour (isRed ? juce::Colour (0xffc9252a) : ink);
            g.drawLine (juce::Line<float> (pointAt (r, a), pointAt (r - 7.f, a)), 1.4f);

            const juce::String txt = v > 0.f ? "+" + juce::String ((int) v) : juce::String ((int) v);
            g.drawText (txt, juce::Rectangle<float> (32.f, 14.f).withCentre (pointAt (r + 13.f, a)).toNearestInt(),
                        juce::Justification::centred, false);
        }

        g.setColour (ink.withAlpha (0.85f));
        g.setFont (tl::font (13.f, true));
        g.drawText ("VU", juce::Rectangle<int> ((int) f.getX() + 12, (int) f.getY() + 8, 40, 16),
                    juce::Justification::centredLeft, false);

        // aguja (rojo oscuro)
        const float a = angleFor (juce::jlimit (0.f, 1.06f, needle));
        const auto tip = pointAt (r, a);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawLine (pivot.x + 3.f, pivot.y + 1.f, tip.x + 3.f, tip.y + 1.f, 2.2f);
        g.setColour (tl::redDark);
        g.drawLine (pivot.x, pivot.y, tip.x, tip.y, 1.9f);

        // placa inferior
        juce::Rectangle<float> plate (f.getCentreX() - f.getWidth() * 0.30f, f.getBottom() - 38.f,
                                      f.getWidth() * 0.60f, 60.f);
        g.setColour (juce::Colour (0xff101113));
        g.fillRoundedRectangle (plate, 22.f);
        g.setColour (tl::redMid.withAlpha (0.8f));
        g.drawRoundedRectangle (plate.reduced (0.5f), 22.f, 1.f);
        g.setColour (juce::Colour (0xfff0e6c8));
        g.setFont (tl::font (13.f, true).withExtraKerningFactor (0.08f));
        g.drawText (label, plate.withHeight (30.f).translated (0.f, 6.f).toNearestInt(),
                    juce::Justification::centred, false);

        // vineta + brillo del cristal
        juce::ColourGradient vig (juce::Colours::transparentBlack, f.getCentreX(), f.getCentreY(),
                                  juce::Colour (0x55402800), f.getX(), f.getY(), true);
        g.setGradientFill (vig);
        g.fillRect (f);

        juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.16f), f.getX(), f.getY(),
                                    juce::Colours::white.withAlpha (0.0f),  f.getX(), f.getY() + h * 0.55f, false);
        g.setGradientFill (gloss);
        g.fillRect (f);

        g.restoreState();
    }

private:
    static constexpr float refAmp = 0.1995f;   // -14 dBFS

    static float vuToPos (float vu) { return std::pow (10.f, vu / 20.f) / 1.4125f; }

    float needle = 0.f;
    juce::String label;
};

//==============================================================================
// Lampara roja que destella cada vez que se dispara un sample.
class JewelLamp : public juce::Component
{
public:
    JewelLamp() { setInterceptsMouseClicks (false, false); }

    void flash() noexcept { intensity = 1.f; }

    void tick()
    {
        const float prev = intensity;
        intensity *= 0.80f;
        if (intensity < 0.02f) intensity = 0.f;
        if (prev != intensity) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

        if (intensity > 0.f)
        {
            juce::ColourGradient glow (tl::redHi.withAlpha (0.55f * intensity), c.x, c.y,
                                       juce::Colours::transparentBlack, c.x + R, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (b);
        }

        const float r1 = R * 0.62f;

        juce::ColourGradient ring (juce::Colour (0xff5a5d64), c.x - r1, c.y - r1,
                                   juce::Colour (0xff121315), c.x + r1, c.y + r1, false);
        g.setGradientFill (ring);
        g.fillEllipse (c.x - r1 - 6.f, c.y - r1 - 6.f, (r1 + 6.f) * 2.f, (r1 + 6.f) * 2.f);
        g.setColour (tl::redDark.withAlpha (0.8f));
        g.drawEllipse (c.x - r1 - 6.f, c.y - r1 - 6.f, (r1 + 6.f) * 2.f, (r1 + 6.f) * 2.f, 1.2f);

        const auto unlitA = juce::Colour (0xff5a1013), unlitB = juce::Colour (0xff1a0405);
        const auto litA   = juce::Colour (0xffffb3a8), litB   = juce::Colour (0xffc0141b);
        juce::ColourGradient lens (unlitA.interpolatedWith (litA, intensity), c.x - r1 * 0.3f, c.y - r1 * 0.4f,
                                   unlitB.interpolatedWith (litB, intensity), c.x + r1, c.y + r1, true);
        g.setGradientFill (lens);
        g.fillEllipse (c.x - r1, c.y - r1, r1 * 2.f, r1 * 2.f);

        g.setColour (juce::Colour (0xff3b3d42));
        g.fillEllipse (c.x - r1 * 0.28f, c.y - r1 * 0.28f, r1 * 0.56f, r1 * 0.56f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (c.x - r1 * 0.55f, c.y - r1 * 0.85f, r1 * 0.7f, r1 * 0.4f);
    }

private:
    float intensity = 0.f;
};
