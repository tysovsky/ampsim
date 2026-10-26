#pragma once

#include <JuceHeader.h>
#include "ControlStrip.h"
#include "SoundVisualiser.h"
#include "TouchLookAndFeel.h"

//==============================================================================
/**
    A block's editor, covering the whole window, for plugins with more controls
    than fit along the bottom (such as a synth). It's set out in an <Editor> in the
    plugin's definition file, as pages picked with tabs along the top:

      <Editor>
        <Page name="Play">
          <Visualiser/>                          the block's sound, drawn (see SoundVisualiser),
                                                 above the rows, which go along the bottom
          <Row label="Macros"> ... </Row>        a row of controls, as in <Controls> (see ControlStrip)
          <Row> ... </Row>                       the rows share the page's height
        </Page>
        ...
      </Editor>

    The editor opens on the page it was last showing for the block.
*/
class BlockEditor final : public Component
{
public:
    BlockEditor();
    ~BlockEditor() override;

    /** Opens over the whole of a component for a block, from its definition file's <Editor>. The
        block's lane level stage, if it has one, is where a <Visualiser> gets its sound from. */
    void show (Component& parent, AudioProcessorGraph::Node::Ptr, const XmlElement& editor, const String& title,
               AudioProcessorGraph::Node::Ptr levelStage);
    void hide();

    /** True while it's open for this block. */
    bool isShowing (AudioProcessorGraph::NodeID) const;

    /** Called after another page or a <View>'s option has been picked, which are kept as properties
        of the block, not the plugin. */
    std::function<void()> onViewChange;

    void paint (Graphics&) override;
    void resized() override;
    void parentSizeChanged() override;

private:
    void showPage (int index);

    TouchLookAndFeel lookAndFeel;

    AudioProcessorGraph::Node::Ptr node;        // held so the plugin outlives its controls
    AudioProcessorGraph::Node::Ptr levelStage;
    std::unique_ptr<XmlElement> editor;
    int currentPage = 0;

    Label titleLabel;
    OwnedArray<TextButton> tabs;
    TextButton closeButton { "Close" };
    OwnedArray<ControlStrip> rows;
    StringArray rowLabels;
    std::unique_ptr<SoundVisualiser> visualiser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockEditor)
};
