#pragma once

#include <JuceHeader.h>
#include "TextEntryPopup.h"
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    A popup covering the window with the sounds saved for a block's plugin (see
    SoundLibrary), set out across the screen. Tapping one puts the plugin back as
    it was when it was saved, and closes the popup. Save keeps the plugin as it is
    now under a name typed on an on-screen keyboard, and each of the user's own
    sounds has buttons to rename it and to delete it, which moves its file to the
    Recycle Bin.

    Where the library has the plugin's presets too, a column down the left picks
    between the user's own sounds and each group of presets, such as each type of
    sound. Otherwise, for a plugin with parameters that step through its own
    presets, two buttons step through them, for finding a sound to save.
*/
class SoundsPopup final : public Component
{
public:
    SoundsPopup();
    ~SoundsPopup() override;

    /** Opens over the whole of a component for a block's sounds, showing the group of the one it's
        set to. canBrowse shows the buttons that step through the plugin's own presets, with
        onBrowse, if the library has none of them. */
    void show (Component& parent, AudioProcessorGraph::Node::Ptr, const String& title, bool canBrowse);
    void hide();

    /** Catches up with the block, e.g. after the footswitch has picked another sound. */
    void refresh();

    /** Called to step to the plugin's previous (-1) or next (1) preset of its own. */
    std::function<void (int direction)> onBrowse;

    /** Called after the block has been set to another sound, or the one it's set to has been
        renamed or deleted, which changes one of its properties. */
    std::function<void()> onSoundChange;

    void paint (Graphics&) override;
    void resized() override;
    void parentSizeChanged() override;

private:
    class Tile;

    void showGroup (const String& group);
    void loadSound (const String& id);
    void saveSound();
    void renameSound (const String& id);
    void deleteSound (const String& id);
    void setCurrentSound (const String& id);
    void updateGroups();
    void updateTiles();
    void scrollToCurrentSound();
    Rectangle<int> getPanelBounds() const;

    TouchLookAndFeel lookAndFeel;
    AudioProcessorGraph::Node::Ptr node;    // held so the plugin outlives the popup
    bool canBrowse = false;
    String group;                           // the group being shown, or empty for the user's own

    Label titleLabel, emptyLabel;
    TextButton previousButton, nextButton, saveButton { "Save" }, closeButton { "Close" };
    Viewport groupViewport, tileViewport;
    Component groupHolder, tileHolder;
    OwnedArray<TextButton> groupButtons;
    StringArray groups;                     // what each of the group buttons shows, "" for the user's own
    OwnedArray<Tile> tiles;

    TextEntryPopup textEntry;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundsPopup)
};
