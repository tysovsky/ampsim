#include "BlockIcons.h"

BlockIcon blockIconFromName (const String& name)
{
    if (name.equalsIgnoreCase ("amp"))      return BlockIcon::amp;
    if (name.equalsIgnoreCase ("pedal"))    return BlockIcon::pedal;
    if (name.equalsIgnoreCase ("eq"))       return BlockIcon::eq;

    return BlockIcon::plugin;
}

String getBlockIconCategory (BlockIcon icon)
{
    switch (icon)
    {
        case BlockIcon::amp:    return "Amps";
        case BlockIcon::pedal:  return "Pedals";
        case BlockIcon::eq:     return "EQ";
        case BlockIcon::input:
        case BlockIcon::output: return "I/O";
        case BlockIcon::plugin:
        case BlockIcon::suite:  break;
    }

    return "Plugins";
}

void drawBlockIcon (Graphics& g, BlockIcon icon, Rectangle<float> area, Colour colour)
{
    // Each icon is drawn in a unit square, then scaled into the largest square that fits the area
    const auto size = jmin (area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre (size, size);

    const auto point = [&] (float x, float y)       { return box.getRelativePoint (x, y); };
    const auto rect = [&] (float x1, float y1, float x2, float y2)
    {
        return Rectangle<float> (point (x1, y1), point (x2, y2));
    };
    const auto dot = [&] (float x, float y, float radius)
    {
        g.fillEllipse (Rectangle<float> (radius * 2.0f * size, radius * 2.0f * size).withCentre (point (x, y)));
    };

    const auto thickness = jmax (1.5f, size * 0.06f);
    const PathStrokeType stroke (thickness, PathStrokeType::curved, PathStrokeType::rounded);
    Path path;

    g.setColour (colour);

    switch (icon)
    {
        case BlockIcon::amp:
        {
            // An amp head: handle, cabinet, grille and a row of knobs
            path.addRoundedRectangle (rect (0.08f, 0.24f, 0.92f, 0.84f), size * 0.06f);
            path.startNewSubPath (point (0.38f, 0.24f));
            path.lineTo (point (0.38f, 0.14f));
            path.lineTo (point (0.62f, 0.14f));
            path.lineTo (point (0.62f, 0.24f));
            path.addRectangle (rect (0.18f, 0.34f, 0.82f, 0.58f));
            g.strokePath (path, stroke);

            for (auto x : { 0.26f, 0.42f, 0.58f, 0.74f })
                dot (x, 0.71f, 0.045f);

            break;
        }

        case BlockIcon::pedal:
        {
            // A stompbox: two knobs, an LED and a footswitch
            path.addRoundedRectangle (rect (0.22f, 0.08f, 0.78f, 0.92f), size * 0.08f);
            path.addEllipse (Rectangle<float> (0.24f * size, 0.24f * size).withCentre (point (0.5f, 0.70f)));
            g.strokePath (path, stroke);

            dot (0.38f, 0.24f, 0.065f);
            dot (0.62f, 0.24f, 0.065f);
            dot (0.5f, 0.43f, 0.03f);
            dot (0.5f, 0.70f, 0.055f);
            break;
        }

        case BlockIcon::eq:
        {
            // Three sliders at different heights
            const float columns[] = { 0.26f, 0.5f, 0.74f };
            const float levels[]  = { 0.62f, 0.32f, 0.50f };

            for (int i = 0; i < 3; ++i)
            {
                path.startNewSubPath (point (columns[i], 0.12f));
                path.lineTo (point (columns[i], 0.88f));
            }

            g.strokePath (path, stroke);

            for (int i = 0; i < 3; ++i)
                g.fillRoundedRectangle (Rectangle<float> (0.18f * size, 0.09f * size).withCentre (point (columns[i], levels[i])),
                                        size * 0.02f);
            break;
        }

        case BlockIcon::suite:
        {
            // Stacked units: one plugin made of several blocks
            path.addRoundedRectangle (rect (0.12f, 0.38f, 0.88f, 0.88f), size * 0.06f);
            path.startNewSubPath (point (0.20f, 0.27f));
            path.lineTo (point (0.80f, 0.27f));
            path.startNewSubPath (point (0.28f, 0.16f));
            path.lineTo (point (0.72f, 0.16f));
            g.strokePath (path, stroke);

            for (auto x : { 0.32f, 0.5f, 0.68f })
                dot (x, 0.63f, 0.05f);

            break;
        }

        case BlockIcon::input:
        {
            // A jack plug on the end of a cable, pointing into the chain
            path.startNewSubPath (point (0.04f, 0.5f));
            path.lineTo (point (0.18f, 0.5f));
            path.addRoundedRectangle (rect (0.18f, 0.30f, 0.54f, 0.70f), size * 0.06f);
            path.startNewSubPath (point (0.32f, 0.30f));
            path.lineTo (point (0.32f, 0.70f));
            path.addRectangle (rect (0.54f, 0.40f, 0.66f, 0.60f));
            path.startNewSubPath (point (0.66f, 0.5f));
            path.lineTo (point (0.84f, 0.5f));
            g.strokePath (path, stroke);

            dot (0.88f, 0.5f, 0.07f);
            break;
        }

        case BlockIcon::output:
        {
            // A speaker with sound coming out
            path.startNewSubPath (point (0.10f, 0.38f));
            path.lineTo (point (0.28f, 0.38f));
            path.lineTo (point (0.52f, 0.16f));
            path.lineTo (point (0.52f, 0.84f));
            path.lineTo (point (0.28f, 0.62f));
            path.lineTo (point (0.10f, 0.62f));
            path.closeSubPath();

            path.addCentredArc (box.getX() + 0.52f * size, box.getCentreY(), 0.16f * size, 0.16f * size,
                                0.0f, MathConstants<float>::pi * 0.25f, MathConstants<float>::pi * 0.75f, true);
            path.addCentredArc (box.getX() + 0.52f * size, box.getCentreY(), 0.32f * size, 0.32f * size,
                                0.0f, MathConstants<float>::pi * 0.25f, MathConstants<float>::pi * 0.75f, true);
            g.strokePath (path, stroke);
            break;
        }

        case BlockIcon::plugin:
        {
            // A generic unit: a screen with a waveform and a knob
            path.addRoundedRectangle (rect (0.08f, 0.20f, 0.92f, 0.80f), size * 0.06f);
            path.addRoundedRectangle (rect (0.18f, 0.32f, 0.62f, 0.68f), size * 0.03f);
            path.startNewSubPath (point (0.22f, 0.5f));

            for (int i = 1; i <= 16; ++i)
            {
                const auto t = (float) i / 16.0f;
                path.lineTo (point (0.22f + t * 0.36f, 0.5f - 0.1f * std::sin (t * MathConstants<float>::twoPi)));
            }

            g.strokePath (path, stroke);
            dot (0.77f, 0.5f, 0.07f);
            break;
        }
    }
}
