/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 7 End-User License
   Agreement and JUCE Privacy Policy.

   End User License Agreement: www.juce.com/juce-7-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#pragma once

#include "PluginGraph.h"


//==============================================================================
/**
    Manages the internal plugin types.
*/
class InternalPluginFormat final : public AudioPluginFormat
{
public:
    //==============================================================================
    InternalPluginFormat();

    //==============================================================================
    const std::vector<PluginDescription>& getAllTypes() const;

    //==============================================================================
    static String getIdentifier()                                                       { return "Internal"; }
    String getName() const override                                                     { return getIdentifier(); }
    bool fileMightContainThisPluginType (const String&) override                        { return true; }
    FileSearchPath getDefaultLocationsToSearch() override                               { return {}; }
    bool canScanForPlugins() const override                                             { return false; }
    bool isTrivialToScan() const override                                               { return true; }
    void findAllTypesForFile (OwnedArray<PluginDescription>&, const String&) override   {}
    bool doesPluginStillExist (const PluginDescription&) override                       { return true; }
    String getNameOfPluginFromIdentifier (const String& fileOrIdentifier) override      { return fileOrIdentifier; }
    bool pluginNeedsRescanning (const PluginDescription&) override                      { return false; }
    StringArray searchPathsForPlugins (const FileSearchPath&, bool, bool) override      { return {}; }

private:
    class InternalPluginFactory
    {
    public:
        using Constructor = std::function<std::unique_ptr<AudioPluginInstance>()>;

        explicit InternalPluginFactory (const std::initializer_list<Constructor>& constructorsIn);

        const std::vector<PluginDescription>& getDescriptions() const       { return descriptions; }

        std::unique_ptr<AudioPluginInstance> createInstance (const String& name) const;

    private:
        const std::vector<Constructor> constructors;
        const std::vector<PluginDescription> descriptions;
    };

    //==============================================================================
    void createPluginInstance (const PluginDescription&,
                               double initialSampleRate, int initialBufferSize,
                               PluginCreationCallback) override;

    std::unique_ptr<AudioPluginInstance> createInstance (const String& name);

    bool requiresUnblockedMessageThreadDuringCreation (const PluginDescription&) const override;

    InternalPluginFactory factory;
};

//==============================================================================
/**
    A stereo level stage that the chain view puts straight after each plugin, so the
    plugins in a parallel column can be mixed against each other. It's an internal
    plugin so it's saved and restored with the graph, but it's hidden from the user.
*/
class LaneLevelProcessor final : public AudioPluginInstance
{
public:
    LaneLevelProcessor();

    static String getIdentifier()                                               { return "Lane Level"; }

    /** The lane's level, from silent (0) to unity gain (1). */
    AudioParameterFloat& getLevel() noexcept                                    { return *level; }

    //==============================================================================
    const String getName() const override                                       { return getIdentifier(); }
    void fillInPluginDescription (PluginDescription&) const override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override                                            {}
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    double getTailLengthSeconds() const override                                { return 0.0; }
    bool acceptsMidi() const override                                           { return false; }
    bool producesMidi() const override                                          { return false; }
    AudioProcessorEditor* createEditor() override                               { return nullptr; }
    bool hasEditor() const override                                             { return false; }

    int getNumPrograms() override                                               { return 1; }
    int getCurrentProgram() override                                            { return 0; }
    void setCurrentProgram (int) override                                       {}
    const String getProgramName (int) override                                  { return {}; }
    void changeProgramName (int, const String&) override                        {}

    void getStateInformation (MemoryBlock&) override;
    void setStateInformation (const void* data, int size) override;

private:
    AudioParameterFloat* level = nullptr;
    SmoothedValue<float> smoothedLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneLevelProcessor)
};
