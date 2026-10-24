#include "RecordingsPopup.h"
#include "ThemeManager.h"

namespace
{
    constexpr int margin = 12;
    constexpr int padding = 16;
    constexpr int headerHeight = 52;
    constexpr int headerButtonWidth = 120;
    constexpr int rowHeight = 64;
    constexpr int rowButtonWidth = 64;
    constexpr int playButtonSize = 88;

    // e.g. "1:05", or "1:02:05" past an hour
    String formatTime (double seconds)
    {
        const auto total = jmax (0, (int) seconds);
        const auto secondsText = String (total % 60).paddedLeft ('0', 2);

        if (total >= 3600)
            return String (total / 3600) + ":" + String (total / 60 % 60).paddedLeft ('0', 2) + ":" + secondsText;

        return String (total / 60) + ":" + secondsText;
    }

    // Recordings are named "YYYY-MM-DD HH-MM-SS <output>", which is when they started
    std::optional<Time> parseStartTime (const String& name)
    {
        StringArray parts;
        parts.addTokens (name.substring (0, 19), " -", {});

        if (parts.size() != 6 || ! name.substring (0, 19).containsOnly ("0123456789- "))
            return {};

        return Time (parts[0].getIntValue(), parts[1].getIntValue() - 1, parts[2].getIntValue(),
                     parts[3].getIntValue(), parts[4].getIntValue(), parts[5].getIntValue());
    }
}

//==============================================================================
// A round button with a play triangle, or pause bars while playing
class RecordingsPopup::PlayButton final : public Button
{
public:
    PlayButton() : Button ("Play") {}

    void setPlaying (bool shouldShowPause)
    {
        if (showsPause != shouldShowPause)
        {
            showsPause = shouldShowPause;
            repaint();
        }
    }

    void paintButton (Graphics& g, bool, bool isButtonDown) override
    {
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const auto accent = findColour (AppColourIds::signalPath);

        g.setColour (isButtonDown ? accent.darker (0.2f) : accent);
        g.fillEllipse (area);

        const auto icon = area.reduced (area.getWidth() * 0.32f);
        g.setColour (Colours::white);

        if (showsPause)
        {
            const auto barWidth = icon.getWidth() * 0.32f;
            g.fillRoundedRectangle (icon.withWidth (barWidth), 2.0f);
            g.fillRoundedRectangle (icon.withLeft (icon.getRight() - barWidth), 2.0f);
        }
        else
        {
            // Nudged right, so it looks centred
            Path triangle;
            triangle.addTriangle (icon.getX() + icon.getWidth() * 0.12f, icon.getY(),
                                  icon.getX() + icon.getWidth() * 0.12f, icon.getBottom(),
                                  icon.getRight() + icon.getWidth() * 0.12f, icon.getCentreY());
            g.fillPath (triangle);
        }
    }

private:
    bool showsPause = false;
};

//==============================================================================
// How far through the recording is, which can be tapped or dragged along to go to that point
class RecordingsPopup::SeekBar final : public Component
{
public:
    std::function<void (double)> onSeek;    // with the proportion through, from 0 to 1

    void setProportion (double newProportion)
    {
        if (! isDragging && proportion != newProportion)
        {
            proportion = newProportion;
            repaint();
        }
    }

    void paint (Graphics& g) override
    {
        const auto track = getTrackArea();
        const auto accent = findColour (AppColourIds::signalPath);
        const auto x = track.getX() + track.getWidth() * (float) proportion;

        g.setColour (findColour (ComboBox::outlineColourId));
        g.fillRoundedRectangle (track, track.getHeight() / 2.0f);

        g.setColour (accent);
        g.fillRoundedRectangle (track.withRight (x), track.getHeight() / 2.0f);
        g.fillEllipse (Rectangle<float> (24.0f, 24.0f).withCentre ({ x, track.getCentreY() }));
    }

    void mouseDown (const MouseEvent& e) override
    {
        isDragging = true;
        seekTo (e.position.x);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        seekTo (e.position.x);
    }

    void mouseUp (const MouseEvent&) override
    {
        isDragging = false;
    }

private:
    // Inset by the thumb's radius, so it fits at either end
    Rectangle<float> getTrackArea() const
    {
        return getLocalBounds().toFloat().reduced (12.0f, 0.0f).withSizeKeepingCentre (getWidth() - 24.0f, 8.0f);
    }

    void seekTo (float x)
    {
        const auto track = getTrackArea();
        proportion = jlimit (0.0, 1.0, (double) ((x - track.getX()) / track.getWidth()));
        repaint();

        if (onSeek != nullptr)
            onSeek (proportion);
    }

    double proportion = 0.0;
    bool isDragging = false;
};

