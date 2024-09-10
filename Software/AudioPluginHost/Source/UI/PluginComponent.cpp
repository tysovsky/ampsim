#include "PluginComponent.h"
#include "GraphEditorPanel.h"

#include "PluginControls/ControlXmlParser.h"

PluginComponent::PluginComponent(GraphEditorPanel& p, AudioProcessorGraph::NodeID id) : panel(p), graph(p.graph), pluginID(id)
{
    shadow.setShadowProperties(DropShadow(Colours::black.withAlpha(0.5f), 3, { 0, 1 }));
    setComponentEffect(&shadow);

    if (auto f = graph.graph.getNodeForId(pluginID))
    {
        if (auto* processor = f->getProcessor())
        {
            if (auto* bypassParam = processor->getBypassParameter())
                bypassParam->addListener(this);
        }
    }

    setSize(150, 60);

    nodeActionWindow = std::make_unique<AlertWindow>("", "", MessageBoxIconType::NoIcon, nullptr);

    loadPluginControl();
}



PluginComponent::~PluginComponent()
{
    if (auto f = graph.graph.getNodeForId(pluginID))
    {
        if (auto* processor = f->getProcessor())
        {
            if (auto* bypassParam = processor->getBypassParameter())
                bypassParam->removeListener(this);
        }
    }
}

void PluginComponent::mouseDown(const MouseEvent& e)
{
    originalPos = localPointToGlobal(Point<int>());

    toFront(true);

    PopupMenu::dismissAllActiveMenus();

    startTimer(750);
}

void PluginComponent::mouseDrag(const MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 5)
        stopTimer();

    if (!e.mods.isPopupMenu())
    {
        auto pos = originalPos + e.getOffsetFromDragStart();

        if (getParentComponent() != nullptr)
            pos = getParentComponent()->getLocalPoint(nullptr, pos);

        pos += getLocalBounds().getCentre();

        graph.setNodePosition(pluginID,
            { pos.x / (double)getParentWidth(),
                pos.y / (double)getParentHeight() });

        panel.updateComponents();
    }
}

void PluginComponent::mouseUp(const MouseEvent& e)
{
    stopTimer();

    if (e.mouseWasDraggedSinceMouseDown())
    {
        graph.setChangedFlag(true);
    }
    else if (e.getNumberOfClicks() == 2)
    {
        if (auto f = graph.graph.getNodeForId(pluginID))
            if (auto* w = graph.getOrCreateWindowFor(f, PluginWindow::Type::normal))
                w->toFront(true);
    }
    else {
        setActive(true);
    }
}

bool  PluginComponent::hitTest(int x, int y)
{
    for (auto* child : getChildren())
        if (child->getBounds().contains(x, y))
            return true;

    return x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize;
}

void PluginComponent::paint(Graphics& g)
{
    auto boxArea = getLocalBounds().reduced(pinSize, 4);
    bool isBypassed = false;

    if (auto* f = graph.graph.getNodeForId(pluginID))
        isBypassed = f->isBypassed();

    auto boxColour = findColour(TextEditor::backgroundColourId);

    if (isBypassed)
        boxColour = boxColour.brighter();

    g.setColour(boxColour);
    g.fillRect(boxArea.toFloat());

    g.setColour(findColour(TextEditor::textColourId));
    g.setFont(font);
    g.drawFittedText(getName(), boxArea, Justification::centred, 2);
    if (isActive) {
        g.drawRect(boxArea.toFloat(), 1.0f);
    }
}

void PluginComponent::resized()
{
    if (auto f = graph.graph.getNodeForId(pluginID))
    {
        if (auto* processor = f->getProcessor())
        {
            for (auto* pin : pins)
            {
                const bool isInput = pin->isInput;
                auto channelIndex = pin->pin.channelIndex;
                int busIdx = 0;
                processor->getOffsetInBusBufferForAbsoluteChannelIndex(isInput, channelIndex, busIdx);

                const int total = isInput ? numIns : numOuts;
                const int index = pin->pin.isMIDI() ? (total - 1) : channelIndex;

                auto totalSpaces = static_cast<float> (total) + (static_cast<float> (jmax(0, processor->getBusCount(isInput) - 1)) * 0.5f);
                auto indexPos = static_cast<float> (index) + (static_cast<float> (busIdx) * 0.5f);

                pin->setBounds(
                    pin->isInput ? 0 : (getWidth() - pinSize),
                    proportionOfHeight((1.0f + indexPos) / (totalSpaces + 1.0f)) - pinSize / 2,
                    pinSize,
                    pinSize);
            }
        }
    }
}

Point<float> PluginComponent::getPinPos(int index, bool isInput) const
{
    for (auto* pin : pins)
        if (pin->pin.channelIndex == index && isInput == pin->isInput)
            return getPosition().toFloat() + pin->getBounds().getCentre().toFloat();

    return {};
}

