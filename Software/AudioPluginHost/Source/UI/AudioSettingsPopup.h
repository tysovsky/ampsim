#pragma once

#include <JuceHeader.h>
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    A popup covering the window for choosing the audio and MIDI devices, laid out
    for the 1280x400 touchscreen: two columns of large dropdowns and buttons.
*/
class AudioSettingsPopup final : public Component,
    private ChangeListener
{
public:
    explicit AudioSettingsPopup(AudioDeviceManager& deviceManager);
    ~AudioSettingsPopup() override;

    void show();
    void hide();

    void paint(Graphics& g) override;
    void resized() override;
    void mouseUp(const MouseEvent& e) override;

private:
    struct InputLevelMeter;

    void changeListenerCallback(ChangeBroadcaster*) override;

    void updateControls();
    void updateMidiControls();
    void layoutMidiInputButtons(Rectangle<int> area);
    Rectangle<int> getPanelBounds() const;

    void deviceTypeChosen();
    void deviceChosen(bool isInput);
    void channelsChosen(bool isInput);
    void applySetup(const AudioDeviceManager::AudioDeviceSetup& setup);

    AudioDeviceManager& deviceManager;
    TouchLookAndFeel lookAndFeel;

    Label titleLabel;
    TextButton doneButton{ "Done" };

    Label deviceTypeLabel, outputLabel, inputLabel, sampleRateLabel, bufferSizeLabel,
          outputChannelsLabel, inputChannelsLabel, midiInputsLabel, midiOutputLabel, noMidiInputsLabel;

    ComboBox deviceTypeBox, outputDeviceBox, inputDeviceBox, sampleRateBox, bufferSizeBox,
             outputChannelsBox, inputChannelsBox, midiOutputBox;

    TextButton testButton{ "Test" }, controlPanelButton{ "Driver settings" };
    std::unique_ptr<InputLevelMeter> inputLevelMeter;
    OwnedArray<TextButton> midiInputButtons;

    StringArray outputNames, inputNames;
    Array<MidiDeviceInfo> midiInputs, midiOutputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPopup)
};
