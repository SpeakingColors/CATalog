// CATalog window, built from docs/mockup/catalog.html: header, three tabs (Library, Stems, Splice), folder and tag lists, type chips, search
// and filters, the sample table, and the preview panel (waveform, spectrum, play, loop, auto-play, pitch, volume, tempo sync, level meter,
// type, tags, favourite, rating, drag to the DAW). Fixed aspect ratio, resizable from every corner and edge, wood cheeks.
// Not built yet (see ROADMAP): send to SupurrSaw, saved searches (the preset bar), the Settings button, Add files.
#include "CATalog.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>
#include <cmath>
#include <map>
#include <set>
#include <array>
#include <juce_dsp/juce_dsp.h>

namespace {
using juce::Colour; using juce::Graphics; using juce::String;
const Colour kBg(0xff101114), kPanel(0xff15161a), kEdge(0xff2c2e35), kText(0xffe8e6e1), kDim(0xff8c909a), kGold(0xffffd100), kGreen(0xff5fe08c);
constexpr int kCheek = 30, kContentW = 1140, kBaseW = kContentW + 2 * kCheek, kBaseH = 780, kBodyY = 40;
juce::Font mono(float h, bool bold = false) { return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Font sans(float h, bool bold = false) { return juce::Font(juce::FontOptions(h, bold ? juce::Font::bold : juce::Font::plain)); }

class Pill : public juce::Button
{
public:
    explicit Pill(const String& t) : juce::Button(t) {}
    void paintButton(Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced(0.5f); const bool on = getToggleState();
        g.setColour(on ? kGreen.withAlpha(0.16f) : (over ? Colour(0xff25272e) : Colour(0xff1b1c21))); g.fillRoundedRectangle(r, 3.f);
        g.setColour(on ? kGreen : Colour(0xff3a3d46)); g.drawRoundedRectangle(r, 3.f, on ? 1.5f : 1.f);
        g.setColour(on ? kGreen : (down ? kGold : kText)); g.setFont(sans(13.f, true)); g.drawFittedText(getButtonText().toUpperCase(), getLocalBounds(), juce::Justification::centred, 1);
    }
};

// A wrapping row of small rounded chips. Used for the type filter and for the tags on the selected sample.
class ChipBar : public juce::Component
{
public:
    struct Chip { String label, value; bool on = false; };
    std::vector<Chip> chips; std::function<void(const String&)> onClick;
    int heightFor(int w) { return layoutChips(w, false); }
    void paint(Graphics& g) override
    {
        layoutChips(getWidth(), true);
        for (size_t i = 0; i < chips.size(); ++i)
        {
            auto r = rects[i].toFloat(); g.setColour(chips[i].on ? kGreen.withAlpha(0.18f) : Colour(0xff1b1c21)); g.fillRoundedRectangle(r, r.getHeight() / 2);
            g.setColour(chips[i].on ? kGreen : Colour(0xff3a3d46)); g.drawRoundedRectangle(r.reduced(0.5f), r.getHeight() / 2, 1.f);
            g.setColour(chips[i].on ? kGreen : kText); g.setFont(sans(12.f)); g.drawText(chips[i].label, rects[i], juce::Justification::centred);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override { for (size_t i = 0; i < rects.size(); ++i) if (rects[i].contains(e.getPosition()) && onClick) { onClick(chips[i].value); return; } }
private:
    std::vector<juce::Rectangle<int>> rects;
    int layoutChips(int w, bool store)
    {
        int x = 0, y = 0; const int h = 22, gap = 5; if (store) rects.clear(); juce::Font f = sans(12.f);
        for (auto& c : chips) { const int cw = juce::roundToInt(juce::GlyphArrangement::getStringWidth(f, c.label)) + 18; if (x > 0 && x + cw > w) { x = 0; y += h + gap; } if (store) rects.push_back({ x, y, cw, h }); x += cw + gap; }
        return y + h;
    }
};

class WaveView : public juce::Component
{
public:
    std::vector<float> peaks; double seconds = 0, pos = -1;
    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff060608)); g.setColour(kEdge); g.drawRect(getLocalBounds(), 1);
        if (peaks.empty()) { g.setColour(kDim); g.setFont(sans(13.f)); g.drawText("Select a sample", getLocalBounds(), juce::Justification::centred); return; }
        const float mid = getHeight() * 0.5f, amp = getHeight() * 0.5f - 4; const int n = (int) peaks.size(), w = getWidth() - 2;
        g.setColour(kGreen);
        for (int x = 0; x < w; ++x) { const float h = std::max(1.f, std::min(1.f, peaks[(size_t) (x * n / w)]) * amp); g.fillRect((float) x + 1, mid - h, 1.f, 2 * h); }
        if (pos >= 0 && seconds > 0) { g.setColour(kGold); g.fillRect(1.f + (float) juce::jlimit(0.0, 1.0, pos / seconds) * (float) w, 0.f, 1.f, (float) getHeight()); }
    }
};

// Drag this onto the DAW (or a file manager): starts an operating system file drag of the selected sample.
class DragPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::File()> fileToDrag;
    void paint(Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.f); g.setColour(kGreen.withAlpha(0.07f)); g.fillRoundedRectangle(r, 3.f);
        const float dash[] = { 5.f, 4.f }; juce::Path p; p.addRoundedRectangle(r, 3.f); juce::PathStrokeType(1.f).createDashedStroke(p, p, dash, 2);
        g.setColour(kGreen); g.fillPath(p); g.setFont(sans(14.f, true)); g.drawText("DRAG TO DAW", getLocalBounds(), juce::Justification::centred);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 6 || fileToDrag == nullptr) return; const auto f = fileToDrag();
        if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
    }
};

