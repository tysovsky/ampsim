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

    /** How many of the last samples in it keeps, for drawing the sound. */
    static constexpr int recentInputSize = 2048;

    /** Copies the last samples to come in (mixed to mono, before the level is applied), up to
        recentInputSize of them, oldest first. It can be called while the audio runs, from another
        thread; a copy may take in a few newer samples, which doesn't matter for drawing. */
    void copyRecentInput (float* destination, int numSamples) const noexcept;

    double getRecentInputSampleRate() const noexcept                            { return recentInputSampleRate.load(); }

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

    std::array<std::atomic<float>, recentInputSize> recentInput {};
    std::atomic<int> recentInputEnd { 0 };          // where the next sample goes
    std::atomic<double> recentInputSampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneLevelProcessor)
};

//==============================================================================
/**
    The two ends of a chain in the chain view. A ChainInputProcessor takes the
    chain's signal from one of the audio device's input channels, or a pair of them,
    and a ChainOutputProcessor sends it to one of the output channels or a pair.

    Each has all the device's channels on its device side (up to maxChannels), so
    picking another channel is a change to a parameter rather than to the graph's
    connections. The chain side is stereo: a single input channel feeds both sides,
    and a single output channel gets the two sides mixed.
*/
class ChainEndProcessor : public AudioPluginInstance,
                          private AudioProcessorParameter::Listener
{
public:
    static constexpr int maxChannels = 8;

    /** Which channel, or pair: "1" to "8", then "1+2" to "7+8". */
    AudioParameterChoice& getChannel() noexcept                                 { return *channel; }

    /** The first device channel chosen, counting from 0, and the second of a pair, or -1. */
    std::pair<int, int> getChosenChannels() const noexcept;

    /** The choice for a channel, or for the pair it starts (an even channel), counting from 0. */
    static int getChoiceFor (int firstChannel, bool isPair) noexcept    { return isPair ? maxChannels + firstChannel / 2 : firstChannel; }

    /** What it's called: the name it's been given, or else its channel. */
    String getBlockName() const;

    /** Its channel, e.g. "Input 1" or "Output 1+2". */
    String getChannelName() const;

    /** A name of its own, e.g. "Guitar", or empty to go by its channel. It's saved with the
        channel, and changing it tells the graph, like changing the channel does.
    */
    const String& getCustomName() const noexcept                                { return customName; }
    void setCustomName (const String&);

    //==============================================================================
    void fillInPluginDescription (PluginDescription&) const override;
    void releaseResources() override                                            {}

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

protected:
    ChainEndProcessor (const BusesProperties&, const String& blockNamePrefix, int defaultChoice);

private:
    void parameterValueChanged (int, float) override;
    void parameterGestureChanged (int, bool) override                           {}

    AudioParameterChoice* channel = nullptr;
    const String blockNamePrefix;
    String customName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainEndProcessor)
};

class ChainInputProcessor final : public ChainEndProcessor
{
public:
    ChainInputProcessor();

    static String getIdentifier()                                               { return "Chain Input"; }
    const String getName() const override                                       { return getIdentifier(); }
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void prepareToPlay (double, int) override                                   {}
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;
};

class ChainOutputProcessor final : public ChainEndProcessor
{
public:
    ChainOutputProcessor();

    static String getIdentifier()                                               { return "Chain Output"; }
    const String getName() const override                                       { return getIdentifier(); }
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    void prepareToPlay (double, int maximumExpectedSamplesPerBlock) override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

private:
    AudioBuffer<float> chainSignal;
};
