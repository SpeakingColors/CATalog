// Core tests for CATalog. Pure C++, no JUCE. A small library of empty files is built in a temp folder.
#include "../src/core/Library.h"
#include <cstdio>
#include <cstdint>
#include <cmath>
using namespace catalog;
namespace fs = std::filesystem;

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; std::printf("FAIL %s:%d  ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static void touch(const fs::path& p, size_t bytes = 10) { fs::create_directories(p.parent_path()); std::ofstream o(p, std::ios::binary); o << std::string(bytes, 'x'); }
static std::string names(const std::vector<const Sample*>& v) { std::string s; for (auto* p : v) s += p->name + "|"; return s; }

static void testGuess() {
    CHECK(guessBpm("Kick_128bpm_Am") == 128, "128bpm");
    CHECK(guessBpm("Loop 90 BPM") == 90, "90 BPM with space");
    CHECK(guessBpm("bpm_140_lead") == 140, "bpm_140");
    CHECK(guessBpm("Kick 808 hard") == 0, "808 is not a tempo");
    CHECK(guessBpm("Pad_300bpm") == 0, "300 bpm is not plausible");
    CHECK(guessBpm("Take 2 of 12") == 0, "plain numbers are not tempos");
    CHECK(guessKey("Pad_Am_120bpm") == "Am", "Am"); CHECK(guessKey("Bass_C#m") == "C#m", "C#m"); CHECK(guessKey("Keys_Dbmin_90") == "Dbmin" || guessKey("Keys_Dbmin_90") == "Dbm", "Dbmin -> %s", guessKey("Keys_Dbmin_90").c_str());
    CHECK(guessKey("Lead_F#maj") == "F#", "F#maj"); CHECK(guessKey("Lead_Eb_key") == "Eb", "bare Eb with key word");
    CHECK(guessKey("fm bass am") == "", "lower case words are not keys, got %s", guessKey("fm bass am").c_str());
    CHECK(guessKey("Kick_A") == "", "bare capital without 'key' is not taken");
    CHECK(guessKey("Key A Pad") == "A", "'Key A' works");
    CHECK(guessKey("Dub Stab") == "", "Dub is not D");
}
static void testNatural() {
    CHECK(Library::natural("kick 2", "kick 10"), "2 before 10"); CHECK(!Library::natural("kick 10", "kick 2"), "10 not before 2");
    CHECK(Library::natural("Apple", "banana"), "case insensitive"); CHECK(!Library::natural("a", "a"), "equal is not less");
}
static void testScanAndQuery() {
    const fs::path root = fs::temp_directory_path() / "catalog_test_lib"; fs::remove_all(root);
    touch(root / "Drums/Kicks/Kick 1.wav", 100); touch(root / "Drums/Kicks/Kick 10.wav", 400); touch(root / "Drums/Kicks/Kick 2.wav", 200);
    touch(root / "Drums/Snares/Snare_90bpm.flac", 300); touch(root / "Synth/Pads/Pad_Am_120bpm.wav", 500); touch(root / "Synth/Lead_C#m.aif", 150);
    touch(root / "notes.txt"); touch(root / "Synth/cover.jpg");
    Library L; const int n = L.scan(root.string()); CHECK(n == 6, "found %d audio files, wanted 6 (txt and jpg ignored)", n);
    Query q; q.sort = "name";
    CHECK(names(L.find(q)).find("Kick 1|Kick 2|Kick 10|") != std::string::npos, "natural order: %s", names(L.find(q)).c_str());
    q.folder = "Drums"; CHECK(L.find(q).size() == 4, "folder Drums holds 4 (got %zu)", L.find(q).size());
    q.folder = "Drums/Kicks"; CHECK(L.find(q).size() == 3, "folder Kicks holds 3");
    q.folder = "Dru"; CHECK(L.find(q).empty(), "folder match is by whole name, not prefix");
    q = Query(); q.text = "pad am"; CHECK(L.find(q).size() == 1, "words search: %zu", L.find(q).size());
    q = Query(); q.text = "120"; CHECK(L.find(q).size() == 1, "search by bpm number");
    q = Query(); q.tags = { "snares" }; CHECK(L.find(q).size() == 1, "folder names became tags");
    q = Query(); q.bpmMin = 100; CHECK(L.find(q).size() == 1 && L.find(q)[0]->name == "Pad_Am_120bpm", "bpm min 100");
    q = Query(); q.sort = "size"; CHECK(L.find(q)[0]->name == "Pad_Am_120bpm", "biggest first");
    q = Query(); q.sort = "bpm"; CHECK(L.find(q)[0]->bpm == 120 && L.find(q)[1]->bpm == 90, "bpm sort");
    auto fc = L.folderCounts(); CHECK(fc["Drums"] == 4 && fc["Drums/Kicks"] == 3 && fc["Synth"] == 2 && fc["Synth/Pads"] == 1, "folder counts");
    auto tc = L.tagCounts(); CHECK(!tc.empty() && tc[0].second >= 3, "tag counts, top = %s", tc.empty() ? "?" : tc[0].first.c_str());

    // your own data survives a rescan and a save / load
    Sample* s = L.byRel("Synth/Pads/Pad_Am_120bpm.wav"); CHECK(s != nullptr, "byRel"); if (!s) return;
    Library::addTag(*s, "  Warm!! "); Library::addTag(*s, "warm"); s->fav = true; s->rating = 4;
    CHECK(std::count(s->tags.begin(), s->tags.end(), "warm") == 1, "tag cleaned and not duplicated");
    q = Query(); q.favOnly = true; CHECK(L.find(q).size() == 1, "favourites"); q.favOnly = false; q.minRating = 5; CHECK(L.find(q).empty(), "rating filter");
    touch(root / "Synth/Pads/New Pad.wav"); L.scan(root.string());
    s = L.byRel("Synth/Pads/Pad_Am_120bpm.wav"); CHECK(s && s->fav && s->rating == 4 && std::count(s->tags.begin(), s->tags.end(), "warm") == 1, "rescan keeps your data");
    CHECK(L.samples.size() == 7, "rescan sees the new file (%zu)", L.samples.size());
    const std::string mf = (root / "meta.txt").string(); CHECK(L.saveMeta(mf), "meta saved");
    Library L2; L2.scan(root.string(), false); CHECK(L2.byRel("Synth/Pads/Pad_Am_120bpm.wav")->tags.empty(), "fresh library has no tags without folder tags");
    CHECK(L2.loadMeta(mf) >= 1, "meta loaded"); s = L2.byRel("Synth/Pads/Pad_Am_120bpm.wav"); CHECK(s->fav && s->rating == 4 && std::count(s->tags.begin(), s->tags.end(), "warm") == 1, "meta round trip");
    Library::removeTag(*s, "WARM"); CHECK(std::count(s->tags.begin(), s->tags.end(), "warm") == 0, "remove tag");
    { std::ofstream bad(root / "bad.txt"); bad << "not a meta file\n"; } CHECK(L2.loadMeta((root / "bad.txt").string()) == 0 && L2.loadMeta((root / "missing.txt").string()) == 0, "bad and missing meta files are ignored");
    fs::remove_all(root);
}

// ---- v2: import banks, formats, classification, analysis, stems, watch ----
static void writeWav(const fs::path& p, const std::vector<float>& x, double sr, int bits = 16) {
    fs::create_directories(p.parent_path()); std::ofstream o(p, std::ios::binary); const uint32_t bps = (uint32_t) bits / 8, data = (uint32_t)(x.size() * bps), rate = (uint32_t) sr; const uint16_t one = 1, ch = 1, b = (uint16_t) bits, al = (uint16_t) bps; const uint32_t sixteen = 16, riff = 36 + data, br = rate * bps;
    o.write("RIFF", 4); o.write((const char*) &riff, 4); o.write("WAVEfmt ", 8); o.write((const char*) &sixteen, 4); o.write((const char*) &one, 2); o.write((const char*) &ch, 2); o.write((const char*) &rate, 4); o.write((const char*) &br, 4); o.write((const char*) &al, 2); o.write((const char*) &b, 2); o.write("data", 4); o.write((const char*) &data, 4);
    for (float v : x) { if (bits == 16) { int16_t q = (int16_t) std::lround(std::max(-1.f, std::min(1.f, v)) * 32767); o.write((const char*) &q, 2); } else { int32_t q = (int32_t)(std::max(-1.0, std::min(1.0, (double) v)) * 8388607.0) << 8; o.write(((const char*) &q) + 1, 3); } }
}
static unsigned rngState = 12345; static float rnd() { rngState = rngState * 1664525u + 1013904223u; return (float)((rngState >> 8) & 0xFFFF) / 32768.f - 1.f; }
static std::vector<float> kickSig(double sr, double len = 0.5) { std::vector<float> x((size_t)(sr * len)); double ph = 0; for (size_t i = 0; i < x.size(); ++i) { const double t = (double) i / sr, f = 45 + 110 * std::exp(-t * 28); ph += 2 * 3.14159265358979 * f / sr; x[i] = (float)(std::sin(ph) * std::exp(-t * 7) * 0.9); } return x; }
static std::vector<float> hatSig(double sr) { std::vector<float> x((size_t)(sr * 0.15)); float p = 0; for (size_t i = 0; i < x.size(); ++i) { const float n = rnd(); const float h = n - p; p = n; x[i] = h * 0.4f * (float) std::exp(-(double) i / sr * 40); } return x; }
static std::vector<float> chordSig(double sr, std::vector<double> f, double len) { std::vector<float> x((size_t)(sr * len)); for (size_t i = 0; i < x.size(); ++i) { double v = 0; for (double q : f) v += std::sin(2 * 3.14159265358979 * q * (double) i / sr) + 0.3 * std::sin(4 * 3.14159265358979 * q * (double) i / sr); x[i] = (float)(v / (double) f.size() / 1.5 * 0.7); } return x; }
static std::vector<float> loopSig(double sr, double bpm, double secs) { std::vector<float> x((size_t)(sr * secs), 0.f); auto k = kickSig(sr, 0.25); const double beat = 60.0 / bpm; for (double t = 0; t < secs - 0.3; t += beat) { const size_t s0 = (size_t)(t * sr); for (size_t i = 0; i < k.size() && s0 + i < x.size(); ++i) x[s0 + i] += k[i]; } return x; }

static void testWords() {
    CHECK(guessType("Kick_Hard_01", "") == "kick", "kick"); CHECK(guessType("BD 12", "Drums") == "kick", "bd");
    CHECK(guessType("OH_01", "Hats") == "hat", "folder says hats"); CHECK(guessType("Lead Stab Cm", "Synth") == "stab", "name beats folder: %s", guessType("Lead Stab Cm", "Synth").c_str());
    CHECK(guessType("Vox_Chop", "") == "vocal", "vox"); CHECK(guessType("Amen_Break_Loop_170", "") == "drum loop", "break loop");
    CHECK(guessType("Hat_Loop_130", "") == "drum loop", "hat loop is a drum loop"); CHECK(guessType("Melody_Loop_Am", "") == "loop", "plain loop");
    CHECK(guessType("Snare_3", "808 Kit") == "snare", "snare in a 808 folder, name wins"); CHECK(guessType("thing_07", "misc") == "", "no word, no type");
    CHECK(guessType("SubBass_01", "") == "bass", "camel case words");
    CHECK(guessGenre("Kick", "Techno Pack/Drums") == "techno", "genre from folder"); CHECK(guessGenre("DnB_Reese", "") == "drum and bass", "dnb");
    CHECK(guessGenre("Hip Hop Snare", "") == "hip hop", "two word genre"); CHECK(guessGenre("Housekeeping", "") == "", "word match only, not substring");
    CHECK(guessGenre("kick", "") == "", "no guess without a word");
}
static void testImport() {
    const fs::path a = fs::temp_directory_path() / "cat_bankA", b = fs::temp_directory_path() / "cat_bankB"; fs::remove_all(a); fs::remove_all(b);
    touch(a / "Drums/Kicks/Deep/Sub/Old/k1.wav"); touch(a / "Drums/Snares/s1.aiff"); touch(a / "Loops/l.m4a"); touch(a / "a.opus"); touch(a / "x.AAC"); touch(a / "readme.pdf"); touch(a / "Vox/v.FLAC"); touch(a / "Vox/v.mp3"); touch(a / "Vox/v.ogg");
    touch(b / "Kicks/k1.wav");
    Library L; int n = L.importFolder(a.string()); CHECK(n == 8, "deep subfolders and formats found: %d", n);
    CHECK(L.byRel("cat_bankA/Drums/Kicks/Deep/Sub/Old/k1.wav") != nullptr, "five levels down is found");
    CHECK(L.importFolder(a.string()) == 0 && L.samples.size() == 8, "same folder twice adds nothing");
    n = L.importFolder(b.string()); CHECK(n == 1 && L.samples.size() == 9, "second bank added, first kept (%zu)", L.samples.size());
    CHECK(L.byRel("cat_bankB/Kicks/k1.wav") && L.byRel("cat_bankA/Drums/Kicks/Deep/Sub/Old/k1.wav"), "same file name in two banks stays apart");
    Query q; q.folder = "cat_bankA/Drums"; CHECK(L.find(q).size() == 2, "bank folder filter"); CHECK(L.roots.size() == 2, "two roots");
    Sample* s = L.byRel("cat_bankA/Vox/v.mp3"); s->fav = true; Library::addTag(*s, "keep");
    const std::string sig0 = Library::folderSignature(a.string()); touch(a / "Late/Deeper/Still/new.wav"); fs::remove(b / "Kicks/k1.wav");
    L.rescanAll(); CHECK(L.byRel("cat_bankA/Late/Deeper/Still/new.wav") != nullptr, "rescan finds new files in subfolders"); CHECK(L.byRel("cat_bankB/Kicks/k1.wav") == nullptr, "rescan drops deleted files");
    s = L.byRel("cat_bankA/Vox/v.mp3"); CHECK(s && s->fav && std::count(s->tags.begin(), s->tags.end(), "keep") == 1, "rescan keeps your data");
    CHECK(Library::folderSignature(a.string()) != sig0, "folder signature changes when a file is added");
    q = Query(); q.type = "kick"; CHECK(L.find(q).size() == 1, "type filter kick: %zu", L.find(q).size()); q.type = "vocal"; CHECK(L.find(q).size() == 3, "type filter vocal (folder Vox): %zu", L.find(q).size());
    q = Query(); q.type = "unknown"; CHECK(!L.find(q).empty(), "unknown filter");
    auto tc = L.typeCounts(); CHECK(!tc.empty(), "type counts");
    Library::setType(*L.byRel("cat_bankA/a.opus"), "Pad"); const std::string mf = (a / "meta.txt").string(); L.saveMeta(mf);
    Library L3; L3.importFolder(a.string()); L3.loadMeta(mf); CHECK(L3.byRel("cat_bankA/a.opus")->type == "pad" && L3.byRel("cat_bankA/a.opus")->typeSrc == "user", "your own type round trips");
    fs::remove_all(a); fs::remove_all(b);
}
static void testStems() {
    const fs::path r = fs::temp_directory_path() / "cat_stems"; fs::remove_all(r);
    touch(r / "Loops/Groove_Am_120bpm.wav"); touch(r / "Loops/Groove_Am_120bpm_drums.wav"); touch(r / "Loops/Groove_Am_120bpm_Bass.wav"); touch(r / "Loops/Groove_Am_120bpm (Vocals).wav"); touch(r / "Loops/Groove_Am_120bpm - other.wav");
    touch(r / "Loops/Solo.wav"); touch(r / "Loops/Solo Loop stems/drums.wav"); touch(r / "Loops/Solo Loop stems/vocals.wav"); touch(r / "Loops/Solo Loop.wav"); touch(r / "Loops/Groove2.wav"); touch(r / "Loops/Groove2 B.wav");
    Library L; L.scan(r.string()); auto* g = L.byRel("Loops/Groove_Am_120bpm.wav"); auto st = L.stemsOf(*g);
    std::string kinds; for (auto& x : st) kinds += x.kind + ","; CHECK(kinds == "bass,drums,other,vocals,", "stems found: %s", kinds.c_str());
    CHECK(L.stemsOf(*L.byRel("Loops/Solo.wav")).empty(), "no stems for Solo"); st = L.stemsOf(*L.byRel("Loops/Solo Loop.wav")); CHECK(st.size() == 2, "stems folder: %zu", st.size());
    CHECK(L.stemsOf(*L.byRel("Loops/Groove2.wav")).empty(), "a sibling that is not a stem word is not a stem");
    fs::remove_all(r);
}
static void testAnalysis() {
    const double sr = 44100;
    auto k = analyse(kickSig(sr), sr); CHECK(k.ok && k.type == "kick", "kick heard as %s (centroid %.0f low %.2f)", k.type.c_str(), k.centroid, k.lowShare);
    auto h = analyse(hatSig(sr), sr); CHECK(h.type == "hat", "hat heard as %s (centroid %.0f high %.2f)", h.type.c_str(), h.centroid, h.highShare);
    auto lp = analyse(loopSig(sr, 120, 8), sr); CHECK(std::fabs(lp.bpm - 120) < 1.5, "loop bpm %.2f", lp.bpm); CHECK(lp.type == "loop" || lp.type == "drum loop", "loop type %s", lp.type.c_str());
    auto lp2 = analyse(loopSig(sr, 95, 10), sr); CHECK(std::fabs(lp2.bpm - 95) < 1.5, "loop bpm 95: %.2f", lp2.bpm);
    CHECK(analyse(kickSig(sr), sr).bpm == 0, "a single hit has no tempo"); CHECK(lp.key.empty(), "a kick pattern has no key, got %s", lp.key.c_str());
    CHECK(guessGenre("Hardstyle Kick", "") == "hardstyle", "hardstyle");
    auto am = analyse(chordSig(sr, { 220.0, 261.63, 329.63 }, 3), sr); CHECK(am.key == "Am", "A minor chord -> %s", am.key.c_str());
    auto c = analyse(chordSig(sr, { 130.81, 164.81, 196.0, 261.63 }, 3), sr); CHECK(c.key == "C", "C major chord -> %s", c.key.c_str());
    auto e = analyse(chordSig(sr, { 164.81, 196.0, 246.94 }, 3), sr); CHECK(e.key == "Em", "E minor chord -> %s", e.key.c_str());
    auto bs = analyse(chordSig(sr, { 55.0 }, 1.0), sr); CHECK(bs.type == "bass", "55 Hz note -> %s (f0 %.1f)", bs.type.c_str(), bs.f0);
    auto pd = analyse(chordSig(sr, { 220.0, 261.63, 329.63 }, 4), sr); CHECK(pd.type == "pad" || pd.type == "synth", "long chord -> %s", pd.type.c_str());
    CHECK(!analyse(std::vector<float>(100, 0.f), sr).ok, "too short is not ok"); CHECK(analyse(std::vector<float>(44100, 0.f), sr).type == "other", "silence -> other");
    // file round trip: 16 bit and 24 bit wav, library fold-in, names and user data win
    const fs::path r = fs::temp_directory_path() / "cat_an"; fs::remove_all(r);
    writeWav(r / "mystery.wav", loopSig(sr, 128, 8), sr); writeWav(r / "Kick_X.wav", kickSig(sr), sr, 24); writeWav(r / "Pad_C_100bpm.wav", chordSig(sr, { 130.81, 164.81, 196.0 }, 3), sr);
    touch(r / "broken.wav", 50); touch(r / "song.mp3");
    Library L; L.scan(r.string()); int ok = 0; for (auto& s : L.samples) if (L.analyseFile(s)) ++ok; CHECK(ok == 3, "3 wav files analysed, broken and mp3 skipped (%d)", ok);
    auto* m = L.byRel("mystery.wav"); CHECK(m->analysed && std::fabs(m->bpm - 128) < 1.5 && m->duration > 7.9 && m->typeSrc == "audio", "mystery: bpm %.1f dur %.2f src %s", m->bpm, m->duration, m->typeSrc.c_str());
    auto* kx = L.byRel("Kick_X.wav"); CHECK(kx->type == "kick" && kx->typeSrc == "name", "24 bit file read, name wins"); auto* pc = L.byRel("Pad_C_100bpm.wav"); CHECK(pc->bpm == 100 && pc->key == "C", "filename bpm and key are not overwritten");
    Audio au; CHECK(!readWav((r / "broken.wav").string(), au) && !readWav((r / "missing.wav").string(), au), "bad and missing wav refused");
    fs::remove_all(r);
}
int main() {
    testWords(); testImport(); testStems(); testAnalysis();
    testGuess(); testNatural(); testScanAndQuery();
    if (fails) { std::printf("%d FAILED\n", fails); return 1; }
    std::printf("all tests passed\n"); return 0;
}
