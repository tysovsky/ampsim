#include "SoundVisualiser.h"
#include "ThemeManager.h"
#include "../Plugins/InternalPlugins.h"

namespace
{
    constexpr int numParticles = 520;
    constexpr int framesPerSecond = 60;

    // A level in decibels as 0 to 1, from the quietest that's worth showing to full scale
    float toUnit (float gain, float quietestDecibels)
    {
        const auto decibels = Decibels::gainToDecibels (gain, -100.0f);
        return jlimit (0.0f, 1.0f, (decibels - quietestDecibels) / -quietestDecibels);
    }

    // Eases towards a target, faster on the way up than down, like a meter
    void follow (float& value, float target, float rise, float fall)
    {
        value += (target - value) * (target > value ? rise : fall);
    }
}

//==============================================================================
SoundVisualiser::SoundVisualiser()
{
    setOpaque (true);

    // More of them towards the middle, going round mostly one way, with a few against the flow
    Random random (1234);

    for (int i = 0; i < numParticles; ++i)
    {
        const auto spread = std::abs (random.nextFloat() + random.nextFloat() - 1.0f);

        particles.push_back ({ random.nextFloat() * MathConstants<float>::twoPi,
                               jmin (1.0f, 0.04f + spread * 1.15f),
                               (random.nextFloat() < 0.15f ? -1.0f : 1.0f) * (0.5f + random.nextFloat()),
                               0.7f + random.nextFloat() * random.nextFloat() * 2.4f,
                               random.nextFloat() * MathConstants<float>::twoPi,
                               random.nextFloat() });
    }
}

SoundVisualiser::~SoundVisualiser()
{
    stopTimer();
}

void SoundVisualiser::setSound (AudioProcessorGraph::Node::Ptr levelStage)
{
    sound = std::move (levelStage);
}

void SoundVisualiser::visibilityChanged()
{
    // Only drawn while it can be seen
    if (isVisible())
    {
        lastFrame = Time::getMillisecondCounterHiRes();
        startTimerHz (framesPerSecond);
    }
    else
    {
        stopTimer();
    }
}

void SoundVisualiser::timerCallback()
{
    if (! isShowing() || isCovered())
        return;

    const auto now = Time::getMillisecondCounterHiRes();
    const auto seconds = (float) jlimit (0.0, 0.1, (now - lastFrame) / 1000.0);
    lastFrame = now;
    time += seconds;

    listen (seconds);

    // Round they go, the inner ones faster, like a galaxy, and faster still the busier the sound
    const auto swirl = 0.10f + 0.9f * middle + 0.7f * burst + 0.3f * loudness;

    for (auto& p : particles)
        p.angle += seconds * p.speed * swirl / (0.3f + p.radius);

    repaint();
}

bool SoundVisualiser::isCovered() const
{
    // Whatever's in front at its middle, such as a popup over the window
    auto* top = getTopLevelComponent();
    auto* inFront = top->getComponentAt (top->getLocalPoint (this, getLocalBounds().getCentre()));

    return inFront != nullptr && inFront != this && ! isParentOf (inFront) && ! inFront->isParentOf (this);
}