// Spectrum of what is playing: last 2048 output samples, log frequency axis, dB scale.
class SpectrumView : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void(float*, int)> source;
    void tickUpdate()
    {
        if (!source) return; std::array<float, 2048> in {}; source(in.data(), 2048);
        std::array<float, 4096> buf {}; for (int i = 0; i < 2048; ++i) buf[(size_t) i] = in[(size_t) i];
        win.multiplyWithWindowingTable(buf.data(), 2048); fft.performFrequencyOnlyForwardTransform(buf.data());
        const double nyq = fs * 0.5; mags.assign((size_t) std::max(1, getWidth()), -100.f);
        for (int x = 0; x < getWidth(); ++x)
        {
            const double f0 = 30.0 * std::pow(16000.0 / 30.0, (double) x / std::max(1, getWidth())), f1 = 30.0 * std::pow(16000.0 / 30.0, (double) (x + 1) / std::max(1, getWidth()));
            const int b0 = juce::jlimit(1, 1023, (int) (f0 / nyq * 1024.0)), b1 = juce::jlimit(b0, 1023, (int) (f1 / nyq * 1024.0)); float m = 0.f;
            for (int b = b0; b <= b1; ++b) m = std::max(m, buf[(size_t) b]);
            mags[(size_t) x] = juce::Decibels::gainToDecibels(m / 512.f, -100.f);
        }
        repaint();
    }
    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff060608)); g.setColour(kEdge); g.drawRect(getLocalBounds(), 1);
        auto db2y = [this](float db) { return (float) getHeight() - 14.f - juce::jlimit(0.f, 1.f, (db + 80.f) / 80.f) * ((float) getHeight() - 18.f); };
        g.setFont(mono(9.f)); for (float db : { -20.f, -40.f, -60.f }) { g.setColour(Colour(0xff1c1d22)); g.drawHorizontalLine((int) db2y(db), 1.f, (float) getWidth()); g.setColour(kDim); g.drawText(String((int) db), 3, (int) db2y(db) - 5, 24, 10, juce::Justification::left); }
        for (double f : { 100.0, 1000.0, 10000.0 }) { const float x = (float) (std::log(f / 30.0) / std::log(16000.0 / 30.0)) * (float) getWidth(); g.setColour(Colour(0xff1c1d22)); g.drawVerticalLine((int) x, 1.f, (float) getHeight() - 14.f); g.setColour(kDim); g.drawText(f >= 1000 ? String((int) (f / 1000)) + "k" : String((int) f), (int) x - 12, getHeight() - 13, 24, 11, juce::Justification::centred); }
        if (mags.empty()) return; juce::Path p; p.startNewSubPath(0.f, db2y(-80.f));
        for (size_t x = 0; x < mags.size(); ++x) p.lineTo((float) x, db2y(mags[x])); p.lineTo((float) mags.size(), db2y(-80.f)); p.closeSubPath();
        g.setColour(kGreen.withAlpha(0.35f)); g.fillPath(p); g.setColour(kGreen); g.strokePath(p, juce::PathStrokeType(1.f));
    }
    double fs = 44100.0;
private:
    juce::dsp::FFT fft { 11 }; juce::dsp::WindowingFunction<float> win { 2048, juce::dsp::WindowingFunction<float>::hann }; std::vector<float> mags;
};

class LevelView : public juce::Component, public juce::SettableTooltipClient
{
public:
    float l = 0.f, r = 0.f;
    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff060608)); g.setColour(kEdge); g.drawRect(getLocalBounds(), 1);
        auto bar = [&](int y, float v) { const float db = juce::Decibels::gainToDecibels(v, -60.f); const float w = juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f) * (float) (getWidth() - 24);
            g.setColour(Colour(0xff15161a)); g.fillRect(4, y, getWidth() - 24, 8); g.setColour(db > -3.f ? Colour(0xffff6b6b) : (db > -12.f ? kGold : kGreen)); g.fillRect(4.f, (float) y, w, 8.f); };
        bar(5, l); bar(17, r); g.setColour(kDim); g.setFont(mono(9.f)); g.drawText("L", getWidth() - 14, 3, 10, 10, juce::Justification::right); g.drawText("R", getWidth() - 14, 15, 10, 10, juce::Justification::right);
    }
};

// Stems tab: the stem files found next to the selected loop. A found stem plays when you click its card.
class StemsPane : public juce::Component
{
public:
    struct Found { String kind, file, rel; };
    std::function<void(const String&)> onPlay; String title; std::vector<Found> found;
    void paint(Graphics& g) override
    {
        g.setColour(kPanel); g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f); g.setColour(kEdge); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.f, 1.f);
        g.setColour(kGreen); g.setFont(sans(16.f, true)); g.drawText("STEMS" + (title.isEmpty() ? String() : "  |  " + title.toUpperCase()), 18, 14, getWidth() - 36, 22, juce::Justification::left, true);
        const String how = "CATalog looks in the same folder for files named like Loop_drums, Loop_bass, Loop_vocals, Loop_other (also \"Loop - drums\" or \"Loop (drums)\"), or in a folder called \"Loop stems\". It does not split audio into stems itself: put the stem files beside the loop, or export them from your stem splitter and import that folder.";
        String head = title.isEmpty() ? "Select a sample in the Library tab." : (found.empty() ? "No stem files found next to this sample." : String((int) found.size()) + " stem file(s) found next to this sample. Click a card to play it.");
        g.setColour(kText); g.setFont(sans(14.f, true)); g.drawFittedText(head, 18, 46, getWidth() - 36, 20, juce::Justification::left, 1);
        g.setFont(sans(13.f)); g.setColour(kDim); g.drawFittedText(how, 18, 72, getWidth() - 36, 62, juce::Justification::topLeft, 4);
        rects.clear(); std::vector<String> kinds { "drums", "bass", "vocals", "other" }; for (auto& f : found) if (std::find(kinds.begin(), kinds.end(), f.kind) == kinds.end()) kinds.push_back(f.kind);
        const int cw = (getWidth() - 36 - 3 * 10) / 4; int i = 0;
        for (auto& k : kinds)
        {
            const Found* hit = nullptr; for (auto& f : found) if (f.kind == k) { hit = &f; break; }
            juce::Rectangle<int> r(18 + (i % 4) * (cw + 10), 150 + (i / 4) * 70, cw, 58); ++i; rects.push_back({ r, hit ? hit->rel : String() });
            g.setColour(hit ? kGreen.withAlpha(0.12f) : Colour(0xff121317)); g.fillRoundedRectangle(r.toFloat(), 3.f); g.setColour(hit ? kGreen : kEdge); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 3.f, 1.f);
            g.setColour(hit ? kGreen : kDim); g.setFont(sans(13.f, true)); g.drawText(k.toUpperCase(), r.withHeight(28), juce::Justification::centred);
            g.setColour(hit ? kText : kDim.withAlpha(0.7f)); g.setFont(sans(12.f)); g.drawText(hit ? hit->file : String("missing"), r.withTrimmedTop(26).reduced(6, 0), juce::Justification::centred, true);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override { for (auto& r : rects) if (r.second.isNotEmpty() && r.first.contains(e.getPosition()) && onPlay) { onPlay(r.second); return; } }
