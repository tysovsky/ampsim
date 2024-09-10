#pragma once

#include <JuceHeader.h>
#include "../Plugins/PluginGraph.h"

class GraphEditorPanel;

//==============================================================================
/**
    A component that represents the arros between various pluginc on the graph.
*/
class ConnectorComponent final : public Component, public SettableTooltipClient
{
public:
    ConnectorComponent(GraphEditorPanel& p);

    void setInput(AudioProcessorGraph::NodeAndChannel newSource);
    void setOutput(AudioProcessorGraph::NodeAndChannel newDest);
    void dragStart(Point<float> pos);
    void dragEnd(Point<float> pos);
    void update();
    void resizeToFit();
    void getPoints(Point<float>& p1, Point<float>& p2) const;
    void paint(Graphics& g) override;
    bool hitTest(int x, int y) override;
    void mouseDown(const MouseEvent&) override;
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;
    void resized() override;
    void getDistancesFromEnds(Point<float> p, double& distanceFromStart, double& distanceFromEnd) const;

    AudioProcessorGraph::Connection connection{ { {}, 0 }, { {}, 0 } };

private:
    GraphEditorPanel& panel;
    PluginGraph& graph;
   
    Point<float> lastInputPos, lastOutputPos;
    Path linePath, hitPath;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConnectorComponent)
};