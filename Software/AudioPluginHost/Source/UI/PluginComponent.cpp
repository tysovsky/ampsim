#include "PluginComponent.h"
#include "GraphEditorPanel.h"
#include "ThemeManager.h"
#include "../Plugins/InternalPlugins.h"


PluginComponent::PluginComponent(GraphEditorPanel& p, AudioProcessorGraph::NodeID id) : panel(p), graph(p.graph), pluginID(id)
{
    shadow.setShadowProperties(DropShadow(Colours::black.withAlpha(0.5f), 3, { 0, 1 }));
    setComponentEffect(&shadow);

    pluginNode = graph.graph.getNodeForId(pluginID);

    if (pluginNode != nullptr)
        if (auto* bypassParam = pluginNode->getProcessor()->getBypassParameter())
            bypassParam->addListener(this);

    setSize(150, 60);

    nodeActionWindow = std::make_unique<AlertWindow>("", "", MessageBoxIconType::NoIcon, nullptr);
}



PluginComponent::~PluginComponent()
{
    // The node's usually gone from the graph by now, but it's held, so its plugin is still there
    // to stop telling this tile about its bypass
    if (pluginNode != nullptr)
        if (auto* bypassParam = pluginNode->getProcessor()->getBypassParameter())
            bypassParam->removeListener(this);

    setLaneLevel(nullptr);
}

void PluginComponent::mouseDown(const MouseEvent& e)
{
    // Touching the mix bar sets the lane's level instead of selecting the tile
    if (auto* level = getLaneLevelParameter(); level != nullptr && getMixBarArea().contains(e.getPosition()))
    {
        // Sliding along the bar mustn't also scroll the chains
        setViewportIgnoreDragFlag(true);
        isAdjustingLevel = true;
        level->beginChangeGesture();
        setLaneLevelFromPosition(e.x);
        return;
    }

    originalPos = localPointToGlobal(Point<int>());

    PopupMenu::dismissAllActiveMenus();

    // The dot at either end of the tile drags out a link to another block, rather than picking the
    // tile up or holding it, and mustn't scroll the chains either
    if (panel.beginLink(*this, e.getPosition()))
    {
        isDraggingLink = true;
        setViewportIgnoreDragFlag(true);
        return;
    }

    toFront(true);

    // A plugin's tile is picked up by dragging it, to move it somewhere else in the chains, so
    // dragging it mustn't also scroll them. A chain's inputs and outputs stay where they are.
    canBeDragged = panel.canMoveBlock(pluginID);
    setViewportIgnoreDragFlag(canBeDragged);
    grabOffset = e.getPosition();

    startTimer(750);
}

void PluginComponent::mouseDrag(const MouseEvent& e)
{
    if (isAdjustingLevel)
    {
        setLaneLevelFromPosition(e.x);
        return;
    }

    if (isDraggingLink)
    {
        panel.dragLink(panel.getLocalPoint(nullptr, e.getScreenPosition()));
        return;
    }

    // Moving cancels a long press, and moving further picks the tile up
    if (e.getDistanceFromDragStart() > 5)
        stopTimer();

    if (canBeDragged && (isBeingDragged || e.getDistanceFromDragStart() > dragDistance))
    {
        isBeingDragged = true;
        panel.dragBlock(*this, panel.getLocalPoint(nullptr, e.getScreenPosition()), grabOffset);
    }
}

void PluginComponent::mouseUp(const MouseEvent& e)
{
    if (isAdjustingLevel)
    {
        isAdjustingLevel = false;
        setViewportIgnoreDragFlag(false);

        if (auto* level = getLaneLevelParameter())
            level->endChangeGesture();

        return;
    }

    stopTimer();
    setViewportIgnoreDragFlag(false);

    if (isDraggingLink)
    {
        isDraggingLink = false;
        panel.endLink();
        return;
    }

    if (isBeingDragged)
    {
        isBeingDragged = false;
        panel.dropBlock(*this);
        return;
    }

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

    // Red, the colour the recordings list deletes in, while letting go of it would delete it
    const Colour deleteColour(0xffe0342c);

    g.setColour(willBeDeleted ? deleteColour
                              : findColour(isBypassed ? AppColourIds::bypassedNodeBackground
                                                      : TextEditor::backgroundColourId));
    g.fillRoundedRectangle(tile, 8.0f);

    if (isActive)
    {
        g.setColour(findColour(AppColourIds::signalPath));
        g.drawRoundedRectangle(tile, 8.0f, 3.0f);
    }

    const auto textColour = findColour(TextEditor::textColourId).withMultipliedAlpha(isBypassed ? 0.5f : 1.0f);
    auto nameArea = tile.reduced((float)labelPadding);

    if (auto* level = getLaneLevelParameter())
    {
        // The mix bar along the bottom: filled up to the lane's level, with the level as a percentage
        const auto bar = getMixBarArea().toFloat();
        const auto accent = findColour(AppColourIds::signalPath);

        g.setColour(accent.withAlpha(0.25f));
        g.fillRoundedRectangle(bar, 4.0f);
        g.setColour(accent);
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * level->get()), 4.0f);

        g.setColour(textColour);
        g.setFont(Font(bar.getHeight() * 0.55f, Font::bold));
        g.drawText("Mix " + String(roundToInt(level->get() * 100.0f)) + "%", bar, Justification::centred);

        nameArea = nameArea.withBottom(bar.getY());
    }

    // The icon across the top says what kind of block this is. It gets all but a line of the name's
    // space on small tiles, and the plugin's name only goes underneath on tiles without a mix bar.
    const auto iconColour = findColour(AppColourIds::signalPath).withMultipliedAlpha(isBypassed ? 0.5f : 1.0f);
    const bool showSubtitle = subtitle.isNotEmpty() && nameArea.getHeight() >= 90.0f;
    const auto iconArea = nameArea.removeFromTop(jmin(40.0f, nameArea.getHeight() - 24.0f)).reduced(0.0f, 2.0f);

    if (willBeDeleted)
        drawBinIcon(g, iconArea, Colours::white);
    else
        drawBlockIcon(g, icon, iconArea, iconColour);

    if (showSubtitle)
    {
        const auto subtitleArea = nameArea.removeFromBottom(26.0f);
        g.setColour(textColour.withMultipliedAlpha(0.6f));
        g.setFont(Font(12.0f));
        g.drawFittedText(subtitle, subtitleArea.toNearestInt(), Justification::centredTop, 2);
    }

    g.setColour(textColour);
    g.setFont(font);
    g.drawFittedText(getName(), nameArea.toNearestInt(), Justification::centred, 3);
}

