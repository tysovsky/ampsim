#include "PinComponent.h"
#include "GraphEditorPanel.h"

PinComponent::PinComponent(GraphEditorPanel& p, AudioProcessorGraph::NodeAndChannel pinToUse, bool isIn)
    : panel(p), graph(p.graph), pin(pinToUse), isInput(isIn)
{
    setSize(16, 16);
}

void PinComponent::paint(Graphics& g)
{
    auto w = (float)getWidth();
    auto h = (float)getHeight();

    Path p;
    p.addEllipse(w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
    p.addRectangle(isInput ? (0.5f * w) : 0.0f, h * 0.4, w * 0.5f, h * 0.2f);

    auto colour = (pin.isMIDI() ? Colours::red : Colours::green);

    g.setColour(colour.withRotatedHue((float)busIdx / 5.0f));
    g.fillPath(p);
}

void PinComponent::mouseDown(const MouseEvent& e)
{
    AudioProcessorGraph::NodeAndChannel dummy{ {}, 0 };

    panel.beginConnectorDrag(isInput ? dummy : pin,
        isInput ? pin : dummy,
        e);
}

void PinComponent::mouseDrag(const MouseEvent& e)
{
    panel.dragConnector(e);
}

void PinComponent::mouseUp(const MouseEvent& e)
{
    panel.endDraggingConnector(e);
}

