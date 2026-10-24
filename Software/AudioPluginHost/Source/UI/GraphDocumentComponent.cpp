#include "GraphDocumentComponent.h"
#include "../Plugins/InternalPlugins.h"
#include "MainHostWindow.h"
#include "ThemeManager.h"

//==============================================================================
struct GraphDocumentComponent::TooltipBar final : public Component,
                                                  private Timer
{
    TooltipBar()
    {
        startTimer(100);
    }

    void paint(Graphics& g) override
    {
        g.setFont(Font((float)getHeight() * 0.7f, Font::bold));
        g.setColour(Colours::black);
        g.drawFittedText(tip, 10, 0, getWidth() - 12, getHeight(), Justification::centredLeft, 1);
    }

    void timerCallback() override
    {
        String newTip;

        if (auto* underMouse = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (auto* ttc = dynamic_cast<TooltipClient*> (underMouse))
                if (!(underMouse->isMouseButtonDown() || underMouse->isCurrentlyBlockedByAnotherModalComponent()))
                    newTip = ttc->getTooltip();

        if (newTip != tip)
        {
            tip = newTip;
            repaint();
        }
    }

    String tip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TooltipBar)
};

//==============================================================================
// A red dot to start recording; while recording, a square to stop, and how long it's been going
class RecordButton final : public Button,
                           private Timer
{
public:
    explicit RecordButton(Recorder& r) : Button("Record"), recorder(r)
    {
        setWantsKeyboardFocus(false);
        startTimerHz(4);
    }

    void paintButton(Graphics& g, bool, bool isButtonDown) override
    {
        const bool isRecording = recorder.isRecording();
        const auto red = Colour(0xffe0342c);
        auto area = getLocalBounds().toFloat();

        g.setColour(isRecording ? red : findColour(TextButton::buttonColourId).darker(isButtonDown ? 0.1f : 0.0f));
        g.fillRoundedRectangle(area, 6.0f);

        auto icon = area.reduced(12.0f, 0.0f).removeFromLeft(18.0f).withSizeKeepingCentre(18.0f, 18.0f);
        auto textArea = area.withTrimmedLeft(icon.getRight() + 8.0f - area.getX()).withTrimmedRight(8.0f);

        g.setColour(isRecording ? Colours::white : red);

        if (isRecording)
            g.fillRoundedRectangle(icon.reduced(2.0f), 2.0f);
        else
            g.fillEllipse(icon);

        const auto seconds = (int) recorder.getRecordingLength();

        g.setColour(isRecording ? Colours::white : findColour(TextButton::textColourOffId));
        g.setFont(Font(18.0f, isRecording ? Font::bold : Font::plain));
        g.drawText(isRecording ? String(seconds / 60) + ":" + String(seconds % 60).paddedLeft('0', 2) : String("Record"),
                   textArea, Justification::centredLeft);
    }

private:
    void timerCallback() override
    {
        if (recorder.isRecording() || wasRecording)
            repaint();

        wasRecording = recorder.isRecording();
    }

    Recorder& recorder;
    bool wasRecording = false;
};

