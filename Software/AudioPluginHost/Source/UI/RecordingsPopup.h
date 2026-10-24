#pragma once

#include <JuceHeader.h>
#include "../Audio/Recorder.h"
#include "TextEntryPopup.h"
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    A popup covering the window with the recordings made so far, newest first.
    Tapping one opens a player for it, with a play/pause button and a bar along
    which it can be moved to any point. It plays on the first two outputs. Each
    has buttons to rename it, on an on-screen keyboard, and to delete it, which
    moves the file to the Recycle Bin.
*/
class RecordingsPopup final : public Component,
                              private ListBoxModel,
                              private Timer
{
public:
    explicit RecordingsPopup (Recorder&);
    ~RecordingsPopup() override;

    void show();
    void hide();

    void paint (Graphics&) override;
    void resized() override;
    void mouseUp (const MouseEvent&) override;

private:
    class PlayButton;
    class SeekBar;
    class DeleteButton;
    class RenameButton;
    class RowControls;

    struct Recording
    {
        File file;
        String name;        // which output it's of, e.g. "Output 1+2"
        Time startTime;
        double length = 0.0;
    };

    int getNumRows() override;
    void paintListBoxItem (int row, Graphics&, int width, int height, bool isSelected) override;
    void listBoxItemClicked (int row, const MouseEvent&) override;
    Component* refreshComponentForRow (int row, bool isSelected, Component* existingComponentToUpdate) override;

    void timerCallback() override;

    void findRecordings();
    void deleteRecording (const File&);
    void renameRecording (const File&, const String& currentName);
    void refreshList();
    void openPlayer (const Recording&);
    void closePlayer();
    std::vector<Component*> getPlayerParts();
    void togglePlayback();
    void updatePlayer();
    Rectangle<int> getPanelBounds() const;

    Recorder& recorder;
    TouchLookAndFeel lookAndFeel;
    AudioFormatManager formats;

    Label titleLabel, emptyLabel, recordingLabel, detailsLabel, positionLabel, lengthLabel;
    TextButton backButton, closeButton { "Close" };
    ListBox list;
    std::unique_ptr<PlayButton> playButton;
    std::unique_ptr<SeekBar> seekBar;

    TextEntryPopup textEntry;
    std::vector<Recording> recordings;
    bool playerOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordingsPopup)
};