void SoundVisualiser::listen (float seconds)
{
    float rms = 0.0f, bassLevel = 0.0f, middleLevel = 0.0f, highLevel = 0.0f;

    if (auto* stage = sound != nullptr ? dynamic_cast<LaneLevelProcessor*> (sound->getProcessor()) : nullptr)
    {
        stage->copyRecentInput (samples.data(), fftSize);

        for (int i = 0; i < fftSize; ++i)
            rms += samples[(size_t) i] * samples[(size_t) i];

        rms = std::sqrt (rms / (float) fftSize);

        // The spectrum, then the average level in each band. A full-scale sine comes out at a quarter
        // of the FFT's size through the Hann window.
        window.multiplyWithWindowingTable (samples.data(), (size_t) fftSize);
        std::fill (samples.begin() + fftSize, samples.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (samples.data());

        const auto binWidth = (float) stage->getRecentInputSampleRate() / (float) fftSize;
        const auto band = [&] (float lowest, float highest)
        {
            const int first = jmax (1, (int) (lowest / binWidth)), last = jmin (fftSize / 2, (int) (highest / binWidth));
            float sum = 0.0f;

            for (int bin = first; bin <= last; ++bin)
                sum += samples[(size_t) bin];

            return sum / (float) jmax (1, last - first + 1) / ((float) fftSize / 4.0f);
        };

        bassLevel = band (30.0f, 250.0f);
        middleLevel = band (250.0f, 2500.0f);
        highLevel = band (2500.0f, 12000.0f);
    }

    const auto heard = toUnit (rms, -54.0f);

    // A note's attack: a jump well above what it's been hearing lately
    if (heard > averageLoudness + 0.15f && heard > loudness + 0.06f)
        burst = 1.0f;

    burst *= std::exp (-seconds * 2.6f);

    follow (loudness, heard, 0.5f, 0.06f);
    follow (averageLoudness, heard, 0.02f, 0.02f);
    follow (bass, toUnit (bassLevel, -60.0f), 0.4f, 0.05f);
    follow (middle, toUnit (middleLevel, -66.0f), 0.3f, 0.05f);
    follow (highs, toUnit (highLevel, -72.0f), 0.4f, 0.08f);
}

void SoundVisualiser::resized()
{
    // Made again for the new size when it's next drawn
    frame = Image();
}

void SoundVisualiser::prepare (float scale)
{
    // At the screen's own pixels, so the frame is copied to it as it is, not scaled up for a screen
    // with more pixels than points
    pixelScale = scale;
    const int width = roundToInt ((float) getWidth() * scale), height = roundToInt ((float) getHeight() * scale);
    frame = width > 0 && height > 0 ? Image (Image::RGB, width, height, false) : Image();

    // The haze: an oval glow across the middle, strongest in the middle and fading out to nothing
    haze.assign ((size_t) (width * height), 0);
    const auto centreX = (float) width * 0.5f, centreY = (float) height * 0.5f;
    const auto across = (float) width * 0.45f, down = (float) height * 0.48f;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const auto dx = ((float) x - centreX) / across, dy = ((float) y - centreY) / down;
            const auto distance = std::sqrt (dx * dx + dy * dy);

            if (distance < 1.0f)
                haze[(size_t) (y * width + x)] = (uint8) roundToInt (255.0f * std::pow (1.0f - distance, 1.6f));
        }
    }

    // The dots, from a pinprick to a big glow: a bright core inside a halo that fades away
    dots.clear();

    for (auto points : { 3, 4, 5, 6, 8, 10, 12, 15, 18, 22, 27, 33 })
    {
        const int diameter = jmax (2, roundToInt ((float) points * scale));

        if (! dots.empty() && dots.back().diameter >= diameter)
            continue;

        Dot dot { diameter, std::vector<uint8> ((size_t) (diameter * diameter)) };
        const auto centre = (float) (diameter - 1) * 0.5f, edge = (float) diameter * 0.5f;

        for (int y = 0; y < diameter; ++y)
        {
            for (int x = 0; x < diameter; ++x)
            {
                const auto distance = std::hypot ((float) x - centre, (float) y - centre) / edge;
                const auto core = jmax (0.0f, 1.0f - distance * 4.0f);
                const auto halo = std::pow (jmax (0.0f, 1.0f - distance), 2.2f) * 0.55f;
                dot.light[(size_t) (y * diameter + x)] = (uint8) jlimit (0, 255, roundToInt (255.0f * jmin (1.0f, core + halo)));
            }
        }

        dots.push_back (std::move (dot));
    }
}

void SoundVisualiser::paint (Graphics& g)
{
    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();

    if (! frame.isValid() || scale != pixelScale)
        prepare (scale);

    if (! frame.isValid())
        return;

    drawFrame();
    g.drawImageTransformed (frame, AffineTransform::scale (1.0f / pixelScale));

    // A ring rippling out from each note, fading as it goes: quick to draw, as it's one shape
    if (burst > 0.02f)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto grown = 1.0f - burst;
        const auto ring = Rectangle<float> (bounds.getWidth() * (0.2f + 0.75f * grown), bounds.getHeight() * (0.25f + 0.85f * grown));

        g.setColour (findColour (AppColourIds::signalPath).withAlpha (burst * 0.5f));
        g.drawEllipse (ring.withCentre (bounds.getCentre()), 1.0f + 2.5f * burst);
    }
}

