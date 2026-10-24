#pragma once

#include <JuceHeader.h>

//==============================================================================
/** What a block in the chain does, shown as an icon on its tile and in the plugin picker. */
enum class BlockIcon
{
    plugin,     // a plugin without a definition file
    suite,      // a whole plugin that's also split into modules
    amp,
    pedal,
    cab,
    eq,
    dynamics,   // compressors, gates, de-essers, limiters
    pitch,      // pitch correction and harmonies
    effect,     // studio effects such as vocal delays and reverbs
    input,
    output
};

/** The icon for a name used in definition files ("amp", "pedal", "cab", "eq", "dynamics", "pitch"
    or "effect"); anything else is a plain plugin.
*/
BlockIcon blockIconFromName (const String& name);

/** The picker's category for blocks with this icon, e.g. "Amps". */
String getBlockIconCategory (BlockIcon);

/** Draws an icon as line art, fitted into the centre of an area. */
void drawBlockIcon (Graphics&, BlockIcon, Rectangle<float> area, Colour);