void PluginComponent::update()
{
    const AudioProcessorGraph::Node::Ptr f(graph.graph.getNodeForId(pluginID));
    jassert(f != nullptr);

    auto& processor = *f->getProcessor();

    numIns = processor.getTotalNumInputChannels();
    if (processor.acceptsMidi())
        ++numIns;

    numOuts = processor.getTotalNumOutputChannels();
    if (processor.producesMidi())
        ++numOuts;

    int w = 100 + pinSize;
    int h = 100;

    h = jmax(h, (jmax(numIns, numOuts) + 1) * pinSize);

    const int textWidth = font.getStringWidth(processor.getName());
    w = jmax(w, 16 + jmin(textWidth, 300));
    if (textWidth > 300)
        h = 100;

    setSize(w, h);
    setName(processor.getName());

    {
        auto p = graph.getNodePosition(pluginID);
        setCentreRelative((float)p.x, (float)p.y);
    }

    if (numIns != numInputs || numOuts != numOutputs)
    {
        numInputs = numIns;
        numOutputs = numOuts;

        pins.clear();

        for (int i = 0; i < processor.getTotalNumInputChannels(); ++i)
            addAndMakeVisible(pins.add(new PinComponent(panel, { pluginID, i }, true)));

        if (processor.acceptsMidi())
            addAndMakeVisible(pins.add(new PinComponent(panel, { pluginID, AudioProcessorGraph::midiChannelIndex }, true)));

        for (int i = 0; i < processor.getTotalNumOutputChannels(); ++i)
            addAndMakeVisible(pins.add(new PinComponent(panel, { pluginID, i }, false)));

        if (processor.producesMidi())
            addAndMakeVisible(pins.add(new PinComponent(panel, { pluginID, AudioProcessorGraph::midiChannelIndex }, false)));

        resized();
    }
}

AudioProcessor* PluginComponent::getProcessor() const
{
    if (auto node = graph.graph.getNodeForId(pluginID))
        return node->getProcessor();

    return {};
}

bool PluginComponent::isNodeUsingARA() const
{
    if (auto node = graph.graph.getNodeForId(pluginID))
        return node->properties["useARA"];

    return false;
}

void PluginComponent::showPopupMenu()
{
    String  bypassBtnText = "Bypass";
    String nodeName = "Node";

    auto* node = graph.graph.getNodeForId(pluginID);

    if (node) {
        if (node->isBypassed()) {
            bypassBtnText = "Enable";
        }

        nodeName = node->getProcessor()->getName();
    }

    if (nodeName == "Audio Input" ||
        nodeName == "Audio Output") {
        return;
    }

    nodeActionWindow->showYesNoCancelBox(
        MessageBoxIconType::NoIcon,
        nodeName,
        "",
        bypassBtnText,
        "Disconnect All",
        "Delete",
        nullptr,
        ModalCallbackFunction::create([this, node](int result) {
            switch (result)
            {
            case 1:
            {
                if (node)
                    node->setBypassed(!node->isBypassed());

                repaint();
                break;
            }
            case 2:
            {
                //graph.graph.disconnectNode(pluginID);
                showWindow(PluginWindow::Type::generic);
                break;
            }
            case 0:
            {
                graph.graph.removeNode(pluginID);
                break;
            }

            default:
                break;
            }
            })
    );
}

void PluginComponent::testStateSaveLoad()
{
    if (auto* processor = getProcessor())
    {
        MemoryBlock state;
        processor->getStateInformation(state);
        processor->setStateInformation(state.getData(), (int)state.getSize());
    }
}

void PluginComponent::showWindow(PluginWindow::Type type)
{
    if (auto node = graph.graph.getNodeForId(pluginID))
        if (auto* w = graph.getOrCreateWindowFor(node, type))
            w->toFront(true);
}

void PluginComponent::timerCallback()
{

    stopTimer();
    showPopupMenu();
}

void PluginComponent::parameterValueChanged(int, float)
{
    // Parameter changes might come from the audio thread or elsewhere, but
    // we can only call repaint from the message thread.
    triggerAsyncUpdate();
}

void PluginComponent::savePluginState()
{
    fileChooser = std::make_unique<FileChooser>("Save plugin state");

    const auto onChosen = [ref = SafePointer<PluginComponent>(this)](const FileChooser& chooser)
        {
            if (ref == nullptr)
                return;

            const auto result = chooser.getResult();

            if (result == File())
                return;

            if (auto* node = ref->graph.graph.getNodeForId(ref->pluginID))
            {
                MemoryBlock block;
                node->getProcessor()->getStateInformation(block);
                result.replaceWithData(block.getData(), block.getSize());
            }
        };

    fileChooser->launchAsync(FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, onChosen);
}

void PluginComponent::loadPluginState()
{
    fileChooser = std::make_unique<FileChooser>("Load plugin state");

    const auto onChosen = [ref = SafePointer<PluginComponent>(this)](const FileChooser& chooser)
        {
            if (ref == nullptr)
                return;

            const auto result = chooser.getResult();

            if (result == File())
                return;

            if (auto* node = ref->graph.graph.getNodeForId(ref->pluginID))
            {
                if (auto stream = result.createInputStream())
                {
                    MemoryBlock block;
                    stream->readIntoMemoryBlock(block);
                    node->getProcessor()->setStateInformation(block.getData(), (int)block.getSize());
                }
            }
        };

    fileChooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, onChosen);
}

void PluginComponent::setActive(bool active) {
    // Do nothing if this component is active alrady (ie if a componenent is clicked multiple times)
    if (active && panel.activeComponent == this) {
        return;
    }

    isActive = active;

    if (isActive)
        panel.activeComponentChanged(this);

    repaint();
}

PluginControl* PluginComponent::getPluginControl() {

    return pluginControl.get();
}

void PluginComponent::loadPluginControl() {
    auto name = getProcessor()->getName();
    auto pluginPath = panel.pluginControlPaths[name.toStdString()];

    if (pluginPath != "") {
        auto parser = ControlXmlParser(getParameters());
        auto control = parser.parse(pluginPath);

        pluginControl.reset(control);
    }
    else {
        pluginControl = std::unique_ptr<PluginControl>{};
    }
}