void SoundVisualiser::drawFrame()
{
    const int width = frame.getWidth(), height = frame.getHeight();
    const auto background = findColour (ResizableWindow::backgroundColourId);
    const auto accent = findColour (AppColourIds::signalPath);

    Image::BitmapData pixels (frame, Image::BitmapData::writeOnly);

    // The background, with the haze added: brighter the louder it is
    const auto hazeLevel = jlimit (0.0f, 1.0f, 0.16f + 0.55f * loudness + 0.45f * burst) * 0.42f;
    const int hazeR = roundToInt (accent.getRed() * hazeLevel), hazeG = roundToInt (accent.getGreen() * hazeLevel),
              hazeB = roundToInt (accent.getBlue() * hazeLevel);
    const int backR = background.getRed(), backG = background.getGreen(), backB = background.getBlue();

    for (int y = 0; y < height; ++y)
    {
        auto* pixel = pixels.getLinePointer (y);
        const auto* hazeLine = haze.data() + (size_t) (y * width);

        for (int x = 0; x < width; ++x, pixel += pixels.pixelStride)
        {
            const int h = hazeLine[x];
            pixel[PixelRGB::indexR] = (uint8) jmin (255, backR + ((hazeR * h) >> 8));
            pixel[PixelRGB::indexG] = (uint8) jmin (255, backG + ((hazeG * h) >> 8));
            pixel[PixelRGB::indexB] = (uint8) jmin (255, backB + ((hazeB * h) >> 8));
        }
    }

    // The particles' light, added on top
    const auto centreX = (float) width * 0.5f, centreY = (float) height * 0.5f;
    const auto across = (float) width * 0.40f, down = (float) height * 0.40f;

    // It breathes slowly while it's quiet, and swells with the bass and each burst
    const auto breath = 0.04f * std::sin ((float) time * 0.9f);
    const auto spread = 1.0f + breath + 0.30f * bass + 0.45f * burst;

    for (const auto& p : particles)
    {
        const auto radius = p.radius * spread * (1.0f + 0.35f * p.depth * burst);
        const auto x = centreX + std::cos (p.angle) * radius * across;
        const auto y = centreY + std::sin (p.angle) * radius * down;

        // Brighter towards the middle and the louder it is, twinkling faster with the highs
        const auto sparkle = 0.5f + 0.5f * std::sin (p.twinkle + (float) time * (1.5f + 9.0f * highs));
        const auto brightness = jlimit (0.0f, 1.0f, (0.20f + 0.65f * loudness + 0.45f * burst)
                                                        * (0.45f + 0.55f * sparkle) * (1.25f - 0.55f * p.radius));

        // The deep ones go white with the highs
        const auto colour = accent.interpolatedWith (Colours::white, jlimit (0.0f, 0.85f, p.depth * (0.25f + highs) * brightness * 1.4f));
        const int r = roundToInt (colour.getRed() * brightness), g = roundToInt (colour.getGreen() * brightness),
                  b = roundToInt (colour.getBlue() * brightness);

        // The dot that fits its size, which grows with the bass and the bursts
        const auto diameter = p.size * 5.0f * pixelScale * (1.0f + 0.7f * bass + 0.5f * burst);
        const auto* dot = &dots.back();

        for (const auto& candidate : dots)
        {
            if ((float) candidate.diameter >= diameter)
            {
                dot = &candidate;
                break;
            }
        }

        const int left = roundToInt (x) - dot->diameter / 2, top = roundToInt (y) - dot->diameter / 2;

        for (int row = jmax (0, -top); row < dot->diameter && top + row < height; ++row)
        {
            auto* pixel = pixels.getPixelPointer (jmax (0, left), top + row);
            const auto* light = dot->light.data() + (size_t) (row * dot->diameter);

            for (int column = jmax (0, -left); column < dot->diameter && left + column < width; ++column, pixel += pixels.pixelStride)
            {
                const int l = light[column];
                pixel[PixelRGB::indexR] = (uint8) jmin (255, pixel[PixelRGB::indexR] + ((r * l) >> 8));
                pixel[PixelRGB::indexG] = (uint8) jmin (255, pixel[PixelRGB::indexG] + ((g * l) >> 8));
                pixel[PixelRGB::indexB] = (uint8) jmin (255, pixel[PixelRGB::indexB] + ((b * l) >> 8));
            }
        }
    }
}
