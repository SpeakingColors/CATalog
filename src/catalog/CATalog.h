// CATalog: the app / plugin. The library model is in src/core. The audio side is only the audition player; the editor is a plain
// editor is in CATalogEditor.cpp (cat-ui window from docs/mockup/catalog.html).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../core/Library.h"
#include <memory>

class CATalogProcessor : public juce::AudioProcessor
{
public:
    CATalogProcessor();
    ~CATalogProcessor() override;
    const juce::String getName() const override { return "CATalog"; }
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override { transport.releaseResources(); }
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
    void saveMeta() const;
    catalog::Library library;
    juce::File libraryFolder, watchFolder; std::string watchSig;

private:
    juce::File metaFile() const; juce::File settingsFile() const; void loadSettings();
    bool loopAudition = false;
    juce::AudioFormatManager formats; juce::AudioTransportSource transport; std::unique_ptr<juce::AudioFormatReaderSource> source;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CATalogProcessor)
};
