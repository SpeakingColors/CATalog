#include "CATalog.h"

CATalogProcessor::CATalogProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();      // wav, aiff, flac, ogg, mp3 (JUCE_USE_MP3AUDIOFORMAT), plus m4a / aac / wma where the OS decoder exists
    loadSettings();
}
CATalogProcessor::~CATalogProcessor() { transport.setSource(nullptr); }

void CATalogProcessor::prepareToPlay(double sr, int maxBlock) { transport.prepareToPlay(maxBlock, sr); }

void CATalogProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    juce::AudioSourceChannelInfo info(&buf, 0, buf.getNumSamples());
    transport.getNextAudioBlock(info);
}

juce::File CATalogProcessor::metaFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Studio87").getChildFile("CATalog").getChildFile("meta.txt");
}
void CATalogProcessor::saveMeta() const { auto f = metaFile(); f.getParentDirectory().createDirectory(); library.saveMeta(f.getFullPathName().toStdString()); }

juce::File CATalogProcessor::settingsFile() const { return metaFile().getSiblingFile("settings.txt"); }
void CATalogProcessor::saveSettings() const
{
    juce::String t; for (auto& r : library.roots) t << "root\t" << juce::String(r) << "\n"; if (watchFolder != juce::File()) t << "watch\t" << watchFolder.getFullPathName() << "\n";
    auto f = settingsFile(); f.getParentDirectory().createDirectory(); f.replaceWithText(t);
}
void CATalogProcessor::loadSettings()
{
    juce::StringArray lines; lines.addLines(settingsFile().loadFileAsString());
    for (auto& l : lines) { const auto tab = l.indexOfChar('\t'); if (tab < 0) continue; const auto k = l.substring(0, tab); const juce::File f(l.substring(tab + 1));
        if (!f.isDirectory()) continue; if (k == "root") library.importFolder(f.getFullPathName().toStdString()); else if (k == "watch") { watchFolder = f; watchSig = catalog::Library::folderSignature(f.getFullPathName().toStdString()); } }
    if (!library.roots.empty()) library.loadMeta(metaFile().getFullPathName().toStdString());
}
int CATalogProcessor::scanFolder(const juce::File& dir)
{
    libraryFolder = dir;
    const int n = library.scan(dir.getFullPathName().toStdString());
    library.loadMeta(metaFile().getFullPathName().toStdString()); saveSettings();
    return n;
}
int CATalogProcessor::importFolder(const juce::File& dir)
{
    libraryFolder = dir; const int n = library.importFolder(dir.getFullPathName().toStdString());     // recursive: every subfolder is read
    library.loadMeta(metaFile().getFullPathName().toStdString()); saveSettings(); return n;
}
void CATalogProcessor::setWatchFolder(const juce::File& dir)
{
    watchFolder = dir; if (!dir.isDirectory()) return;
    importFolder(dir); watchSig = catalog::Library::folderSignature(dir.getFullPathName().toStdString()); saveSettings();
}
bool CATalogProcessor::pollWatch()
{
    if (!watchFolder.isDirectory()) return false;
    const auto sig = catalog::Library::folderSignature(watchFolder.getFullPathName().toStdString()); if (sig == watchSig) return false;
    watchSig = sig; library.rescanAll(); library.loadMeta(metaFile().getFullPathName().toStdString()); return true;
}
int CATalogProcessor::analysePending(int maxFiles)
{
    int done = 0;
    for (auto& s : library.samples)
    {
        if (s.analysed) continue; if (done >= maxFiles) break; ++done;
        std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(juce::File(s.path)));
        if (r == nullptr || r->lengthInSamples <= 0 || r->sampleRate <= 0) { s.analysed = true; continue; }          // unreadable: do not retry every time
        const int n = (int) std::min<juce::int64>(r->lengthInSamples, (juce::int64)(r->sampleRate * 40.0));         // the first 40 seconds are enough
        juce::AudioBuffer<float> buf((int) r->numChannels, n); r->read(&buf, 0, n, 0, true, true);
        std::vector<float> mono((size_t) n, 0.f);
        for (int c = 0; c < buf.getNumChannels(); ++c) { const float* p = buf.getReadPointer(c); for (int i = 0; i < n; ++i) mono[(size_t) i] += p[i] / (float) buf.getNumChannels(); }
        auto info = catalog::analyse(mono, r->sampleRate); info.duration = (double) r->lengthInSamples / r->sampleRate;
        catalog::Library::applyAnalysis(s, info);
    }
    return done;
}
bool CATalogProcessor::openSplice() const
{
   #if JUCE_WINDOWS
    for (auto base : { juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getParentDirectory().getChildFile("Local/Programs/Splice/Splice.exe"), juce::File("C:/Program Files/Splice/Splice.exe") }) if (base.existsAsFile()) return base.startAsProcess();
   #elif JUCE_MAC
    if (juce::File("/Applications/Splice.app").exists()) return juce::File("/Applications/Splice.app").startAsProcess();
   #endif
    return juce::URL("https://splice.com").launchInDefaultBrowser();          // not installed: open the site; you sign in there yourself
}

void CATalogProcessor::audition(const catalog::Sample& s)
{
    transport.stop(); transport.setSource(nullptr);
    std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(juce::File(s.path)));
    if (r == nullptr) return;
    const double sr = r->sampleRate;
    source = std::make_unique<juce::AudioFormatReaderSource>(r.release(), true);
    transport.setSource(source.get(), 0, nullptr, sr);
    source->setLooping(loopAudition);
    transport.setPosition(0.0); transport.start();
}


void CATalogProcessor::getStateInformation(juce::MemoryBlock& dest) { juce::MemoryOutputStream m(dest, false); m.writeString(libraryFolder.getFullPathName()); }
void CATalogProcessor::setStateInformation(const void* d, int n)
{
    juce::MemoryInputStream m(d, (size_t) n, false); const juce::File f(m.readString());
    if (f.isDirectory()) importFolder(f);          // adds it (nothing happens if it is already there)
}
