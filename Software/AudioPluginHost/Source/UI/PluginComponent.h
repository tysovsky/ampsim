#pragma once

#include <JuceHeader.h>
#include "PinComponent.h"

class GraphEditorPanel;
class PluginControl;

class PluginComponent final : public Component,
                              public Timer,
                              private AudioProcessorParameter::Listener,
                              private AsyncUpdater
{
public:
    PluginComponent(GraphEditorPanel& p, AudioProcessorGraph::NodeID id);
    PluginComponent(const PluginComponent&) = delete;
    PluginComponent& operator= (const PluginComponent&) = delete;
    ~PluginComponent() override;

    void mouseDown(const MouseEvent& e) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    bool hitTest(int x, int y) override;
    void paint(Graphics& g) override;
    void resized() override;
    Point<float> getPinPos(int index, bool isInput) const;
    void update();
    AudioProcessor* getProcessor() const;
    bool isNodeUsingARA() const;
    void showPopupMenu();
    void testStateSaveLoad();
    void showWindow(PluginWindow::Type type);
    void timerCallback() override;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {};
    void handleAsyncUpdate() override { repaint(); };
    void savePluginState();
    void loadPluginState();
    void setActive(bool active);
    
    std::map<String, AudioProcessorParameter*> getParameters() {
        std::map<String, AudioProcessorParameter*> result;

        auto params = getProcessor()->getParameters();

        for (auto p : params) {
            result[p->getName(20)] = p;
        }

        return result;
    }
    
    PluginControl* getPluginControl();

    const AudioProcessorGraph::NodeID pluginID;


private:
    void loadPluginControl();

    GraphEditorPanel& panel;
    PluginGraph& graph;
    OwnedArray<PinComponent> pins;
    int numInputs = 0, numOutputs = 0;
    int pinSize = 32;
    Point<int> originalPos;
    Font font{ 13.0f, Font::bold };
    int numIns = 0, numOuts = 0;
    DropShadowEffect shadow;
    std::unique_ptr<PopupMenu> menu;
    std::unique_ptr<AlertWindow> nodeActionWindow;
    std::unique_ptr<FileChooser> fileChooser;
    std::unique_ptr < PluginControl> pluginControl;
    const String formatSuffix = getFormatSuffix(getProcessor());
    bool isActive = false;

};