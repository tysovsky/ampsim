#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Sits between the audio device and the graph's player. The graph plays as
    before, and while recording, what it sends to each output in use is written to
    a WAV file of its own. Recordings can also be played back, on the first two
    outputs, on top of whatever the graph is playing.

    Recordings go in getRecordingsFolder(), a file for each output for each time
    recording is started, named by when it started and which output it's of, e.g.
    "2026-10-23 21-30-05 Output 1+2.wav". They're 24-bit, like the audio
    interfaces the host is used with.
*/
class Recorder final : public AudioIODeviceCallback
{
public:
    explicit Recorder (AudioIODeviceCallback& player);
    ~Recorder() override;

    /** An output to record: one of the device's output channels, or a pair, counting from 0. */
    struct Output
    {
        int first = 0, second = -1;     // second is -1 for a single channel
        String name;                    // what the file is named after, e.g. "Output 1+2" or "Main mix"
    };

    /** Starts recording the outputs that the device has. Returns false if it has none of them,
        or isn't running.
    */
    bool start (const std::vector<Output>&);

    /** Stops recording, and finishes the files. */
    void stop();

    bool isRecording() const noexcept                   { return recording; }

    /** How long the current recording has been going, in seconds. */
    double getRecordingLength() const noexcept;

    /** Where recordings are kept: next to the app's settings. */
    static File getRecordingsFolder();

    //==============================================================================
    /** Gets a recording ready to play back, or returns false if it can't be read. */
    bool loadForPlayback (const File&);

    /** Stops playing back and lets go of the recording. */
    void unloadPlayback();

    /** The transport for the recording loaded for playback, to start, stop and position it. */
    AudioTransportSource& getPlayback() noexcept        { return playback; }

    //==============================================================================
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const String& errorMessage) override;

private:
    struct Take
    {
        std::unique_ptr<AudioFormatWriter::ThreadedWriter> writer;
        int first = 0, second = -1;
    };

    void stopSoon();

    AudioIODeviceCallback& player;

    TimeSliceThread writerThread { "Recording" };
    CriticalSection takesLock;
    std::vector<std::unique_ptr<Take>> takes;   // one for each output being recorded
    std::atomic<bool> recording { false };
    std::atomic<int64> samplesRecorded { 0 };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> numDeviceOutputs { 0 };

    AudioFormatManager formats;
    TimeSliceThread readAheadThread { "Playback" };
    std::unique_ptr<AudioFormatReaderSource> playbackSource;
    AudioTransportSource playback;
    AudioBuffer<float> playbackBuffer;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Recorder)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recorder)
};
