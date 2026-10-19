// Core tests for CATalog. Pure C++, no JUCE. A small library of empty files is built in a temp folder.
#include "../src/core/Library.h"
#include <cstdio>
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
int main() {
    testGuess(); testNatural(); testScanAndQuery();
    if (fails) { std::printf("%d FAILED\n", fails); return 1; }
    std::printf("all tests passed\n"); return 0;
}
