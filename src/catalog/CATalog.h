// CATalog: the app / plugin. The library model is in src/core. The audio side is only the audition player; the editor is a plain
// JUCE list for now (folder button, search, click to audition). The cat-ui editor from docs/mockup is on the ROADMAP.
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
    int scanFolder(const juce::File& dir);
    void audition(const catalog::Sample& s);
    void stopAudition() { transport.stop(); }
    void saveMeta() const;
    catalog::Library library;
    juce::File libraryFolder;

private:
    juce::File metaFile() const;
    juce::AudioFormatManager formats; juce::AudioTransportSource transport; std::unique_ptr<juce::AudioFormatReaderSource> source;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CATalogProcessor)
};