private:
    std::vector<std::pair<juce::Rectangle<int>, String>> rects;
};

// Splice tab: no login on purpose. Choose the folder Splice saves into; CATalog watches it and imports new files.
class SplicePane : public juce::Component
{
public:
    Pill openB { "Open Splice" }, chooseB { "Choose Splice save folder" }, checkB { "Check now" };
    String status; std::vector<String> names;
    SplicePane() { addAndMakeVisible(openB); addAndMakeVisible(chooseB); addAndMakeVisible(checkB); }
    void resized() override { openB.setBounds(18, 188, 120, 34); chooseB.setBounds(146, 188, 230, 34); checkB.setBounds(384, 188, 120, 34); }
    void paint(Graphics& g) override
    {
        g.setColour(kPanel); g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f); g.setColour(kEdge); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.f, 1.f);
        g.setColour(kGreen); g.setFont(sans(16.f, true)); g.drawText("SPLICE", 18, 14, 300, 22, juce::Justification::left);
        g.setColour(kText); g.setFont(sans(14.f, true)); g.drawFittedText("No login here, on purpose.", 18, 44, getWidth() - 36, 20, juce::Justification::left, 1);
        g.setFont(sans(13.f)); g.setColour(kDim);
        g.drawFittedText("Splice has no public API for other apps, so CATalog cannot sign in to your account or browse its catalogue. You search and download in the Splice app or on splice.com; CATalog picks the files up from the folder Splice saves into.", 18, 66, getWidth() - 36, 50, juce::Justification::topLeft, 3);
        g.setColour(kText); g.drawFittedText("1. Open Splice and sign in there.\n2. Choose the folder Splice saves its downloads into (see the download location in the Splice app's settings).\n3. Choosing it starts watching: new files appear in the Library under that folder, analysed and sorted. The app and plugin watch while they are open and remember the folder.", 18, 112, getWidth() - 36, 70, juce::Justification::topLeft, 6);
        g.setColour(status.startsWith("Watching") ? kGreen : kDim); g.setFont(mono(12.f)); g.drawFittedText(status, 18, 232, getWidth() - 36, 18, juce::Justification::left, 1);
        g.setColour(kGreen); g.setFont(sans(13.f, true)); g.drawText("IMPORTED FROM THIS FOLDER", 18, 264, 300, 18, juce::Justification::left);
        g.setColour(Colour(0xff0c0d10)); g.fillRect(18, 288, getWidth() - 36, getHeight() - 306); g.setColour(kEdge); g.drawRect(18, 288, getWidth() - 36, getHeight() - 306, 1);
        g.setColour(kDim); g.setFont(sans(13.f)); const int rowsFit = (getHeight() - 306 - 8) / 20;
        if (names.empty()) g.drawText("Nothing imported yet.", 28, 292, getWidth() - 56, 20, juce::Justification::left);
        for (int i = 0; i < (int) names.size() && i < rowsFit; ++i) g.drawText(names[(size_t) i], 28, 292 + i * 20, getWidth() - 56, 20, juce::Justification::left, true);
        if ((int) names.size() > rowsFit) g.drawText("... and " + String((int) names.size() - rowsFit) + " more", 28, 292 + (rowsFit - 1) * 20, getWidth() - 56, 20, juce::Justification::left);
    }
};