//==============================================================================
// A bin, for deleting a recording
class RecordingsPopup::DeleteButton final : public Button
{
public:
    DeleteButton() : Button ("Delete")
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (Graphics& g, bool, bool isButtonDown) override
    {
        const auto size = jmin (24.0f, (float) jmin (getWidth(), getHeight()));
        const auto box = getLocalBounds().toFloat().withSizeKeepingCentre (size, size);
        const auto point = [&] (float x, float y) { return box.getRelativePoint (x, y); };

        Path bin;
        bin.startNewSubPath (point (0.08f, 0.22f));
        bin.lineTo (point (0.92f, 0.22f));
        bin.startNewSubPath (point (0.36f, 0.22f));
        bin.lineTo (point (0.40f, 0.06f));
        bin.lineTo (point (0.60f, 0.06f));
        bin.lineTo (point (0.64f, 0.22f));
        bin.startNewSubPath (point (0.18f, 0.22f));
        bin.lineTo (point (0.24f, 0.94f));
        bin.lineTo (point (0.76f, 0.94f));
        bin.lineTo (point (0.82f, 0.22f));
        bin.startNewSubPath (point (0.42f, 0.38f));
        bin.lineTo (point (0.42f, 0.78f));
        bin.startNewSubPath (point (0.58f, 0.38f));
        bin.lineTo (point (0.58f, 0.78f));

        g.setColour (isButtonDown ? Colour (0xffe0342c) : findColour (Label::textColourId).withAlpha (0.7f));
        g.strokePath (bin, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
};

//==============================================================================
// A pencil, for renaming a recording
class RecordingsPopup::RenameButton final : public Button
{
public:
    RenameButton() : Button ("Rename")
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

//==============================================================================
// What goes on top of each row of the list: its rename and delete buttons, at the right-hand end
class RecordingsPopup::RowControls final : public Component
{
public:
    explicit RowControls (RecordingsPopup& p) : popup (p)
    {
        // Taps anywhere else in the row go through to the list, which opens the recording
        setInterceptsMouseClicks (false, true);

        renameButton.onClick = [this] { popup.renameRecording (file, name); };
        deleteButton.onClick = [this] { popup.deleteRecording (file); };
        addAndMakeVisible (renameButton);
        addAndMakeVisible (deleteButton);
    }

    void setRecording (const Recording& recording)
    {
        file = recording.file;
        name = recording.name;
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (0, 4);
        deleteButton.setBounds (area.removeFromRight (rowButtonWidth));
        renameButton.setBounds (area.removeFromRight (rowButtonWidth));
    }

private:
    RecordingsPopup& popup;
    File file;
    String name;
    RenameButton renameButton;
    DeleteButton deleteButton;
};

//==============================================================================
RecordingsPopup::RecordingsPopup (Recorder& r)
    : recorder (r), playButton (std::make_unique<PlayButton>()), seekBar (std::make_unique<SeekBar>())
{
    setLookAndFeel (&lookAndFeel);
    formats.registerBasicFormats();

    titleLabel.setText ("Recordings", dontSendNotification);
    titleLabel.setFont (Font (24.0f, Font::bold));
    addAndMakeVisible (titleLabel);

    emptyLabel.setText ("No recordings yet. Tap Record to make one.", dontSendNotification);
    emptyLabel.setFont (Font (TouchLookAndFeel::fontSize));
    emptyLabel.setJustificationType (Justification::centred);
    addChildComponent (emptyLabel);

    backButton.setButtonText (String (CharPointer_UTF8 ("\xe2\x80\xb9  Back")));
    backButton.onClick = [this] { closePlayer(); };
    addChildComponent (backButton);

    closeButton.onClick = [this] { hide(); };
    addAndMakeVisible (closeButton);

    for (auto* button : { &backButton, &closeButton })
        button->setWantsKeyboardFocus (false);

    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setColour (ListBox::backgroundColourId, Colours::transparentBlack);
    list.getViewport()->setScrollOnDragMode (Viewport::ScrollOnDragMode::all);
    list.getViewport()->setScrollBarThickness (8);
    addAndMakeVisible (list);

    // The player, shown instead of the list
    recordingLabel.setFont (Font (24.0f, Font::bold));
    detailsLabel.setFont (Font (16.0f));
    positionLabel.setFont (Font (16.0f));
    lengthLabel.setFont (Font (16.0f));
    lengthLabel.setJustificationType (Justification::centredRight);

    for (auto* label : { &recordingLabel, &detailsLabel, &positionLabel, &lengthLabel })
        addChildComponent (label);

    playButton->setWantsKeyboardFocus (false);
    playButton->onClick = [this] { togglePlayback(); };
    addChildComponent (playButton.get());

    seekBar->onSeek = [this] (double proportion)
    {
        auto& playback = recorder.getPlayback();
        playback.setPosition (proportion * playback.getLengthInSeconds());
        updatePlayer();
    };
    addChildComponent (seekBar.get());
}

RecordingsPopup::~RecordingsPopup()
{
    recorder.unloadPlayback();
    setLookAndFeel (nullptr);
}

//==============================================================================
void RecordingsPopup::show()
{
    closePlayer();
    findRecordings();
    list.updateContent();
    list.getViewport()->setViewPosition (0, 0);
    emptyLabel.setVisible (recordings.empty());

    setVisible (true);
    toFront (true);
}

void RecordingsPopup::hide()
{
    textEntry.hide();
    closePlayer();
    setVisible (false);
}

void RecordingsPopup::findRecordings()
{
    recordings.clear();

    for (auto& file : Recorder::getRecordingsFolder().findChildFiles (File::findFiles, false, "*.wav"))
    {
        Recording recording;
        recording.file = file;

        const auto name = file.getFileNameWithoutExtension();
        const auto startTime = parseStartTime (name);
        recording.startTime = startTime.value_or (file.getCreationTime());
        recording.name = startTime.has_value() ? name.substring (20) : name;

        if (std::unique_ptr<AudioFormatReader> reader { formats.createReaderFor (file) })
            recording.length = reader->sampleRate > 0.0 ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;

        recordings.push_back (recording);
    }

    // Newest first, and a take's outputs in order
    std::sort (recordings.begin(), recordings.end(), [] (const Recording& a, const Recording& b)
    {
        if (a.startTime != b.startTime)
            return a.startTime > b.startTime;

        return a.name.compareNatural (b.name) < 0;
    });
}

//==============================================================================
int RecordingsPopup::getNumRows()
{
    return (int) recordings.size();
}

void RecordingsPopup::paintListBoxItem (int row, Graphics& g, int width, int height, bool)
{
    if (! isPositiveAndBelow (row, (int) recordings.size()))
        return;

    const auto& recording = recordings[(size_t) row];
    auto area = Rectangle<int> (width, height).reduced (0, 4);

    g.setColour (findColour (TextEditor::backgroundColourId));
    g.fillRoundedRectangle (area.toFloat(), 8.0f);

    // Room for the rename and delete buttons on top, at the right-hand end
    area.removeFromRight (2 * rowButtonWidth);
    area.reduce (16, 6);
    const auto textColour = findColour (Label::textColourId);

    g.setColour (textColour);
    g.setFont (Font (20.0f));
    g.drawText (formatTime (recording.length), area.removeFromRight (100), Justification::centredRight);

    g.setFont (Font (18.0f, Font::bold));
    g.drawText (recording.name, area.removeFromTop (area.getHeight() / 2), Justification::bottomLeft);

    g.setColour (textColour.withAlpha (0.6f));
    g.setFont (Font (14.0f));
    g.drawText (recording.startTime.formatted ("%d %b %Y  %H:%M:%S"), area, Justification::topLeft);
}

void RecordingsPopup::listBoxItemClicked (int row, const MouseEvent&)
{
    if (isPositiveAndBelow (row, (int) recordings.size()))
        openPlayer (recordings[(size_t) row]);
}

Component* RecordingsPopup::refreshComponentForRow (int row, bool, Component* existingComponentToUpdate)
{
    std::unique_ptr<Component> existing (existingComponentToUpdate);

    if (! isPositiveAndBelow (row, (int) recordings.size()))
        return nullptr;

    std::unique_ptr<RowControls> controls (dynamic_cast<RowControls*> (existing.release()));

    if (controls == nullptr)
        controls = std::make_unique<RowControls> (*this);

    controls->setRecording (recordings[(size_t) row]);
    return controls.release();
}

void RecordingsPopup::deleteRecording (const File& file)
{
    // Later, as the list gets rid of rows it doesn't need any more, which could be the one with the
    // button that's just been tapped
    MessageManager::callAsync ([safeThis = SafePointer<RecordingsPopup> (this), file]
    {
        // To the Recycle Bin, so one that's deleted by mistake can be got back
        if (! file.moveToTrash())
            file.deleteFile();

        if (safeThis != nullptr)
            safeThis->refreshList();
    });
}

void RecordingsPopup::renameRecording (const File& file, const String& currentName)
{
    textEntry.show (*this, "Rename recording", currentName, [safeThis = SafePointer<RecordingsPopup> (this), file] (const String& newName)
    {
        const auto name = File::createLegalFileName (newName.trim());

        if (safeThis == nullptr || name.isEmpty())
            return;

        // The time it was started stays at the front of the file's name, which is how the list
        // knows when it was made
        const auto oldName = file.getFileNameWithoutExtension();
        const auto startTime = parseStartTime (oldName).has_value() ? oldName.substring (0, 20) : String();
        auto renamed = file.getSiblingFile (startTime + name + file.getFileExtension());

        // A different recording with the name already gets a number on the end
        if (renamed.exists() && ! renamed.getFullPathName().equalsIgnoreCase (file.getFullPathName()))
            renamed = renamed.getNonexistentSibling();

        if (renamed != file)
            file.moveFileTo (renamed);

        safeThis->refreshList();
    });
}

void RecordingsPopup::refreshList()
{
    findRecordings();
    list.updateContent();
    list.repaint();
    emptyLabel.setVisible (! playerOpen && recordings.empty());
}

//==============================================================================
void RecordingsPopup::openPlayer (const Recording& recording)
{
    if (! recorder.loadForPlayback (recording.file))
        return;

    playerOpen = true;
    recordingLabel.setText (recording.name, dontSendNotification);
    detailsLabel.setText (recording.startTime.formatted ("%d %b %Y  %H:%M:%S"), dontSendNotification);

    list.setVisible (false);
    emptyLabel.setVisible (false);

    for (auto* part : getPlayerParts())
        part->setVisible (true);

    updatePlayer();
    startTimerHz (30);
    resized();
}

void RecordingsPopup::closePlayer()
{
    stopTimer();
    recorder.unloadPlayback();
    playerOpen = false;

    for (auto* part : getPlayerParts())
        part->setVisible (false);

    list.setVisible (true);
    emptyLabel.setVisible (recordings.empty());
    resized();
}

std::vector<Component*> RecordingsPopup::getPlayerParts()
{
    return { &backButton, &recordingLabel, &detailsLabel, &positionLabel, &lengthLabel, playButton.get(), seekBar.get() };
}

void RecordingsPopup::togglePlayback()
{
    auto& playback = recorder.getPlayback();

    if (playback.isPlaying())
    {
        playback.stop();
    }
    else
    {
        // From the start again after playing to the end
        if (playback.getCurrentPosition() >= playback.getLengthInSeconds() - 0.05)
            playback.setPosition (0.0);

        playback.start();
    }

    updatePlayer();
}

void RecordingsPopup::updatePlayer()
{
    auto& playback = recorder.getPlayback();
    const auto length = playback.getLengthInSeconds();
    const auto position = jmin (playback.getCurrentPosition(), length);

    playButton->setPlaying (playback.isPlaying());
    seekBar->setProportion (length > 0.0 ? position / length : 0.0);
    positionLabel.setText (formatTime (position), dontSendNotification);
    lengthLabel.setText (formatTime (length), dontSendNotification);
}

void RecordingsPopup::timerCallback()
{
    updatePlayer();
}

//==============================================================================
void RecordingsPopup::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.6f));

