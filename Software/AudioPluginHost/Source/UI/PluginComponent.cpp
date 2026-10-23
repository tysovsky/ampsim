#include "PluginComponent.h"
#include "GraphEditorPanel.h"
#include "ThemeManager.h"

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
    // Tiles sit in signal-chain order, so dragging doesn't move them; it just cancels a long press
    if (e.getDistanceFromDragStart() > 5)
        stopTimer();
}

void PluginComponent::mouseUp(const MouseEvent& e)
{
    stopTimer();

    if (e.mouseWasDraggedSinceMouseDown())
    {
        return;
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

void PluginComponent::paint(Graphics& g)
{
    auto tile = getLocalBounds().toFloat().reduced(4.0f);
    bool isBypassed = false;

    if (auto* f = graph.graph.getNodeForId(pluginID))
        isBypassed = f->isBypassed();

    g.setColour(findColour(isBypassed ? AppColourIds::bypassedNodeBackground
                                      : TextEditor::backgroundColourId));
    g.fillRoundedRectangle(tile, 8.0f);

    if (isActive)
    {
        g.setColour(findColour(AppColourIds::signalPath));
        g.drawRoundedRectangle(tile, 8.0f, 3.0f);
    }

    g.setColour(findColour(TextEditor::textColourId).withMultipliedAlpha(isBypassed ? 0.5f : 1.0f));
    g.setFont(font);
    g.drawFittedText(getName(), tile.reduced((float)labelPadding).toNearestInt(), Justification::centred, 3);
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

    // The panel sizes and places the tiles along the chain, so only the name needs updating here
    setName(f->getProcessor()->getName());
    repaint();
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
                panel.removeFromChain(pluginID);
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