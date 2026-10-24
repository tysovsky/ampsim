#include "Recorder.h"
#include "../UI/MainHostWindow.h"

Recorder::Recorder (AudioIODeviceCallback& p) : player (p)
{
    formats.registerBasicFormats();
    writerThread.startThread();
    readAheadThread.startThread();
}

Recorder::~Recorder()
{
    stop();
    unloadPlayback();
    readAheadThread.stopThread (1000);
    writerThread.stopThread (1000);
}

File Recorder::getRecordingsFolder()
{
    return getAppProperties().getUserSettings()->getFile().getSiblingFile ("Recordings");
}

//==============================================================================
bool Recorder::start (const std::vector<Output>& outputs)
{
    stop();

    const auto rate = sampleRate.load();
    const int numOutputs = numDeviceOutputs.load();

    if (rate <= 0.0)
        return false;

    const auto folder = getRecordingsFolder();
    folder.createDirectory();

    // Windows doesn't allow colons in file names
    const auto startTime = Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    std::vector<std::unique_ptr<Take>> newTakes;

    for (auto& output : outputs)
    {
        if (output.first >= numOutputs || output.second >= numOutputs)
            continue;

        const auto file = folder.getChildFile (startTime + " " + File::createLegalFileName (output.name) + ".wav").getNonexistentSibling();
        std::unique_ptr<OutputStream> stream (file.createOutputStream());

        if (stream == nullptr)
            continue;

        const auto numChannels = (unsigned int) (output.second >= 0 ? 2 : 1);
        std::unique_ptr<AudioFormatWriter> writer (WavAudioFormat().createWriterFor (stream.get(), rate, numChannels, 24, {}, 0));

        if (writer == nullptr)
            continue;

        // The writer has the stream now
        stream.release();

        auto take = std::make_unique<Take>();
        take->writer = std::make_unique<AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 32768);
        take->first = output.first;
        take->second = output.second;
        newTakes.push_back (std::move (take));
    }

    if (newTakes.empty())
        return false;

    samplesRecorded = 0;

    const ScopedLock sl (takesLock);
    takes = std::move (newTakes);
    recording = true;
    return true;
}

void Recorder::stop()
{
    std::vector<std::unique_ptr<Take>> finished;

    {
        const ScopedLock sl (takesLock);
        std::swap (finished, takes);
        recording = false;
    }

    // Deleting the writers writes what's still waiting and finishes the files, which isn't done
    // while holding the lock the audio thread takes
    finished.clear();
}

double Recorder::getRecordingLength() const noexcept
{
    const auto rate = sampleRate.load();
    return rate > 0.0 ? (double) samplesRecorded.load() / rate : 0.0;
}

//==============================================================================
bool Recorder::loadForPlayback (const File& file)
{
    unloadPlayback();

    if (auto* reader = formats.createReaderFor (file))
    {
        playbackSource = std::make_unique<AudioFormatReaderSource> (reader, true);

        // Resampled to the device's rate if it's changed since, and up to two channels
        playback.setSource (playbackSource.get(), 32768, &readAheadThread, reader->sampleRate, 2);
        return true;
    }

    return false;
}

void Recorder::unloadPlayback()
{
    playback.stop();
    playback.setSource (nullptr);
    playbackSource.reset();
}

//==============================================================================
void Recorder::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                 float* const* outputChannelData, int numOutputChannels,
                                                 int numSamples, const AudioIODeviceCallbackContext& context)
{
    player.audioDeviceIOCallbackWithContext (inputChannelData, numInputChannels, outputChannelData,
                                             numOutputChannels, numSamples, context);

    // What the graph's sent to the outputs. If recording is just starting or stopping, this block
    // is skipped rather than waiting.
    {
        const ScopedTryLock sl (takesLock);

        if (sl.isLocked() && ! takes.empty())
        {
            for (auto& take : takes)
            {
                if (take->first >= numOutputChannels || take->second >= numOutputChannels)
                    continue;

                const float* channels[] = { outputChannelData[take->first],
                                            take->second >= 0 ? outputChannelData[take->second] : nullptr };
                take->writer->write (channels, numSamples);
            }

            samplesRecorded += numSamples;
        }
    }

    // A recording being played back goes on top, after the outputs have been recorded
    if (numOutputChannels > 0 && playbackBuffer.getNumSamples() >= numSamples)
    {
        AudioSourceChannelInfo info (&playbackBuffer, 0, numSamples);
        playback.getNextAudioBlock (info);

        for (int channel = 0; channel < jmin (2, numOutputChannels); ++channel)
            FloatVectorOperations::add (outputChannelData[channel], playbackBuffer.getReadPointer (channel), numSamples);
    }
}

void Recorder::audioDeviceAboutToStart (AudioIODevice* device)
{
    player.audioDeviceAboutToStart (device);

    // A recording can't carry on through a change of device, sample rate or channels
    if (recording)
        stopSoon();

    sampleRate = device->getCurrentSampleRate();
    numDeviceOutputs = device->getActiveOutputChannels().countNumberOfSetBits();

    // Room for blocks bigger than the device says, which some drivers send
    const int blockSize = device->getCurrentBufferSizeSamples();
    playbackBuffer.setSize (2, jmax (blockSize, 4096));
    playback.prepareToPlay (blockSize, sampleRate);
}

void Recorder::audioDeviceStopped()
{
    player.audioDeviceStopped();

    if (recording)
        stopSoon();

    sampleRate = 0.0;
    playback.releaseResources();
}

void Recorder::stopSoon()
{
    // The device can call from its own thread, and the files are finished on the message thread
    MessageManager::callAsync ([weakThis = WeakReference<Recorder> (this)]
    {
        if (weakThis != nullptr)
            weakThis->stop();
    });
}

void Recorder::audioDeviceError (const String& errorMessage)
{
    player.audioDeviceError (errorMessage);
}
