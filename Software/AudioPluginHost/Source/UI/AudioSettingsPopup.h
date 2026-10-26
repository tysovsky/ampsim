#pragma once

#include <JuceHeader.h>
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    A popup covering the window for choosing the audio and MIDI devices and the
    colour theme, laid out for the 1280x400 touchscreen: two columns of large
    dropdowns and buttons. A tick box in its header turns free links between blocks
    on (see GraphEditorPanel::setLinksEnabled()).
*/
class AudioSettingsPopup final : public Component,
    private ChangeListener
{
public:
    explicit AudioSettingsPopup(AudioDeviceManager& deviceManager);
    ~AudioSettingsPopup() override;

    void show();
    void hide();

    /** Whether free links between blocks are turned on. They're off until they've been turned on. */
    static bool areFreeLinksOn();

    /** Called when free links are turned on or off. */
    std::function<void(bool)> onFreeLinksChange;

    void paint(Graphics& g) override;
    void resized() override;
    void mouseUp(const MouseEvent& e) override;

private:
    struct InputLevelMeter;
    struct CheckBox;

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
    std::unique_ptr<CheckBox> freeLinksBox;

    Label deviceTypeLabel, outputLabel, inputLabel, sampleRateLabel, bufferSizeLabel,
          outputChannelsLabel, inputChannelsLabel, midiInputsLabel, midiOutputLabel, noMidiInputsLabel, themeLabel;

    ComboBox deviceTypeBox, outputDeviceBox, inputDeviceBox, sampleRateBox, bufferSizeBox,
             outputChannelsBox, inputChannelsBox, midiOutputBox, themeBox;

    TextButton testButton{ "Test" }, controlPanelButton{ "Driver settings" };
    std::unique_ptr<InputLevelMeter> inputLevelMeter;
    OwnedArray<TextButton> midiInputButtons;

    StringArray outputNames, inputNames;
    Array<MidiDeviceInfo> midiInputs, midiOutputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsPopup)
};