//==============================================================================
// This is the title shown on top
class GraphDocumentComponent::TitleBarComponent final : public Component,
    private Button::Listener
{
public:
    explicit TitleBarComponent(GraphDocumentComponent& graphDocumentComponent)
        : owner(graphDocumentComponent)
    {
        static const unsigned char burgerMenuPathData[]
            = { 110,109,0,0,128,64,0,0,32,65,108,0,0,224,65,0,0,32,65,98,254,212,232,65,0,0,32,65,0,0,240,65,252,
                169,17,65,0,0,240,65,0,0,0,65,98,0,0,240,65,8,172,220,64,254,212,232,65,0,0,192,64,0,0,224,65,0,0,
                192,64,108,0,0,128,64,0,0,192,64,98,16,88,57,64,0,0,192,64,0,0,0,64,8,172,220,64,0,0,0,64,0,0,0,65,
                98,0,0,0,64,252,169,17,65,16,88,57,64,0,0,32,65,0,0,128,64,0,0,32,65,99,109,0,0,224,65,0,0,96,65,108,
                0,0,128,64,0,0,96,65,98,16,88,57,64,0,0,96,65,0,0,0,64,4,86,110,65,0,0,0,64,0,0,128,65,98,0,0,0,64,
                254,212,136,65,16,88,57,64,0,0,144,65,0,0,128,64,0,0,144,65,108,0,0,224,65,0,0,144,65,98,254,212,232,
                65,0,0,144,65,0,0,240,65,254,212,136,65,0,0,240,65,0,0,128,65,98,0,0,240,65,4,86,110,65,254,212,232,
                65,0,0,96,65,0,0,224,65,0,0,96,65,99,109,0,0,224,65,0,0,176,65,108,0,0,128,64,0,0,176,65,98,16,88,57,
                64,0,0,176,65,0,0,0,64,2,43,183,65,0,0,0,64,0,0,192,65,98,0,0,0,64,254,212,200,65,16,88,57,64,0,0,208,
                65,0,0,128,64,0,0,208,65,108,0,0,224,65,0,0,208,65,98,254,212,232,65,0,0,208,65,0,0,240,65,254,212,
                200,65,0,0,240,65,0,0,192,65,98,0,0,240,65,2,43,183,65,254,212,232,65,0,0,176,65,0,0,224,65,0,0,176,
                65,99,101,0,0 };

        {
            Path p;
            p.loadPathFromData(burgerMenuPathData, sizeof(burgerMenuPathData));
            burgerButton.setShape(p, true, true, false);
        }

        {
            // A play triangle and a list, for the recordings
            Path p;
            p.addTriangle(0.0f, 0.0f, 0.0f, 10.0f, 8.0f, 5.0f);
            p.addRectangle(12.0f, 3.5f, 20.0f, 3.0f);
            p.addRectangle(0.0f, 14.0f, 32.0f, 3.0f);
            p.addRectangle(0.0f, 24.0f, 32.0f, 3.0f);
            recordingsButton.setShape(p, true, true, false);
        }

        burgerButton.addListener(this);
        addAndMakeVisible(burgerButton);

        // Another chain, with its own input and output, for another instrument or mic
        addChainButton.setWantsKeyboardFocus(false);
        addChainButton.onClick = [this] { owner.graphPanel->addChain(); };
        addAndMakeVisible(addChainButton);

        recordButton.onClick = [this] { owner.toggleRecording(); };
        addAndMakeVisible(recordButton);

        recordingsButton.addListener(this);
        addAndMakeVisible(recordingsButton);

        titleLabel.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        setOpaque(true);
        lookAndFeelChanged();
    }

private:
    void paint(Graphics& g) override
    {
        g.setColour(findColour(AppColourIds::titleBarBackground));
        g.fillRect(getLocalBounds());
    }

    void lookAndFeelChanged() override
    {
        const auto icon = findColour(AppColourIds::titleBarIcon);
        const auto pressed = findColour(AppColourIds::titleBarIconPressed);

        burgerButton.setColours(icon, icon, pressed);
        recordingsButton.setColours(icon, icon, pressed);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int h = getHeight();

        // The buttons span the full bar height so they're easy to hit on the touchscreen,
        // and the border keeps the icons themselves at a sensible size
        burgerButton.setBounds(r.removeFromLeft(h + 16));
        burgerButton.setBorderSize(BorderSize<int>(h / 4));

        recordingsButton.setBounds(r.removeFromRight(h + 16));
        recordingsButton.setBorderSize(BorderSize<int>(h / 4));

        recordButton.setBounds(r.removeFromRight(140).reduced(0, 4));

        addChainButton.setBounds(r.withSizeKeepingCentre(160, h - 8));

        titleLabel.setFont(Font(static_cast<float> (h) * 0.5f, Font::plain));
        titleLabel.setBounds(r.withRight(addChainButton.getX()));
    }

    void buttonClicked(Button* b) override
    {
        if (b == &burgerButton) {
            owner.showSettings();
        }
        else {
            owner.showRecordings();
        }
    }

    GraphDocumentComponent& owner;

    Label titleLabel{ "titleLabel", "Studio" };
    ShapeButton burgerButton{ "burgerButton", Colours::lightgrey, Colours::lightgrey, Colours::white };
    ShapeButton recordingsButton{ "recordingsButton", Colours::lightgrey, Colours::lightgrey, Colours::white };
    TextButton addChainButton{ "Add chain" };
    RecordButton recordButton{ owner.recorder };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitleBarComponent)
};