    const auto panel = getPanelBounds().toFloat();

    g.setColour (findColour (ResizableWindow::backgroundColourId));
    g.fillRoundedRectangle (panel, 10.0f);

    g.setColour (findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (panel, 10.0f, 1.5f);
    g.fillRect (panel.getX() + padding, panel.getY() + headerHeight, panel.getWidth() - 2 * padding, 1.0f);
}

void RecordingsPopup::resized()
{
    auto panel = getPanelBounds();
    auto header = panel.removeFromTop (headerHeight).reduced (padding, 6);

    closeButton.setBounds (header.removeFromRight (headerButtonWidth));

    if (backButton.isVisible())
    {
        backButton.setBounds (header.removeFromLeft (headerButtonWidth));
        header.removeFromLeft (padding);
    }

    titleLabel.setBounds (header);

    auto content = panel.reduced (padding, 10);
    list.setBounds (content);
    emptyLabel.setBounds (content);

    // The player: what's playing along the top, then the play button and the bar to move through it
    content.removeFromTop (8);
    recordingLabel.setBounds (content.removeFromTop (32));
    detailsLabel.setBounds (content.removeFromTop (24));

    auto controls = content.withSizeKeepingCentre (content.getWidth(), playButtonSize);
    playButton->setBounds (controls.removeFromLeft (playButtonSize));
    controls.removeFromLeft (24);

    // The bar level with the middle of the play button, with the times under its ends
    const auto bar = controls.withSizeKeepingCentre (controls.getWidth(), 44);
    auto times = Rectangle<int> (bar.getX(), bar.getBottom(), bar.getWidth(), 24);

    seekBar->setBounds (bar);
    positionLabel.setBounds (times.removeFromLeft (times.getWidth() / 2));
    lengthLabel.setBounds (times);
}

void RecordingsPopup::mouseUp (const MouseEvent& e)
{
    // Tapping outside the panel closes the popup
    if (e.mouseWasClicked() && ! getPanelBounds().contains (e.getPosition()))
        hide();
}

Rectangle<int> RecordingsPopup::getPanelBounds() const
{
    return getLocalBounds().reduced (margin);
}
