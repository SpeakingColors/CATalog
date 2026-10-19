#include "CATalog.h"

CATalogProcessor::CATalogProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();
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

int CATalogProcessor::scanFolder(const juce::File& dir)
{
    libraryFolder = dir;
    const int n = library.scan(dir.getFullPathName().toStdString());
    library.loadMeta(metaFile().getFullPathName().toStdString());
    return n;
}

void CATalogProcessor::audition(const catalog::Sample& s)
{
    transport.stop(); transport.setSource(nullptr);
    std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(juce::File(s.path)));
    if (r == nullptr) return;
    const double sr = r->sampleRate;
    source = std::make_unique<juce::AudioFormatReaderSource>(r.release(), true);
    transport.setSource(source.get(), 0, nullptr, sr);
    transport.setPosition(0.0); transport.start();
}

namespace {
class CATalogEditor : public juce::AudioProcessorEditor, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    explicit CATalogEditor(CATalogProcessor& p) : AudioProcessorEditor(p), proc(p)
    {
        addAndMakeVisible(choose); addAndMakeVisible(search); addAndMakeVisible(list); addAndMakeVisible(info);
        choose.setButtonText("Choose sample folder..."); search.setTextToShowWhenEmpty("Search: words in name, tag, key, folder", juce::Colours::grey); search.addListener(this);
        list.setModel(this); list.setRowHeight(22);
        choose.onClick = [this] {
            chooser = std::make_unique<juce::FileChooser>("Pick your sample folder");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& c) {
                if (c.getResult().isDirectory()) { proc.scanFolder(c.getResult()); refresh(); } });
        };
        setSize(720, 520); refresh();
    }
    void resized() override { auto r = getLocalBounds().reduced(8); auto top = r.removeFromTop(28); choose.setBounds(top.removeFromLeft(200)); top.removeFromLeft(6); search.setBounds(top); r.removeFromTop(6); info.setBounds(r.removeFromBottom(22)); list.setBounds(r); }
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (row < 0 || row >= (int) rows.size()) return; const auto& s = *rows[(size_t) row];
        if (sel) g.fillAll(juce::Colours::darkslategrey); g.setColour(juce::Colours::white); g.setFont(14.f);
        g.drawText(juce::String(s.fav ? juce::CharPointer_UTF8("\xe2\x98\x85 ") : "  ") + s.name, 6, 0, w - 330, h, juce::Justification::centredLeft, true);
        g.setColour(juce::Colours::lightgrey);
        g.drawText((s.bpm > 0 ? juce::String((int) s.bpm) + " bpm" : juce::String()), w - 320, 0, 70, h, juce::Justification::centredLeft);
        g.drawText(s.key, w - 245, 0, 50, h, juce::Justification::centredLeft);
        juce::String t; for (auto& x : s.tags) t << (t.isEmpty() ? "" : ", ") << x; g.drawText(t, w - 190, 0, 184, h, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged(int row) override { if (row >= 0 && row < (int) rows.size()) proc.audition(*rows[(size_t) row]); }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { if (row >= 0 && row < (int) rows.size()) { auto* s = const_cast<catalog::Sample*>(rows[(size_t) row]); s->fav = !s->fav; proc.saveMeta(); list.repaintRow(row); } }
    void textEditorTextChanged(juce::TextEditor&) override { refresh(); }
private:
    void refresh()
    {
        catalog::Query q; q.text = search.getText().toStdString(); rows = proc.library.find(q); list.updateContent(); list.repaint();
        info.setText(juce::String((int) rows.size()) + " of " + juce::String((int) proc.library.samples.size()) + " samples. Click to listen, double click for favourite.", juce::dontSendNotification);
    }
    CATalogProcessor& proc; juce::TextButton choose; juce::TextEditor search; juce::ListBox list; juce::Label info;
    std::vector<const catalog::Sample*> rows; std::unique_ptr<juce::FileChooser> chooser;
};
}
juce::AudioProcessorEditor* CATalogProcessor::createEditor() { return new CATalogEditor(*this); }

void CATalogProcessor::getStateInformation(juce::MemoryBlock& dest) { juce::MemoryOutputStream m(dest, false); m.writeString(libraryFolder.getFullPathName()); }
void CATalogProcessor::setStateInformation(const void* d, int n)
{
    juce::MemoryInputStream m(d, (size_t) n, false); const juce::File f(m.readString());
    if (f.isDirectory()) scanFolder(f);
}