//==============================================================================
struct GraphDocumentComponent::PluginListBoxModel final : public ListBoxModel,
    public ChangeListener,
    public MouseListener
{
    PluginListBoxModel(ListBox& lb, KnownPluginList& kpl)
        : owner(lb),
        knownPlugins(kpl)
    {
        knownPlugins.addChangeListener(this);
        owner.addMouseListener(this, true);

#if JUCE_IOS
        scanner.reset(new AUScanner(knownPlugins));
#endif
    }

    int getNumRows() override
    {
        return knownPlugins.getNumTypes();
    }

    void paintListBoxItem(int rowNumber, Graphics& g,
        int width, int height, bool rowIsSelected) override
    {
        g.fillAll(owner.findColour(rowIsSelected ? AppColourIds::pluginListSelectedRow
                                                 : AppColourIds::pluginListRow));

        g.setColour(owner.findColour(rowIsSelected ? AppColourIds::pluginListSelectedText
                                                   : AppColourIds::pluginListText));

        if (rowNumber < knownPlugins.getNumTypes())
            g.drawFittedText(knownPlugins.getTypes()[rowNumber].name, { 0, 0, width, height - 2 }, Justification::centred, 1);

        g.setColour(Colours::black.withAlpha(0.4f));
        g.drawRect(0, height - 1, width, 1);
    }

    var getDragSourceDescription(const SparseSet<int>& selectedRows) override
    {
        if (!isOverSelectedRow)
            return var();

        return String("PLUGIN: " + String(selectedRows[0]));
    }

    void changeListenerCallback(ChangeBroadcaster*) override
    {
        owner.updateContent();
    }

    void mouseDown(const MouseEvent& e) override
    {
        isOverSelectedRow = owner.getRowPosition(owner.getSelectedRow(), true)
            .contains(e.getEventRelativeTo(&owner).getMouseDownPosition());
    }

    ListBox& owner;
    KnownPluginList& knownPlugins;

    bool isOverSelectedRow = false;

#if JUCE_IOS
    std::unique_ptr<AUScanner> scanner;
#endif

    JUCE_DECLARE_NON_COPYABLE(PluginListBoxModel)
};

//==============================================================================
GraphDocumentComponent::GraphDocumentComponent(AudioPluginFormatManager& fm,
    AudioDeviceManager& dm,
    KnownPluginList& kpl)
    : graph(new PluginGraph(fm, kpl)),
    deviceManager(dm),
    pluginList(kpl),
    graphPlayer(getAppProperties().getUserSettings()->getBoolValue("doublePrecisionProcessing", false))
{
    init();

    deviceManager.addChangeListener(graphPanel.get());
    deviceManager.addAudioCallback(&recorder);
    deviceManager.addMidiInputDeviceCallback({}, &graphPlayer.getMidiMessageCollector());
    deviceManager.addChangeListener(this);
}


