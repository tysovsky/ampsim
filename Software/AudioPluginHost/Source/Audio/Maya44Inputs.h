#pragma once

#include <optional>

//==============================================================================
/**
    Switches the analog inputs of an ESI MAYA44 eX card, the way ESI's own control
    panel (MAYA44eXPan.exe) does, so they can be set from the host's settings.

    Inputs 1/2 take a line, a mic, or a mic that needs 48V phantom power. Inputs 3/4
    take a line or an instrument (Hi-Z). The card's control driver (MAYA44eXDrv.sys)
    keeps all of the card's settings in one 540-byte block: a mode is read from that
    block, and set by sending the block back with the mode changed. The driver only
    touches the hardware for what changed (muting the input while it switches), and
    saves the block, so the modes are still set after the computer restarts.

    ESI's control panel doesn't notice changes made here while it's running: it shows
    the old mode until it's reopened, and changing anything in it puts its old mode back.

    Windows only. Elsewhere, or without the card, isAvailable() is false and nothing is done.
*/
namespace Maya44Inputs
{
    /** The modes of inputs 1/2. Digital is the S/PDIF input taking their place, which
        ESI's panel only allows while a digital signal is connected. */
    enum class Input12Mode { line = 0, mic = 1, micPhantom = 2, digital = 3 };

    /** The modes of inputs 3/4. */
    enum class Input34Mode { line = 0, hiZ = 1 };

    struct Modes
    {
        Input12Mode input12 = Input12Mode::line;
        Input34Mode input34 = Input34Mode::line;
    };

    /** Whether a MAYA44 eX card is installed and its control driver can be reached. */
    bool isAvailable();

    /** The modes the inputs are in, or nothing if the card can't be reached or reports
        a mode this doesn't know. */
    std::optional<Modes> read();

    /** Sets inputs 1/2 or 3/4 to a mode. Returns false if the card can't be reached. */
    bool setInput12Mode (Input12Mode mode);
    bool setInput34Mode (Input34Mode mode);
}