void PluginComponent::setLaneLevel(AudioProcessorGraph::Node::Ptr levelNode)
{
    if (levelNode == laneLevelNode)
        return;

    if (auto* level = getLaneLevelParameter())
        level->removeListener(this);

    laneLevelNode = levelNode;

    if (auto* level = getLaneLevelParameter())
        level->addListener(this);

    repaint();
}

void PluginComponent::setWillBeDeleted(bool shouldShowDeleting)
{
    if (willBeDeleted != shouldShowDeleting)
    {
        willBeDeleted = shouldShowDeleting;
        repaint();
    }
}

AudioParameterFloat* PluginComponent::getLaneLevelParameter() const
{
    if (laneLevelNode != nullptr)
        if (auto* levelStage = dynamic_cast<LaneLevelProcessor*>(laneLevelNode->getProcessor()))
            return &levelStage->getLevel();

    return nullptr;
}

Rectangle<int> PluginComponent::getMixBarArea() const
{
    // Inside the tile's rounded corners, tall enough to hit with a finger
    auto tile = getLocalBounds().reduced(10);
    return tile.removeFromBottom(jmax(20, roundToInt((float)getHeight() * 0.26f)));
}

void PluginComponent::setLaneLevelFromPosition(int x)
{
    if (auto* level = getLaneLevelParameter())
    {
        const auto bar = getMixBarArea();
        level->setValueNotifyingHost(jlimit(0.0f, 1.0f, (float)(x - bar.getX()) / (float)bar.getWidth()));
        repaint();
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

    // The panel sizes and places the tiles along the chain, so only the name needs updating here.
    // A plugin added as one of its modules shows the module's name, with the plugin's underneath.
    const auto pluginName = f->getProcessor()->getName();
    const auto moduleName = f->properties["module"].toString();

    setName(moduleName.isNotEmpty() ? moduleName : pluginName);
    subtitle = moduleName.isNotEmpty() ? pluginName : String();

    // A chain's input or output goes by its channel, e.g. "Input 2", or by the name it's been given,
    // with its channel underneath
    if (auto* end = dynamic_cast<ChainEndProcessor*>(f->getProcessor()))
    {
        setName(end->getBlockName());
        subtitle = end->getCustomName().isNotEmpty() ? end->getChannelName() : String();
    }
    icon = panel.getBlockIcon(pluginID);
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

    // A chain's input or output can be renamed or deleted; the last of either takes its chain with it
    if (node != nullptr && dynamic_cast<ChainEndProcessor*>(node->getProcessor()) != nullptr) {
        const bool isInput = dynamic_cast<ChainInputProcessor*>(node->getProcessor()) != nullptr;
        const bool deletesChain = panel.removesWholeChain(pluginID);

        nodeActionWindow->showYesNoCancelBox(
            MessageBoxIconType::NoIcon,
            getName(),
            deletesChain ? "Deleting the chain's only " + String(isInput ? "input" : "output")
                           + " deletes the chain and every block in it too."
                         : String(),
            "Rename",
            deletesChain ? "Delete chain" : "Delete",
            "Cancel",
            nullptr,
            ModalCallbackFunction::create([this](int result) {
                if (result == 1)
                    panel.renameChainEnd(pluginID);
                else if (result == 2)
                    panel.removeFromChain(pluginID);
            }));
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
                    graph.setNodeBypassed(pluginID, !node->isBypassed());

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