class CATalogEditor : public juce::AudioProcessorEditor, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    explicit CATalogEditor(CATalogProcessor& p) : AudioProcessorEditor(p), proc(p), gate(*this)
    {
        setLookAndFeel(&laf); addAndMakeVisible(content); content.addAndMakeVisible(body); auto& c = body;
        for (auto* b : { &guide, &about }) content.addAndMakeVisible(*b);
        const char* tn[] = { "Library", "Stems", "Splice" };
        for (int i = 0; i < 3; ++i) { tabs[i] = std::make_unique<Pill>(tn[i]); content.addAndMakeVisible(*tabs[i]); tabs[i]->setRadioGroupId(11); tabs[i]->setClickingTogglesState(true); tabs[i]->onClick = [this, i] { setTab(i); }; }
        tabs[0]->setToggleState(true, juce::dontSendNotification);
        for (auto* b : { &importB, &playB, &loopB, &autoB, &syncB, &favB }) c.addAndMakeVisible(*b);
        for (auto* s : { &pitchS, &volS, &bpmS }) { c.addAndMakeVisible(*s); s->setSliderStyle(juce::Slider::RotaryVerticalDrag); s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); }
        c.addAndMakeVisible(spectrum); c.addAndMakeVisible(level); c.addAndMakeVisible(stemsPane); c.addAndMakeVisible(splicePane);
        spectrum.source = [this](float* d, int n) { proc.copyRecent(d, n); }; spectrum.setTooltip("What is playing right now: frequency from 30 Hz to 16 kHz, level in dB.");
        level.setTooltip("Output level, left and right.");
        pitchS.setRange(-12.0, 12.0, 1.0); pitchS.setValue(0.0); pitchS.setDoubleClickReturnValue(true, 0.0); pitchS.setTooltip("Pitch in semitones. Like tape: the sample also gets faster or slower.");
        volS.setRange(-24.0, 6.0, 0.1); volS.setValue(0.0); volS.setDoubleClickReturnValue(true, 0.0); volS.setTooltip("Preview volume in dB. Only for listening: it does not change the file you drag out.");
        bpmS.setRange(60.0, 200.0, 0.1); bpmS.setValue(128.0); bpmS.setDoubleClickReturnValue(true, 128.0); bpmS.setTooltip("Tempo to sync to. Follows the host tempo (plugin) until you turn this knob; double click to reset.");
        pitchS.onValueChange = [this] { proc.setPitchSemitones((float) pitchS.getValue()); body.repaint(); };
        volS.onValueChange = [this] { proc.setVolumeDb((float) volS.getValue()); body.repaint(); };
        bpmS.onValueChange = [this] { proc.setHostBpm(bpmS.getValue()); if (!settingBpmFromHost) bpmTouched = true; body.repaint(); };
        syncB.setClickingTogglesState(true); autoB.setClickingTogglesState(true); autoB.setToggleState(true, juce::dontSendNotification);
        syncB.setTooltip("Play the sample at the tempo on the knob (needs a BPM on the sample). Speed and pitch move together.");
        autoB.setTooltip("Play a sample as soon as you select it.");
        syncB.onClick = [this] { proc.setSync(syncB.getToggleState()); body.repaint(); };
        stemsPane.onPlay = [this](const String& rel) { if (auto* s = proc.library.byRel(rel.toStdString())) { proc.setSampleBpm(s->bpm); proc.audition(*s); } };
        splicePane.openB.onClick = [this] { proc.openSplice(); };
        splicePane.chooseB.onClick = [this] { pick("Pick the folder Splice saves into", [this](const juce::File& f) { proc.setWatchFolder(f); proc.analysePending(); refresh(); }); };
        splicePane.checkB.onClick = [this] { proc.pollWatch(); proc.analysePending(); refresh(); };
        splicePane.openB.setTooltip("Opens the Splice app (or splice.com). You sign in and download there."); splicePane.chooseB.setTooltip("Choose the folder Splice saves your downloads into. CATalog watches it and picks up new files. There is no Splice login here: Splice has no public API.");
        splicePane.checkB.setTooltip("Look at the folder now instead of waiting for the next automatic check.");
        for (auto* l : { &folderList, &tagList, &table }) c.addAndMakeVisible(*l);
        for (auto* x : { &search }) c.addAndMakeVisible(*x);
        for (auto* cb : { &genreBox, &keyBox, &sortBox, &typeBox }) c.addAndMakeVisible(*cb);
        c.addAndMakeVisible(favOnly); c.addAndMakeVisible(typeChips); c.addAndMakeVisible(wave); c.addAndMakeVisible(dragPad); c.addAndMakeVisible(selTags); c.addAndMakeVisible(addTag);
        favOnly.setClickingTogglesState(true); loopB.setClickingTogglesState(true);
        folderModel.o = this; tagModel.o = this; tagModel.tags = true;
        folderList.setModel(&folderModel); tagList.setModel(&tagModel); table.setModel(this);
        folderList.setRowHeight(22); tagList.setRowHeight(22); table.setRowHeight(26);
        for (auto* l : { &folderList, &tagList, &table }) { l->setColour(juce::ListBox::backgroundColourId, Colour(0xff0c0d10)); l->setColour(juce::ListBox::outlineColourId, kEdge); l->setOutlineThickness(1); }
        search.setTextToShowWhenEmpty("Search names, tags, type...", kDim); search.addListener(this); search.setColour(juce::TextEditor::backgroundColourId, Colour(0xff0c0d10));
        addTag.setTextToShowWhenEmpty("+ tag", kDim); addTag.setColour(juce::TextEditor::backgroundColourId, Colour(0xff0c0d10));
        addTag.onReturnKey = [this] { if (auto* s = selected()) { catalog::Library::addTag(*s, addTag.getText().toStdString()); proc.saveMeta(); addTag.clear(); refresh(); } };
        sortBox.addItemList({ "Name", "BPM", "Key", "Rating", "Size" }, 1); sortBox.setSelectedId(1, juce::dontSendNotification);
        for (auto* cb : { &genreBox, &keyBox, &sortBox }) cb->onChange = [this] { refresh(); };
        favOnly.onClick = [this] { refresh(); };
        typeBox.setTooltip("Fix the type of the selected sample. Your choice is kept in the library file and wins over the guess.");
        typeBox.onChange = [this] { if (auto* s = selected()) { catalog::Library::setType(*s, typeBox.getSelectedId() > 1 ? typeBox.getText().toStdString() : std::string()); proc.saveMeta(); juce::MessageManager::callAsync([sp = juce::Component::SafePointer<CATalogEditor>(this)] { if (sp != nullptr) sp->refresh(); }); } };
        importB.setTooltip("Adds a folder and everything in its subfolders. You can add as many as you like.");
        playB.setTooltip("Play the selected sample from the start."); loopB.setTooltip("Repeat the sample while it plays.");
        favB.setTooltip("Mark as favourite (double click a row does the same)."); dragPad.setTooltip("Drag the selected sample onto a track in your DAW.");
        importB.onClick = [this] { pick("Pick a sample folder (subfolders are included)", [this](const juce::File& f) { proc.importFolder(f); proc.analysePending(); refresh(); }); };
        playB.onClick = [this] { if (auto* s = selected()) { proc.setSampleBpm(s->bpm); proc.audition(*s); } };
        loopB.onClick = [this] { proc.setAuditionLoop(loopB.getToggleState()); };
        favB.onClick = [this] { if (auto* s = selected()) { s->fav = !s->fav; proc.saveMeta(); refresh(); } };
        guide.onClick = [] { juce::URL("https://github.com/SpeakingColors/CATalog/blob/main/docs/CATalog-manual.pdf").launchInDefaultBrowser(); };
        about.onClick = [] { juce::URL("https://ko-fi.com/studio87").launchInDefaultBrowser(); };
        typeChips.onClick = [this](const String& v) { typeFilter = v == typeFilter ? String() : v; refresh(); };
        selTags.onClick = [this](const String& v) { if (auto* s = selected()) { catalog::Library::removeTag(*s, v.toStdString()); proc.saveMeta(); refresh(); } };
        dragPad.fileToDrag = [this] { auto* s = selected(); return s ? juce::File(String(s->path)) : juce::File(); };
        constrainer.setFixedAspectRatio((double) kBaseW / kBaseH); constrainer.setSizeLimits(kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
        setConstrainer(&constrainer); setResizable(true, true); setSize(kBaseW, kBaseH);
        gate.onFrame = [this] { tick(); }; gate.start(20);
        if (const char* f = std::getenv("CAT_TOUR_LOAD"))   // documentation tool for the screenshots, never set for users
            juce::Timer::callAfterDelay(1200, [sp = juce::Component::SafePointer<CATalogEditor>(this), path = String(f)] {
                if (sp == nullptr) return; sp->proc.importFolder(juce::File(path)); while (sp->proc.analysePending(200) > 0) {} sp->refresh(); sp->table.selectRow(std::min(2, std::max(0, (int) sp->rows.size() - 1))); });
        refresh(); setTab(0);
    }
    ~CATalogEditor() override { setLookAndFeel(nullptr); }
    void paint(Graphics& g) override { g.fillAll(kBg); catui::drawWoodCheeks(g, getLocalBounds(), juce::roundToInt(kCheek * scale())); }
    void resized() override { const float s = scale(); content.setBounds(juce::roundToInt(kCheek * s), 0, kContentW, kBaseH); content.setTransform(juce::AffineTransform::scale(s)); layout(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override { const auto p = e.getEventRelativeTo(this).getPosition(); if (p.x < kCheek * scale() || p.x > getWidth() - kCheek * scale()) setSize(kBaseW, kBaseH); }

    // table model
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem(int row, Graphics& g, int w, int h, bool sel) override
    {
        if (row < 0 || row >= (int) rows.size()) return; const auto& s = *rows[(size_t) row];
        if (sel) g.fillAll(kGreen.withAlpha(0.14f)); else if (row % 2) g.fillAll(Colour(0x10ffffff));
        { juce::Path st; st.addStar({ 15.f, (float) h * 0.5f }, 5, 3.6f, 8.f); if (s.fav) { g.setColour(kGold); g.fillPath(st); } else { g.setColour(kDim.withAlpha(0.6f)); g.strokePath(st, juce::PathStrokeType(1.f)); } }
        g.setColour(kText); g.setFont(sans(14.f)); g.drawText(String(s.name), colName, 0, colType - colName - 6, h, juce::Justification::centredLeft, true);
        g.setColour(kDim); g.setFont(sans(13.f)); g.drawText(s.type.empty() ? "?" : String(s.type), colType, 0, colGenre - colType, h, juce::Justification::centredLeft, true);
        g.drawText(String(s.genre), colGenre, 0, colBpm - colGenre, h, juce::Justification::centredLeft, true);
        g.setColour(kGold); g.setFont(mono(13.f)); g.drawText(s.bpm > 0 ? String(juce::roundToInt(s.bpm)) : String(), colBpm, 0, colKey - colBpm, h, juce::Justification::centredLeft);
        g.drawText(String(s.key), colKey, 0, colLen - colKey, h, juce::Justification::centredLeft);
        g.setColour(kDim); if (s.duration > 0) g.drawText(String(s.duration, 1) + " s", colLen, 0, colTags - colLen, h, juce::Justification::centredLeft);
        String t; for (auto& x : s.tags) t << (t.isEmpty() ? "" : ", ") << x; g.setFont(sans(12.f)); g.drawText(t, colTags, 0, w - colTags - 6, h, juce::Justification::centredLeft, true);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= (int) rows.size() || e.x >= 30) return; auto* s = proc.library.byRel(rows[(size_t) row]->rel); if (s) { s->fav = !s->fav; proc.saveMeta(); refresh(); }
    }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { if (row >= 0 && row < (int) rows.size()) { auto* s = proc.library.byRel(rows[(size_t) row]->rel); if (s) { s->fav = !s->fav; proc.saveMeta(); refresh(); } } }
    void selectedRowsChanged(int row) override
    {
        if (row < 0 || row >= (int) rows.size()) return; selRel = rows[(size_t) row]->rel; proc.setSampleBpm(rows[(size_t) row]->bpm); if (autoB.getToggleState()) proc.audition(*rows[(size_t) row]); loadWave(*rows[(size_t) row]); updateSelection(); updatePanes(); content.repaint();
    }
    void textEditorTextChanged(juce::TextEditor&) override { refresh(); }

private:
    struct ListModel : juce::ListBoxModel
    {
        CATalogEditor* o = nullptr; bool tags = false;
        int getNumRows() override { return tags ? (int) o->tagItems.size() : (int) o->folderItems.size(); }
        void paintListBoxItem(int row, Graphics& g, int w, int h, bool sel) override
        {
            const auto& it = tags ? o->tagItems : o->folderItems; if (row < 0 || row >= (int) it.size()) return;
            const bool on = tags ? o->tagSel.count(it[(size_t) row].value.toStdString()) > 0 : sel;
            if (on) g.fillAll(kGreen.withAlpha(0.14f)); g.setColour(on ? kGreen : kText); g.setFont(sans(13.f));
            g.drawText(it[(size_t) row].label, 8 + it[(size_t) row].indent * 12, 0, w - 50, h, juce::Justification::centredLeft, true);
            g.setColour(kDim); g.drawText(String(it[(size_t) row].count), w - 44, 0, 38, h, juce::Justification::centredRight);
        }
        void listBoxItemClicked(int row, const juce::MouseEvent&) override
        {
            if (tags) { if (row < 0 || row >= (int) o->tagItems.size()) return; const auto v = o->tagItems[(size_t) row].value.toStdString(); if (!o->tagSel.erase(v)) o->tagSel.insert(v); o->refresh(); }
            else { if (row < 0 || row >= (int) o->folderItems.size()) return; o->folderSel = o->folderItems[(size_t) row].value; o->refresh(); }
        }
    };
    struct Item { String label, value; int count = 0, indent = 0; };

    CATalogProcessor& proc; catui::CatLookAndFeel laf; juce::ComponentBoundsConstrainer constrainer; catui::RenderGate gate;
    struct Content : juce::Component { CATalogEditor& o; explicit Content(CATalogEditor& e) : o(e) {} void paint(Graphics& g) override { o.paintContent(g); } } content { *this };
    Pill guide { "Guide" }, about { "About & Support" }, importB { "Import folder" }, playB { "Play" }, loopB { "Loop" }, autoB { "Auto-play" }, syncB { "Sync" }, favB { "Favourite" }, favOnly { "Favourites" };
    std::unique_ptr<Pill> tabs[3]; int tab = 0; juce::Slider pitchS, volS, bpmS; bool bpmTouched = false, settingBpmFromHost = false; SpectrumView spectrum; LevelView level; StemsPane stemsPane; SplicePane splicePane;
    struct Body : juce::Component { CATalogEditor& o; explicit Body(CATalogEditor& e) : o(e) {} void paint(Graphics& g) override { o.paintBody(g); } } body { *this };
    juce::ListBox folderList, tagList, table; ListModel folderModel, tagModel; juce::TextEditor search, addTag; juce::ComboBox genreBox, keyBox, sortBox, typeBox;
    ChipBar typeChips, selTags; WaveView wave; DragPad dragPad;
    std::vector<const catalog::Sample*> rows; std::vector<Item> folderItems, tagItems; std::set<std::string> tagSel; String folderSel, typeFilter, selRel, statusLine;
    std::unique_ptr<juce::FileChooser> chooser; size_t lastCount = (size_t) -1;
    static constexpr int colName = 32, colType = 236, colGenre = 296, colBpm = 358, colKey = 400, colLen = 440, colTags = 496, cx0 = 258;

    float scale() const { return (float) getWidth() / (float) kBaseW; }
    catalog::Sample* selected() { return selRel.isEmpty() ? nullptr : proc.library.byRel(selRel.toStdString()); }
    void pick(const String& title, std::function<void(const juce::File&)> done)
    {
        chooser = std::make_unique<juce::FileChooser>(title);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [done](const juce::FileChooser& c) { if (c.getResult().isDirectory()) done(c.getResult()); });
    }
    void layout()
    {
        guide.setBounds(900, 20, 90, 34); about.setBounds(1000, 20, 130, 34);
        for (int i = 0; i < 3; ++i) tabs[i]->setBounds(14 + i * 372, 90, 368, 32);
        body.setBounds(0, kBodyY, kContentW, kBaseH - kBodyY);
        const int lx = 14, lw = 230, cx = cx0, cw = 600, rx = 874, rw = 252;
        importB.setBounds(lx, 106, lw, 34);
        folderList.setBounds(lx, 170, lw, 344); tagList.setBounds(lx, 546, lw, 160);
        typeChips.setBounds(cx, 106, cw, 50); search.setBounds(cx, 160, 200, 28); genreBox.setBounds(cx + 208, 160, 110, 28); keyBox.setBounds(cx + 324, 160, 80, 28); sortBox.setBounds(cx + 410, 160, 90, 28); favOnly.setBounds(cx + 506, 160, 94, 28);
        table.setBounds(cx, 226, cw, 480);
        stemsPane.setBounds(cx, 92, cw, 614); splicePane.setBounds(cx, 92, cw, 614);
        wave.setBounds(rx, 128, rw, 90); playB.setBounds(rx, 226, 72, 30); loopB.setBounds(rx + 78, 226, 64, 30); autoB.setBounds(rx + 148, 226, 104, 30);
        spectrum.setBounds(rx, 262, rw, 62);
        pitchS.setBounds(rx, 332, 44, 44); volS.setBounds(rx + 126, 332, 44, 44); bpmS.setBounds(rx, 384, 44, 44); syncB.setBounds(rx + 126, 392, 80, 30);
        typeBox.setBounds(rx, 436, 160, 28); selTags.setBounds(rx, 506, rw, 52); addTag.setBounds(rx, 562, 100, 24);
        favB.setBounds(rx, 594, 120, 30); dragPad.setBounds(rx, 632, rw, 40); level.setBounds(rx, 708, rw, 28);
    }
    void setTab(int t)
    {
        tab = t; const bool lib = t == 0;
        for (auto* x : std::initializer_list<juce::Component*> { &typeChips, &search, &genreBox, &keyBox, &sortBox, &favOnly, &table }) x->setVisible(lib);
        stemsPane.setVisible(t == 1); splicePane.setVisible(t == 2); updatePanes(); body.repaint();
    }
    void updatePanes()
    {
        auto* s = selected(); stemsPane.title = s ? String(s->name) : String(); stemsPane.found.clear();
        if (s) for (auto& st : proc.library.stemsOf(*s)) { auto* o = proc.library.byRel(st.rel); stemsPane.found.push_back({ String(st.kind), o ? juce::File(String(o->path)).getFileName() : String(st.rel), String(st.rel) }); }
        stemsPane.repaint();
        splicePane.status = proc.watchFolder.isDirectory() ? "Watching: " + proc.watchFolder.getFullPathName() : String("No Splice save folder chosen yet.");
        splicePane.names.clear(); if (proc.watchFolder.isDirectory()) { const auto wf = proc.watchFolder.getFullPathName().toStdString(); for (auto& sm : proc.library.samples) if (sm.path.compare(0, wf.size(), wf) == 0) splicePane.names.push_back(String(sm.rel)); std::sort(splicePane.names.begin(), splicePane.names.end()); }
        splicePane.repaint();
    }
    void tick()
    {
        bool changed = proc.pollWatch(); if (proc.analysePending(6) > 0) changed = true; if (changed) refresh();
        const double pos = proc.auditionPlaying() ? proc.auditionSeconds() : -1.0; if (pos != wave.pos) { wave.pos = pos; wave.repaint(); }
        if (!bpmTouched && proc.hostBpmSeen() >= 40.0 && std::fabs(bpmS.getValue() - proc.hostBpmSeen()) > 0.05) { settingBpmFromHost = true; bpmS.setValue(proc.hostBpmSeen(), juce::sendNotificationSync); settingBpmFromHost = false; }
        level.l = proc.levelL(); level.r = proc.levelR(); level.repaint(); const double fsr = proc.getSampleRate(); spectrum.fs = fsr > 0 ? fsr : 44100.0; spectrum.tickUpdate();
    }
    void loadWave(const catalog::Sample& s)
    {
        wave.peaks.assign(512, 0.f); wave.seconds = s.duration; juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(juce::File(String(s.path)))); if (r == nullptr || r->lengthInSamples <= 0) { wave.peaks.clear(); return; }
        const int n = (int) std::min<juce::int64>(r->lengthInSamples, (juce::int64) (r->sampleRate * 60.0)); juce::AudioBuffer<float> b((int) r->numChannels, n); r->read(&b, 0, n, 0, true, true);
        wave.seconds = (double) r->lengthInSamples / r->sampleRate; const int bins = (int) wave.peaks.size();
        for (int i = 0; i < bins; ++i) { const int a = (int) ((juce::int64) n * i / bins), e = std::max(a + 1, (int) ((juce::int64) n * (i + 1) / bins)); float m = 0.f; for (int c = 0; c < b.getNumChannels(); ++c) m = std::max(m, b.findMinMax(c, a, e - a).getEnd() > 0 ? std::max(std::fabs(b.findMinMax(c, a, e - a).getStart()), std::fabs(b.findMinMax(c, a, e - a).getEnd())) : 0.f); wave.peaks[(size_t) i] = m; }
    }
    void updateSelection()
    {
        auto* s = selected(); favB.setToggleState(s && s->fav, juce::dontSendNotification);
        selTags.chips.clear(); if (s) for (auto& t : s->tags) selTags.chips.push_back({ String(t) + " x", String(t), true }); selTags.repaint();
        typeBox.clear(juce::dontSendNotification); typeBox.addItem("Type: unknown", 1); int pickId = 1, id = 2;
        for (auto* t : { "kick", "snare", "clap", "hat", "perc", "bass", "synth", "stab", "pad", "lead", "vocal", "fx", "loop", "drum loop" }) { typeBox.addItem(t, id); if (s && s->type == t) pickId = id; ++id; }
        typeBox.setSelectedId(pickId, juce::dontSendNotification); typeBox.setEnabled(s != nullptr);
    }
    void refresh()
    {
        auto& L = proc.library;
        if (L.samples.size() != lastCount)
        {   // rebuild facets when the library changed
            lastCount = L.samples.size();
            auto fill = [](juce::ComboBox& cb, const std::vector<std::pair<std::string, int>>& v, const String& all) { const auto cur = cb.getText(); cb.clear(juce::dontSendNotification); cb.addItem(all, 1); int id = 2, pick = 1; for (auto& p : v) { if (p.first.empty()) continue; cb.addItem(String(p.first), id); if (String(p.first) == cur) pick = id; ++id; } cb.setSelectedId(pick, juce::dontSendNotification); };
            fill(genreBox, L.genreCounts(), "Genre: all"); fill(keyBox, L.keyCounts(), "Key: all");
            std::map<std::string, int> fc; for (auto& s : L.samples) { std::string p; std::stringstream ss(s.folder); std::string part; while (std::getline(ss, part, '/')) { p += (p.empty() ? "" : "/") + part; ++fc[p]; } }
            folderItems.clear(); folderItems.push_back({ "All samples", "", (int) L.samples.size(), 0 });
            for (auto& kv : fc) { const auto slash = kv.first.find_last_of('/'); const int depth = (int) std::count(kv.first.begin(), kv.first.end(), '/'); folderItems.push_back({ String(slash == std::string::npos ? kv.first : kv.first.substr(slash + 1)), String(kv.first), kv.second, depth }); }
            tagItems.clear(); auto tc = L.tagCounts(); std::stable_sort(tc.begin(), tc.end(), [](auto& a, auto& b) { return a.second > b.second; }); for (auto& t : tc) { tagItems.push_back({ String(t.first), String(t.first), t.second, 0 }); if (tagItems.size() >= 60) break; }
            folderList.updateContent(); tagList.updateContent();
        }
        typeChips.chips.clear(); typeChips.chips.push_back({ "all " + String((int) L.samples.size()), "", typeFilter.isEmpty() }); for (auto& t : L.typeCounts()) typeChips.chips.push_back({ String(t.first) + " " + String(t.second), String(t.first), typeFilter == String(t.first) }); typeChips.repaint();
        catalog::Query q; q.text = search.getText().toStdString(); q.folder = folderSel.toStdString(); q.type = typeFilter.toStdString(); q.favOnly = favOnly.getToggleState();
        for (auto& t : tagSel) q.tags.push_back(t);
        if (genreBox.getSelectedId() > 1) q.genre = genreBox.getText().toStdString(); if (keyBox.getSelectedId() > 1) q.key = keyBox.getText().toStdString();
        const char* sorts[] = { "name", "bpm", "key", "rating", "size" }; q.sort = sorts[juce::jlimit(0, 4, sortBox.getSelectedId() - 1)];
        rows = L.find(q); table.updateContent(); table.repaint(); updateSelection();
        int pend = 0; for (auto& s : L.samples) if (!s.analysed) ++pend;
        statusLine = String((int) rows.size()) + " of " + String((int) L.samples.size()) + " samples in " + String((int) L.roots.size()) + " folder(s)." + (pend ? "  Analysing " + String(pend) + " more..." : String());
        updatePanes(); content.repaint();
    }
    void paintContent(Graphics& g)
    {
        g.fillAll(kBg);
        catui::drawCatHead(g, { 22, 14, 60, 56 }, kText, kBg);
        g.setColour(kText); g.setFont(sans(34.f, true)); g.drawText("CATALOG", 96, 8, 400, 46, juce::Justification::left);
        g.setColour(kGold); g.setFont(sans(14.f, true)); g.drawText("STUDIO 87", 98, 52, 200, 20, juce::Justification::left);
        g.setColour(kEdge); g.drawHorizontalLine(84, 14.f, 1126.f);
    }
    void paintBody(Graphics& g)
    {
        auto lab = [&](const String& t, int x, int y) { g.setColour(kDim); g.setFont(sans(12.f, true)); g.drawText(t, x, y, 260, 16, juce::Justification::left); };
        lab("IMPORT", 14, 88); lab("FOLDERS", 14, 150); lab("TAGS", 14, 526);
        if (tab == 0)
        {
            lab("TYPE", cx0, 88);
            g.setColour(kDim); g.setFont(mono(12.f)); g.drawText(statusLine, cx0, 192, 600, 18, juce::Justification::left);
            g.setFont(mono(11.f)); const int ty = 208; g.drawText("NAME", cx0 + colName, ty, 200, 16, juce::Justification::left); g.drawText("TYPE", cx0 + colType, ty, 80, 16, juce::Justification::left); g.drawText("GENRE", cx0 + colGenre, ty, 80, 16, juce::Justification::left);
            g.drawText("BPM", cx0 + colBpm, ty, 50, 16, juce::Justification::left); g.drawText("KEY", cx0 + colKey, ty, 50, 16, juce::Justification::left); g.drawText("LENGTH", cx0 + colLen, ty, 70, 16, juce::Justification::left); g.drawText("TAGS", cx0 + colTags, ty, 80, 16, juce::Justification::left);
        }
        lab("PREVIEW", 874, 88);
        auto cap = [&](int x, int y, const String& name, const String& val) { g.setColour(kDim); g.setFont(sans(11.f, true)); g.drawText(name, x, y + 4, 80, 14, juce::Justification::left); g.setColour(kGreen); g.setFont(mono(13.f)); g.drawText(val, x, y + 20, 80, 16, juce::Justification::left); };
        cap(874 + 50, 332, "PITCH", String(juce::roundToInt(pitchS.getValue())) + " st"); cap(874 + 126 + 50, 332, "VOLUME", String(volS.getValue(), 1) + " dB"); cap(874 + 50, 384, "TEMPO", String(bpmS.getValue(), 1));
        { auto* sm = selected(); const String note = !syncB.getToggleState() ? String() : (sm && sm->bpm > 0 ? "speed x" + String(proc.currentRate(), 2) : String("no BPM on this sample")); g.setColour(kDim); g.setFont(sans(11.f)); g.drawText(note, 874 + 126, 424, 126, 12, juce::Justification::left); }
        if (auto* s = selected())
        {
            g.setColour(kGreen); g.setFont(mono(15.f, true)); g.drawText(String(s->name), 874, 104, 252, 20, juce::Justification::left, true);
            const auto st = proc.library.stemsOf(*s); g.setColour(kDim); g.setFont(sans(12.f)); juce::String t;
            if (st.empty()) t = "Stems: none found next to this file."; else { t = "Stems found: "; for (auto& x : st) t << x.kind << " "; t << "(Stems tab)"; }
            g.drawFittedText(t, 874, 676, 252, 28, juce::Justification::topLeft, 2);
            g.drawText("TYPE: " + (s->type.empty() ? String("unknown") : String(s->type)) + (s->typeSrc.empty() ? String() : " (from " + String(s->typeSrc) + ")"), 874, 466, 252, 18, juce::Justification::left);
            lab("TAGS ON THIS SAMPLE", 874, 488);
            for (int i = 0; i < 5; ++i) { juce::Path sp; sp.addStar({ (float) (874 + 138 + i * 22 + 10), 609.f }, 5, 4.f, 9.f); if (i < s->rating) { g.setColour(kGold); g.fillPath(sp); } else { g.setColour(kEdge); g.strokePath(sp, juce::PathStrokeType(1.f)); } }
        }
        g.setColour(kDim); g.setFont(mono(11.f)); g.drawText("STUDIO 87 | CATALOG | hover a control for help", 14, 722, 700, 16, juce::Justification::left);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const auto p = e.getEventRelativeTo(&body).getPosition(); auto* s = selected();
        if (s && p.y > 594 && p.y < 624 && p.x >= 874 + 138 && p.x < 874 + 138 + 110) { const int r = (p.x - (874 + 138)) / 22 + 1; s->rating = (s->rating == r) ? 0 : r; proc.saveMeta(); refresh(); }
    }
};
}
juce::AudioProcessorEditor* CATalogProcessor::createEditor() { return new CATalogEditor(*this); }
