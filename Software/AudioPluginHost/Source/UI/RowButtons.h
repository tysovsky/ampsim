#pragma once

#include <JuceHeader.h>
#include "BlockIcons.h"
#include "ThemeManager.h"

//==============================================================================
/** A bin, for deleting something from a list. It goes red while it's pressed. */
class BinButton final : public Button
{
public:
    BinButton() : Button ("Delete")
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (Graphics& g, bool, bool isButtonDown) override
    {
        const auto size = jmin (24.0f, (float) jmin (getWidth(), getHeight()));

        drawBinIcon (g, getLocalBounds().toFloat().withSizeKeepingCentre (size, size),
                     isButtonDown ? Colour (0xffe0342c) : findColour (Label::textColourId).withAlpha (0.7f));
    }
};

//==============================================================================
/** A pencil, for renaming something in a list. */
class PencilButton final : public Button
{
public:
    PencilButton() : Button ("Rename")
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (Graphics& g, bool, bool isButtonDown) override
    {
        const auto size = jmin (24.0f, (float) jmin (getWidth(), getHeight()));
        const auto box = getLocalBounds().toFloat().withSizeKeepingCentre (size, size);
        const auto point = [&] (float x, float y) { return box.getRelativePoint (x, y); };

        Path pencil;
        pencil.startNewSubPath (point (0.10f, 0.90f));
        pencil.lineTo (point (0.16f, 0.66f));
        pencil.lineTo (point (0.70f, 0.12f));
        pencil.lineTo (point (0.88f, 0.30f));
        pencil.lineTo (point (0.34f, 0.84f));
        pencil.closeSubPath();
        pencil.startNewSubPath (point (0.58f, 0.24f));
        pencil.lineTo (point (0.76f, 0.42f));

        g.setColour (isButtonDown ? findColour (AppColourIds::signalPath) : findColour (Label::textColourId).withAlpha (0.7f));
        g.strokePath (pencil, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
};
