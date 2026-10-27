// CATalog window, built from docs/mockup/catalog.html: header, import bar, folder and tag lists, type chips, search and filters,
// the sample table, and the preview panel (waveform, play, loop, type, tags, favourite, rating, drag to the DAW).
// Fixed aspect ratio, resizable from every corner and edge, wood cheeks.
// Not built yet (see ROADMAP): the Stems and Splice tabs, spectrum, pitch / volume / BPM sync preview, send to SupurrSaw, presets.
#include "CATalog.h"
#include <catui/CatLookAndFeel.h>
#include <catui/RenderGate.h>
#include <catui/Logo.h>
#include <cmath>
#include <map>
#include <set>

namespace {
using juce::Colour; using juce::Graphics; using juce::String;
const Colour kBg(0xff101114), kPanel(0xff15161a), kEdge(0xff2c2e35), kText(0xffe8e6e1), kDim(0xff8c909a), kGold(0xffffd100), kGreen(0xff5fe08c);
constexpr int kCheek = 30, kContentW = 1140, kBaseW = kContentW + 2 * kCheek, kBaseH = 760;
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

class CATalogEditor : public juce::AudioProcessorEditor, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    explicit CATalogEditor(CATalogProcessor& p) : AudioProcessorEditor(p), proc(p), gate(*this)
    {
        setLookAndFeel(&laf); auto& c = content; addAndMakeVisible(c);
        for (auto* b : { &guide, &about, &importB, &watchB, &spliceB, &playB, &loopB, &favB }) c.addAndMakeVisible(*b);
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
        watchB.setTooltip("Choose the folder Splice saves your downloads into. CATalog watches it and picks up new files. There is no Splice login here: Splice has no public API.");
        spliceB.setTooltip("Opens the Splice app (or splice.com). You sign in and download there.");
        playB.setTooltip("Play the selected sample from the start."); loopB.setTooltip("Repeat the sample while it plays.");
        favB.setTooltip("Mark as favourite (double click a row does the same)."); dragPad.setTooltip("Drag the selected sample onto a track in your DAW.");
        importB.onClick = [this] { pick("Pick a sample folder (subfolders are included)", [this](const juce::File& f) { proc.importFolder(f); proc.analysePending(); refresh(); }); };
        watchB.onClick = [this] { pick("Pick the folder Splice saves into", [this](const juce::File& f) { proc.setWatchFolder(f); proc.analysePending(); refresh(); }); };
        spliceB.onClick = [this] { proc.openSplice(); };
        playB.onClick = [this] { if (auto* s = selected()) proc.audition(*s); };
        loopB.onClick = [this] { proc.setAuditionLoop(loopB.getToggleState()); };
        favB.onClick = [this] { if (auto* s = selected()) { s->fav = !s->fav; proc.saveMeta(); refresh(); } };
        guide.onClick = [] { juce::URL("https://github.com/SpeakingColors/CATalog/blob/main/docs/CATalog-manual.pdf").launchInDefaultBrowser(); };
        about.onClick = [] { juce::URL("https://ko-fi.com/studio87").launchInDefaultBrowser(); };
        typeChips.onClick = [this](const String& v) { typeFilter = v == typeFilter ? String() : v; refresh(); };
        selTags.onClick = [this](const String& v) { if (auto* s = selected()) { catalog::Library::removeTag(*s, v.toStdString()); proc.saveMeta(); refresh(); } };
        dragPad.fileToDrag = [this] { auto* s = selected(); return s ? juce::File(String(s->path)) : juce::File(); };
        constrainer.setFixedAspectRatio((double) kBaseW / kBaseH); constrainer.setSizeLimits(kBaseW / 2, kBaseH / 2, kBaseW * 2, kBaseH * 2);
        setConstrainer(&constrainer); setResizable(true, true); setSize(kBaseW, kBaseH);
        gate.onFrame = [this] { tick(); }; gate.start(10);
        if (const char* f = std::getenv("CAT_TOUR_LOAD"))   // documentation tool for the screenshots, never set for users
            juce::Timer::callAfterDelay(1200, [sp = juce::Component::SafePointer<CATalogEditor>(this), path = String(f)] {
                if (sp == nullptr) return; sp->proc.importFolder(juce::File(path)); while (sp->proc.analysePending(200) > 0) {} sp->refresh(); sp->table.selectRow(std::min(2, std::max(0, (int) sp->rows.size() - 1))); });
        refresh();
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
        if (row < 0 || row >= (int) rows.size()) return; selRel = rows[(size_t) row]->rel; proc.audition(*rows[(size_t) row]); loadWave(*rows[(size_t) row]); updateSelection(); content.repaint();
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
    Pill guide { "Guide" }, about { "About & Support" }, importB { "Import folder" }, watchB { "Splice save folder" }, spliceB { "Open Splice" }, playB { "Play" }, loopB { "Loop" }, favB { "Favourite" }, favOnly { "Favourites" };
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
        const int lx = 14, lw = 230, cx = cx0, cw = 600, rx = 874, rw = 252;
        importB.setBounds(lx, 106, lw, 34); watchB.setBounds(lx, 146, lw, 30); spliceB.setBounds(lx, 182, lw, 30);
        folderList.setBounds(lx, 244, lw, 270); tagList.setBounds(lx, 546, lw, 160);
        typeChips.setBounds(cx, 106, cw, 50); search.setBounds(cx, 160, 200, 28); genreBox.setBounds(cx + 208, 160, 110, 28); keyBox.setBounds(cx + 324, 160, 80, 28); sortBox.setBounds(cx + 410, 160, 90, 28); favOnly.setBounds(cx + 506, 160, 94, 28);
        table.setBounds(cx, 226, cw, 480);
        wave.setBounds(rx, 128, rw, 90); playB.setBounds(rx, 226, 80, 32); loopB.setBounds(rx + 88, 226, 70, 32);
        typeBox.setBounds(rx, 268, 160, 28); selTags.setBounds(rx, 340, rw, 70); addTag.setBounds(rx, 416, 100, 24);
        favB.setBounds(rx, 450, 120, 32); dragPad.setBounds(rx, 500, rw, 44);
    }
    void tick()
    {
        bool changed = proc.pollWatch(); if (proc.analysePending(6) > 0) changed = true; if (changed) refresh();
        const double pos = proc.auditionPlaying() ? proc.auditionSeconds() : -1.0; if (pos != wave.pos) { wave.pos = pos; wave.repaint(); }
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
        content.repaint();
    }
    void paintContent(Graphics& g)
    {
        g.fillAll(kBg);
        catui::drawCatHead(g, { 22, 14, 60, 56 }, kText, kBg);
        g.setColour(kText); g.setFont(sans(34.f, true)); g.drawText("CATALOG", 96, 8, 400, 46, juce::Justification::left);
        g.setColour(kGold); g.setFont(sans(14.f, true)); g.drawText("STUDIO 87", 98, 52, 200, 20, juce::Justification::left);
        g.setColour(kEdge); g.drawHorizontalLine(84, 14.f, 1126.f);
        auto lab = [&](const String& t, int x, int y) { g.setColour(kDim); g.setFont(sans(12.f, true)); g.drawText(t, x, y, 260, 16, juce::Justification::left); };
        lab("IMPORT", 14, 88); lab("FOLDERS", 14, 224); lab("TAGS", 14, 526); lab("TYPE", cx0, 88);
        g.setColour(kDim); g.setFont(mono(12.f)); g.drawText(statusLine, cx0, 192, 600, 18, juce::Justification::left);
        g.setFont(mono(11.f)); const int ty = 208; g.drawText("NAME", cx0 + colName, ty, 200, 16, juce::Justification::left); g.drawText("TYPE", cx0 + colType, ty, 80, 16, juce::Justification::left); g.drawText("GENRE", cx0 + colGenre, ty, 80, 16, juce::Justification::left);
        g.drawText("BPM", cx0 + colBpm, ty, 50, 16, juce::Justification::left); g.drawText("KEY", cx0 + colKey, ty, 50, 16, juce::Justification::left); g.drawText("LENGTH", cx0 + colLen, ty, 70, 16, juce::Justification::left); g.drawText("TAGS", cx0 + colTags, ty, 80, 16, juce::Justification::left);
        if (auto* s = selected())
        {
            g.setColour(kGreen); g.setFont(mono(15.f, true)); g.drawText(String(s->name), 874, 100, 252, 22, juce::Justification::left, true);
            const auto st = proc.library.stemsOf(*s); g.setColour(kDim); g.setFont(sans(12.f)); juce::String t;
            if (st.empty()) t = "Stems: none found next to this file."; else { t = "Stems found: "; for (auto& x : st) t << x.kind << " "; }
            g.drawFittedText(t, 874, 560, 252, 48, juce::Justification::topLeft, 3);
            g.drawText("TYPE: " + (s->type.empty() ? String("unknown") : String(s->type)) + (s->typeSrc.empty() ? String() : " (from " + String(s->typeSrc) + ")"), 874 + 0, 300, 252, 18, juce::Justification::left);
            lab("TAGS ON THIS SAMPLE", 874, 320);
            g.setColour(kGold); g.setFont(sans(18.f)); const int stars = s->rating; for (int i = 0; i < 5; ++i) g.setColour(i < stars ? kGold : kEdge), g.drawText("*", 874 + 140 + i * 20, 454, 20, 24, juce::Justification::centred);
        }
        g.setColour(kDim); g.setFont(mono(11.f)); g.drawText("STUDIO 87 | CATALOG | hover a control for help", 14, 736, 700, 16, juce::Justification::left);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        const auto p = e.getEventRelativeTo(&content).getPosition(); auto* s = selected();
        if (s && p.y > 450 && p.y < 480 && p.x > 1014 && p.x < 1126) { const int r = (p.x - 1014) / 20 + 1; s->rating = (s->rating == r) ? 0 : r; proc.saveMeta(); refresh(); }
    }
};
}
juce::AudioProcessorEditor* CATalogProcessor::createEditor() { return new CATalogEditor(*this); }
