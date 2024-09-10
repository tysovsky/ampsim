#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"

class GraphEditorPanel;

class PinComponent final : public Component {
public:
    PinComponent(GraphEditorPanel& p, AudioProcessorGraph::NodeAndChannel pinToUse, bool isIn);

    void paint(Graphics&) override;

    void mouseDown(const MouseEvent&) override;
    void mouseUp(const MouseEvent&) override;
    void mouseDrag(const MouseEvent&) override;

    const bool isInput;
    AudioProcessorGraph::NodeAndChannel pin;

private:
    GraphEditorPanel& panel;
    PluginGraph& graph;
    
    int busIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PinComponent)
};