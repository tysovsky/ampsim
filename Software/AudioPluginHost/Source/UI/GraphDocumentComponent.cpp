#include "GraphDocumentComponent.h"
#include "../Plugins/InternalPlugins.h"
#include "MainHostWindow.h"

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

        static const unsigned char pluginListPathData[]
            = { 110,109,193,202,222,64,80,50,21,64,108,0,0,48,65,0,0,0,0,108,160,154,112,65,80,50,21,64,108,0,0,48,65,80,
                50,149,64,108,193,202,222,64,80,50,21,64,99,109,0,0,192,64,251,220,127,64,108,160,154,32,65,165,135,202,
                64,108,160,154,32,65,250,220,47,65,108,0,0,192,64,102,144,10,65,108,0,0,192,64,251,220,127,64,99,109,0,0,
                128,65,251,220,127,64,108,0,0,128,65,103,144,10,65,108,96,101,63,65,251,220,47,65,108,96,101,63,65,166,135,
                202,64,108,0,0,128,65,251,220,127,64,99,109,96,101,79,65,148,76,69,65,108,0,0,136,65,0,0,32,65,108,80,
                77,168,65,148,76,69,65,108,0,0,136,65,40,153,106,65,108,96,101,79,65,148,76,69,65,99,109,0,0,64,65,63,247,
                95,65,108,80,77,128,65,233,161,130,65,108,80,77,128,65,125,238,167,65,108,0,0,64,65,51,72,149,65,108,0,0,64,
                65,63,247,95,65,99,109,0,0,176,65,63,247,95,65,108,0,0,176,65,51,72,149,65,108,176,178,143,65,125,238,167,65,
                108,176,178,143,65,233,161,130,65,108,0,0,176,65,63,247,95,65,99,109,12,86,118,63,148,76,69,65,108,0,0,160,
                64,0,0,32,65,108,159,154,16,65,148,76,69,65,108,0,0,160,64,40,153,106,65,108,12,86,118,63,148,76,69,65,99,
                109,0,0,0,0,63,247,95,65,108,62,53,129,64,233,161,130,65,108,62,53,129,64,125,238,167,65,108,0,0,0,0,51,
                72,149,65,108,0,0,0,0,63,247,95,65,99,109,0,0,32,65,63,247,95,65,108,0,0,32,65,51,72,149,65,108,193,202,190,
                64,125,238,167,65,108,193,202,190,64,233,161,130,65,108,0,0,32,65,63,247,95,65,99,101,0,0 };

        {
            Path p;
            p.loadPathFromData(burgerMenuPathData, sizeof(burgerMenuPathData));
            burgerButton.setShape(p, true, true, false);
        }

        {
            Path p;
            p.loadPathFromData(pluginListPathData, sizeof(pluginListPathData));
            pluginButton.setShape(p, true, true, false);
        }

        saveButton.setButtonText("Save");

        burgerButton.addListener(this);
        addAndMakeVisible(burgerButton);

        pluginButton.addListener(this);
        addAndMakeVisible(pluginButton);

        titleLabel.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        saveButton.addListener(this);
        addAndMakeVisible(saveButton);

        setOpaque(true);
    }

private:
    void paint(Graphics& g) override
    {
        auto titleBarBackgroundColour = getLookAndFeel().findColour(ResizableWindow::backgroundColourId).darker();

        g.setColour(titleBarBackgroundColour);
        g.fillRect(getLocalBounds());
    }

    void resized() override
    {
        auto r = getLocalBounds();

        burgerButton.setBounds(r.removeFromLeft(40).withSizeKeepingCentre(20, 20));

        pluginButton.setBounds(r.removeFromRight(40).withSizeKeepingCentre(20, 20));

        saveButton.setBounds(r.removeFromTop(40).withSizeKeepingCentre(50, 20));

        titleLabel.setFont(Font(static_cast<float> (getHeight()) * 0.5f, Font::plain));
        titleLabel.setBounds(r);
    }

    void buttonClicked(Button* b) override
    {
        if (b == &saveButton) {
            owner.graph->saveAsync(false, false, nullptr);
        }
        else {
            owner.showSidePanel(b == &burgerButton);
        }
    }

    GraphDocumentComponent& owner;

    Label titleLabel{ "titleLabel", "Studio" };
    ShapeButton burgerButton{ "burgerButton", Colours::lightgrey, Colours::lightgrey, Colours::white };
    ShapeButton pluginButton{ "pluginButton", Colours::lightgrey, Colours::lightgrey, Colours::white };
    TextButton saveButton;

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
        g.fillAll(rowIsSelected ? Colour(0xff42A2C8)
            : Colour(0xff263238));

        g.setColour(rowIsSelected ? Colours::black : Colours::white);

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
    deviceManager.addAudioCallback(&graphPlayer);
    deviceManager.addMidiInputDeviceCallback({}, &graphPlayer.getMidiMessageCollector());
    deviceManager.addChangeListener(this);
}


