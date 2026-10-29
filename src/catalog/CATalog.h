// CATalog: the app / plugin. The library model is in src/core. The audio side is only the audition player; the editor is a plain
// editor is in CATalogEditor.cpp (cat-ui window from docs/mockup/catalog.html).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../core/Library.h"
#include <memory>
#include <array>
#include <atomic>

class CATalogProcessor : public juce::AudioProcessor
{
public:
    CATalogProcessor();
    ~CATalogProcessor() override;
    const juce::String getName() const override { return "CATalog"; }
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override { resampler.releaseResources(); transport.releaseResources(); }
    bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // message thread
    int scanFolder(const juce::File& dir);                 // replaces the library with this folder (and all its subfolders)
    int importFolder(const juce::File& dir);               // adds a folder and all its subfolders; returns new samples
    void setWatchFolder(const juce::File& dir);            // e.g. the folder Splice saves into; polled by pollWatch()
    bool pollWatch();                                      // true if the folder changed and the library was refreshed
    int analysePending(int maxFiles = 40);                 // decodes and analyses samples not yet analysed (any format JUCE can read)
    bool openSplice() const;                               // opens the Splice desktop app if installed; never handles a login
    void saveSettings() const;
    void audition(const catalog::Sample& s);
    void stopAudition() { transport.stop(); }
    void setAuditionLoop(bool b) { loopAudition = b; if (source) source->setLooping(b); }
    bool auditionPlaying() const { return transport.isPlaying(); }
    double auditionSeconds() const { return transport.getCurrentPosition(); }
    void replayAudition() { transport.setPosition(0.0); transport.start(); }
    // preview controls (any thread). Pitch and tempo sync work like tape: speed and pitch move together.
    void setPitchSemitones(float s) { pitchSt = s; }
    void setVolumeDb(float d) { volDb = d; }
    void setSync(bool b) { syncOn = b; }
    void setHostBpm(double b) { hostBpm = (float) b; }
    void setSampleBpm(double b) { sampleBpm = (float) b; }
    double hostBpmSeen() const { return seenBpm.load(); }          // tempo reported by the host, 0 if none
    float levelL() const { return peakL.load(); } float levelR() const { return peakR.load(); }
    void copyRecent(float* dst, int n) const;                       // last n output samples (mono), for the spectrum
    double currentRate() const { return rate.load(); }
    void saveMeta() const;
    catalog::Library library;
    juce::File libraryFolder, watchFolder; std::string watchSig;

private:
    juce::File metaFile() const; juce::File settingsFile() const; void loadSettings();
    bool loopAudition = false;
    juce::ResamplingAudioSource resampler { &transport, false, 2 };
    std::atomic<float> pitchSt { 0.f }, volDb { 0.f }, hostBpm { 128.f }, sampleBpm { 0.f }, peakL { 0.f }, peakR { 0.f }; std::atomic<bool> syncOn { false };
    std::atomic<double> seenBpm { 0.0 }, rate { 1.0 };
    static constexpr int ringSize = 4096; std::array<std::atomic<float>, ringSize> ring {}; std::atomic<int> ringPos { 0 };
    juce::AudioFormatManager formats; juce::AudioTransportSource transport; std::unique_ptr<juce::AudioFormatReaderSource> source;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CATalogProcessor)
};