void GraphDocumentComponent::init()
{
    updateMidiOutput();

    graphPanel.reset(new GraphEditorPanel(*graph, pluginList));
    graphPanel->addListener(this);

    // Scrolled by dragging, as there's no mouse wheel on the touchscreen and no room for scrollbars
    chainViewport.setViewedComponent(graphPanel.get(), false);
    chainViewport.setScrollBarsShown(false, false, true, true);
    chainViewport.setScrollOnDragMode(Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(chainViewport);
    graphPlayer.setProcessor(&graph->graph);

    keyState.addListener(&graphPlayer.getMidiMessageCollector());

    keyboardComp.reset(new MidiKeyboardComponent(keyState, MidiKeyboardComponent::horizontalKeyboard));
    statusBar.reset(new TooltipBar());
    addAndMakeVisible(statusBar.get());

    graphPanel->updateComponents();

    titleBarComponent.reset(new TitleBarComponent(*this));
    addAndMakeVisible(titleBarComponent.get());

    pluginListBoxModel.reset(new PluginListBoxModel(pluginListBox, pluginList));

    pluginListBox.setModel(pluginListBoxModel.get());
    pluginListBox.setRowHeight(60);

    pluginListSidePanel.setContent(&pluginListBox, false);


    blockControls.reset(new BlockControlsPanel(deviceManager));
    blockControls->onClose = [this] { graphPanel->activeComponentChanged(nullptr); };
    addChildComponent(blockControls.get());

    addAndMakeVisible(pluginListSidePanel);

    // Added last so it covers everything when shown
    settingsPopup.reset(new AudioSettingsPopup(deviceManager));
    addChildComponent(settingsPopup.get());

    recordingsPopup.reset(new RecordingsPopup(recorder));
    addChildComponent(recordingsPopup.get());
}



GraphDocumentComponent::~GraphDocumentComponent()
{
    if (midiOutput != nullptr)
        midiOutput->stopBackgroundThread();

    deviceManager.removeChangeListener(this);

    releaseGraph();

    keyState.removeListener(&graphPlayer.getMidiMessageCollector());
}

void GraphDocumentComponent::resized()
{
    auto r = [this]
        {
            auto bounds = getLocalBounds();

            if (auto* display = Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds()))
                return display->safeAreaInsets.subtractedFrom(bounds);

            return bounds;
        }();

        int titleBarHeight = 44;
        int blockControlsHeight = 136;

        titleBarComponent->setBounds(r.removeFromTop(titleBarHeight));

        // The selected block's controls go along the bottom, and the chain shrinks to fit above them
        blockControls->setVisible(blockControls->isShowingBlock());

        if (blockControls->isShowingBlock())
            blockControls->setBounds(r.removeFromBottom(blockControlsHeight));

        // The chain view makes itself bigger than this if the chains need scrolling
        chainViewport.setBounds(r);
        graphPanel->setSize(r.getWidth(), r.getHeight());

        // Keep the selected block in view, e.g. when its controls have taken some of the space
        if (activePlugin != nullptr)
        {
            const auto view = chainViewport.getViewArea();
            const auto tile = activePlugin->getBounds();

            if (! view.contains(tile))
                chainViewport.setViewPosition(tile.getCentreX() - view.getWidth() / 2, tile.getCentreY() - view.getHeight() / 2);
        }

        settingsPopup->setBounds(getLocalBounds());
        recordingsPopup->setBounds(getLocalBounds());
}


void GraphDocumentComponent::createNewPlugin(const PluginDescriptionAndPreference& desc, Point<int> pos)
{
    graphPanel->createNewPlugin(desc, graphPanel->getLocalPoint(this, pos));
}

void GraphDocumentComponent::releaseGraph()
{
    recorder.stop();
    deviceManager.removeAudioCallback(&recorder);
    deviceManager.removeMidiInputDeviceCallback({}, &graphPlayer.getMidiMessageCollector());

    if (graphPanel != nullptr)
    {
        deviceManager.removeChangeListener(graphPanel.get());
        graphPanel = nullptr;
    }

    keyboardComp = nullptr;
    statusBar = nullptr;

    // Let go of the plugin whose controls are showing before the graph goes
    activePlugin = nullptr;

    if (blockControls != nullptr)
        blockControls->clear();

    graphPlayer.setProcessor(nullptr);
    graph = nullptr;
}

bool GraphDocumentComponent::isInterestedInDragSource(const SourceDetails& details)
{
    return ((dynamic_cast<ListBox*> (details.sourceComponent.get()) != nullptr)
        && details.description.toString().startsWith("PLUGIN"));
}

void GraphDocumentComponent::itemDropped(const SourceDetails& details)
{
    // don't allow items to be dropped behind the sidebar
    if (pluginListSidePanel.getBounds().contains(details.localPosition))
        return;

    auto pluginTypeIndex = details.description.toString()
        .fromFirstOccurrenceOf("PLUGIN: ", false, false)
        .getIntValue();

    // must be a valid index!
    jassert(isPositiveAndBelow(pluginTypeIndex, pluginList.getNumTypes()));

    createNewPlugin(PluginDescriptionAndPreference{ pluginList.getTypes()[pluginTypeIndex] },
        details.localPosition);
}

void GraphDocumentComponent::showSettings()
{
    settingsPopup->show();
}

void GraphDocumentComponent::showPluginList()
{
    pluginListSidePanel.showOrHide(true);
}

void GraphDocumentComponent::showRecordings()
{
    recordingsPopup->show();
}