void GraphDocumentComponent::init()
{
    updateMidiOutput();

    graphPanel.reset(new GraphEditorPanel(*graph));
    graphPanel->addListener(this);
    addAndMakeVisible(graphPanel.get());
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

    mobileSettingsSidePanel.setContent(new AudioDeviceSelectorComponent(deviceManager,
        0, 2, 0, 2,
        true, true, true, false));


    topPluginControlPanel.reset(new PluginControlsPanel(graphPanel.get(), true));
    bottomPluginControlPanel.reset(new PluginControlsPanel(graphPanel.get(), false));
    addAndMakeVisible(topPluginControlPanel.get());
    addAndMakeVisible(bottomPluginControlPanel.get());

    addAndMakeVisible(pluginListSidePanel);
    addAndMakeVisible(mobileSettingsSidePanel);
}



GraphDocumentComponent::~GraphDocumentComponent()
{
    if (midiOutput != nullptr)
        midiOutput->stopBackgroundThread();

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

        int titleBarHeight = 30;
        int pluginControlsHeight = 0;


        if (activePlugin && activePlugin->getPluginControl()) {
            pluginControlsHeight = 50;
        }
        
        titleBarComponent->setBounds(r.removeFromTop(titleBarHeight));
        

        graphPanel->setBounds(r);

        topPluginControlPanel->setBounds(Rectangle<int>(0, 0, r.getWidth(), pluginControlsHeight));
        bottomPluginControlPanel->setBounds(r.removeFromBottom(pluginControlsHeight));        

        checkAvailableWidth();
}


void GraphDocumentComponent::createNewPlugin(const PluginDescriptionAndPreference& desc, Point<int> pos)
{
    graphPanel->createNewPlugin(desc, pos);
}

void GraphDocumentComponent::releaseGraph()
{
    deviceManager.removeAudioCallback(&graphPlayer);
    deviceManager.removeMidiInputDeviceCallback({}, &graphPlayer.getMidiMessageCollector());

    if (graphPanel != nullptr)
    {
        deviceManager.removeChangeListener(graphPanel.get());
        graphPanel = nullptr;
    }

    keyboardComp = nullptr;
    statusBar = nullptr;

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

void GraphDocumentComponent::showSidePanel(bool showSettingsPanel)
{
    if (showSettingsPanel)
        mobileSettingsSidePanel.showOrHide(true);
    else
        pluginListSidePanel.showOrHide(true);

    checkAvailableWidth();

    lastOpenedSidePanel = showSettingsPanel ? &mobileSettingsSidePanel
        : &pluginListSidePanel;
}

void GraphDocumentComponent::hideLastSidePanel()
{
    if (lastOpenedSidePanel != nullptr)
        lastOpenedSidePanel->showOrHide(false);

    if (mobileSettingsSidePanel.isPanelShowing())    lastOpenedSidePanel = &mobileSettingsSidePanel;
    else if (pluginListSidePanel.isPanelShowing())        lastOpenedSidePanel = &pluginListSidePanel;
    else                                                  lastOpenedSidePanel = nullptr;
}

void GraphDocumentComponent::checkAvailableWidth()
{
    if (mobileSettingsSidePanel.isPanelShowing() && pluginListSidePanel.isPanelShowing())
    {
        if (getWidth() - (mobileSettingsSidePanel.getWidth() + pluginListSidePanel.getWidth()) < 150)
            hideLastSidePanel();
    }
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
    resized();
}

void GraphDocumentComponent::onNodeDeleted(PluginComponent* removedComponent) {
    if (removedComponent == activePlugin) {
        activePlugin = nullptr;
        resized();
    }
};