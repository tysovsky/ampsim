#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    A glowing cloud of particles that moves with a block's sound, as a synth's own
    window might show it. It drifts and breathes while it's quiet; playing makes
    it brighter and swirl faster, the bass swells it, each loud note bursts it
    outwards, and the highs make it sparkle.

    It listens to the block's lane level stage (see LaneLevelProcessor), which
    keeps the last of the sound coming out of the block.

    Each frame is built up in an image of its own, adding each particle's light
    to the pixels, and drawn in one go: far quicker for JUCE's software renderer
    than drawing hundreds of shapes, and the light adds up where they overlap.
*/
class SoundVisualiser final : public Component,
                              private Timer
{
public:
    SoundVisualiser();
    ~SoundVisualiser() override;

    /** The lane level stage to follow, or nullptr for no sound (it just drifts). */
    void setSound (AudioProcessorGraph::Node::Ptr levelStage);

    void paint (Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    struct Particle
    {
        float angle, radius;    // where it is round the middle, radius 0 to 1 out to the cloud's edge
        float speed;            // how fast it goes round, and which way
        float size;             // in pixels
        float twinkle;          // a phase, so they don't all sparkle together
        float depth;            // 0 to 1: how far a burst throws it, and how white it can get
    };

    // A dot's light: a bright core with a soft halo, as 0 to 255 for each pixel of a square
    struct Dot
    {
        int diameter;
        std::vector<uint8> light;
    };

    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;

    void timerCallback() override;
    void listen (float seconds);
    void prepare (float pixelScale);
    void drawFrame();

    AudioProcessorGraph::Node::Ptr sound;
    std::vector<Particle> particles;
    std::vector<Dot> dots;              // smallest first
    std::vector<uint8> haze;            // how much of the glow in the middle each pixel gets, 0 to 255
    Image frame;
    float pixelScale = 0.0f;            // the screen's pixels for each point, which the images are made for

    dsp::FFT fft { fftOrder };
    dsp::WindowingFunction<float> window { (size_t) fftSize, dsp::WindowingFunction<float>::hann };
    std::array<float, 2 * fftSize> samples {};

    // What it's hearing, each from 0 to 1, smoothed from one frame to the next
    float loudness = 0.0f, averageLoudness = 0.0f, bass = 0.0f, middle = 0.0f, highs = 0.0f;
    float burst = 0.0f;         // set to 1 by a loud note, and dies away
    double time = 0.0, lastFrame = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundVisualiser)
};
