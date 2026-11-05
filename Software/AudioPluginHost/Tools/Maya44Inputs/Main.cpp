/*
    Maya44Inputs: reads or sets the modes of an ESI MAYA44 eX card's inputs, through the
    same code the host's settings use (Source/Audio/Maya44Inputs.cpp), to try it on the card.

    Usage: Maya44Inputs                     prints the modes
           Maya44Inputs 12 line|mic|48v     sets inputs 1/2
           Maya44Inputs 34 line|hiz         sets inputs 3/4
*/

#include "../../Source/Audio/Maya44Inputs.h"
#include <cstring>
#include <iostream>

using namespace Maya44Inputs;

static const char* getName (Input12Mode mode)
{
    switch (mode)
    {
        case Input12Mode::line:         return "Line";
        case Input12Mode::mic:          return "Mic";
        case Input12Mode::micPhantom:   return "Mic + 48V";
        case Input12Mode::digital:      return "Digital (S/PDIF)";
    }

    return "?";
}

static const char* getName (Input34Mode mode)
{
    return mode == Input34Mode::hiZ ? "Hi-Z" : "Line";
}

static int printModes()
{
    auto modes = read();

    if (! modes.has_value())
    {
        std::cerr << "Couldn't read the modes from the card\n";
        return 1;
    }

    std::cout << "Inputs 1/2: " << getName (modes->input12) << "\n"
              << "Inputs 3/4: " << getName (modes->input34) << "\n";
    return 0;
}

static int printUsage()
{
    std::cerr << "usage: Maya44Inputs [12 line|mic|48v] [34 line|hiz]\n";
    return 1;
}

int main (int argc, char* argv[])
{
    if (! isAvailable())
    {
        std::cerr << "No MAYA44 eX card found\n";
        return 1;
    }

    if (argc == 1)
        return printModes();

    if (argc != 3)
        return printUsage();

    const auto is = [] (const char* arg, const char* word) { return std::strcmp (arg, word) == 0; };
    bool ok = false;

    if (is (argv[1], "12"))
    {
        if      (is (argv[2], "line"))  ok = setInput12Mode (Input12Mode::line);
        else if (is (argv[2], "mic"))   ok = setInput12Mode (Input12Mode::mic);
        else if (is (argv[2], "48v"))   ok = setInput12Mode (Input12Mode::micPhantom);
        else                            return printUsage();
    }
    else if (is (argv[1], "34"))
    {
        if      (is (argv[2], "line"))  ok = setInput34Mode (Input34Mode::line);
        else if (is (argv[2], "hiz"))   ok = setInput34Mode (Input34Mode::hiZ);
        else                            return printUsage();
    }
    else
    {
        return printUsage();
    }

    if (! ok)
    {
        std::cerr << "Couldn't set the mode\n";
        return 1;
    }

    return printModes();
}