void GraphDocumentComponent::toggleRecording()
{
    if (recorder.isRecording())
    {
        recorder.stop();
        return;
    }

    // A recording for each output, or pair, that one of the chains' output blocks sends to
    std::vector<Recorder::Output> outputs;

    for (auto* node : graph->graph.getNodes())
    {
        if (auto* output = dynamic_cast<ChainOutputProcessor*>(node->getProcessor()))
        {
            const auto [first, second] = output->getChosenChannels();
            const auto sameOutput = [first = first, second = second](const Recorder::Output& o) { return o.first == first && o.second == second; };

            const auto existing = std::find_if(outputs.begin(), outputs.end(), sameOutput);

            // Outputs sharing channels make one recording, named after whichever of them has a name
            if (existing == outputs.end())
                outputs.push_back({ first, second, output->getBlockName() });
            else if (output->getCustomName().isNotEmpty() && existing->name == output->getChannelName())
                existing->name = output->getCustomName();
        }
    }

    std::sort(outputs.begin(), outputs.end(), [](const auto& a, const auto& b)
    {
        return a.first != b.first ? a.first < b.first : a.second < b.second;
    });

    if (! recorder.start(outputs))
        messageBox = AlertWindow::showScopedAsync(MessageBoxOptions::makeOptionsOk(MessageBoxIconType::WarningIcon,
                                                                                    "Nothing to record",
                                                                                    "The audio device isn't running, or it has none of the outputs the chains send to."),
                                                  nullptr);
}

void GraphDocumentComponent::hideLastSidePanel()
{
    if (recordingsPopup->isVisible())
        recordingsPopup->hide();
    else if (settingsPopup->isVisible())
        settingsPopup->hide();
    else
        pluginListSidePanel.showOrHide(false);
}

void GraphDocumentComponent::setDoublePrecision(bool doublePrecision)
{
    graphPlayer.setDoublePrecisionProcessing(doublePrecision);
}

bool GraphDocumentComponent::closeAnyOpenPluginWindows()
{
    return graphPanel->graph.closeAnyOpenPluginWindows();
}

void GraphDocumentComponent::changeListenerCallback(ChangeBroadcaster*)
{
    updateMidiOutput();

    // Only save changes made from the settings popup. Automatic changes, like falling back to another
    // device because the saved one couldn't be opened at startup, mustn't overwrite the saved choice.
    if (settingsPopup != nullptr && settingsPopup->isVisible())
        saveAudioDeviceState();
}

void GraphDocumentComponent::saveAudioDeviceState()
{
    if (auto state = deviceManager.createStateXml())
    {
        auto* settings = getAppProperties().getUserSettings();
        settings->setValue("audioDeviceState", state.get());
        settings->saveIfNeeded();
    }
}

void GraphDocumentComponent::updateMidiOutput()
{
    auto* defaultMidiOutput = deviceManager.getDefaultMidiOutput();

    if (midiOutput != defaultMidiOutput)
    {
        midiOutput = defaultMidiOutput;

        if (midiOutput != nullptr)
            midiOutput->startBackgroundThread();

        graphPlayer.setMidiOutput(midiOutput);
    }
}

void GraphDocumentComponent::onActiveComponentChange(PluginComponent* activeComponent) {
    activePlugin = activeComponent;
    showBlockControls();
    resized();
}

void GraphDocumentComponent::onNodeDeleted(PluginComponent* removedComponent) {
    if (removedComponent == activePlugin) {
        activePlugin = nullptr;
        blockControls->clear();
        resized();
    }
};

void GraphDocumentComponent::onComponentsUpdated()
{
    if (activePlugin != nullptr && blockControls->isShowingBlock())
        blockControls->setBlockName(activePlugin->getName(), activePlugin->getSubtitle());
}

void GraphDocumentComponent::showBlockControls()
{
    if (activePlugin != nullptr)
    {
        auto node = graph->graph.getNodeForId(activePlugin->pluginID);
        auto* controls = graphPanel->getBlockControls(activePlugin->pluginID);

        if (node != nullptr && controls != nullptr)
        {
            blockControls->showBlock(node, *controls, activePlugin->getName(), activePlugin->getSubtitle(), activePlugin->getIcon());
            return;
        }
    }

    blockControls->clear();
}