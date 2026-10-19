// CATalog: the sample library model (pure C++17, no JUCE). Scans folders, guesses BPM and key from file and folder names,
// keeps your tags / favourites / ratings, and answers search, tag, folder and sort queries.
#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdlib>

namespace catalog {

inline std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char) std::tolower(c); }); return s; }

struct Sample {
    std::string path;            // full path
    std::string rel;             // path relative to the library root, with '/' separators (this is the identity of the sample)
    std::string name;            // file name without extension
    std::string folder;          // relative folder, '' for the root
    std::vector<std::string> tags;
    bool fav = false; int rating = 0;
    double bpm = 0;              // 0 = unknown
    std::string key;             // e.g. "Am", "C#", "" = unknown
    double bytes = 0;
};

inline bool isAudioFile(const std::string& ext) {
    static const std::set<std::string> e { ".wav", ".aif", ".aiff", ".flac", ".ogg", ".mp3" };
    return e.count(lower(ext)) > 0;
}

// "Kick_128bpm_Am.wav" -> bpm 128, key Am. Only plausible values are taken (60..200 BPM).
inline double guessBpm(const std::string& name) {
    const std::string s = lower(name);
    for (size_t i = 0; i < s.size(); ++i) {
        if (!std::isdigit((unsigned char) s[i]) || (i > 0 && std::isdigit((unsigned char) s[i - 1]))) continue;
        size_t j = i; while (j < s.size() && std::isdigit((unsigned char) s[j])) ++j;
        if (j - i > 3) continue;
        size_t k = j; if (k < s.size() && (s[k] == ' ' || s[k] == '_' || s[k] == '-')) ++k;
        if (s.compare(k, 3, "bpm") == 0) { const int v = std::atoi(s.substr(i, j - i).c_str()); if (v >= 60 && v <= 200) return v; }
        if (i >= 4 && s.compare(i - 4, 3, "bpm") == 0 && (s[i - 1] == '_' || s[i - 1] == '-' || s[i - 1] == ' ')) { const int v = std::atoi(s.substr(i, j - i).c_str()); if (v >= 60 && v <= 200) return v; }
    }
    return 0;
}
// A key token set apart by _ - space . or brackets: Am, C#, C#m, Dbmin, F#maj. The note letter must be a capital (so "am" or "fm" in a
// word is not taken), minor needs a lower case m / min / minor, and a bare note ("A", "Kick_A") only counts if the name also says "key".
inline std::string guessKey(const std::string& name) {
    std::vector<std::string> tok; std::string cur;
    for (char c : name) { if (c == '_' || c == '-' || c == ' ' || c == '.' || c == '(' || c == ')') { if (!cur.empty()) tok.push_back(cur); cur.clear(); } else cur += c; }
    if (!cur.empty()) tok.push_back(cur);
    const bool keyWord = lower(name).find("key") != std::string::npos;
    for (const auto& t : tok) {
        if (t.empty() || t[0] < 'A' || t[0] > 'G') continue;
        std::string note(1, t[0]); size_t p = 1;
        if (p < t.size() && t[p] == '#') { note += '#'; ++p; } else if (p < t.size() && t[p] == 'b') { note += 'b'; ++p; }
        const std::string rest = t.substr(p);
        if (rest == "m" || rest == "min" || rest == "minor") return note + "m";
        if (rest == "maj" || rest == "major") return note;
        if (rest.empty() && (note.size() == 2 || keyWord)) return note;
    }
    return "";
}

struct Query {
    std::string text;                // every word must be found in name, tags, key, bpm or folder
    std::vector<std::string> tags;   // every tag must be on the sample
    std::string folder;              // '' = all; otherwise this folder and everything under it
    bool favOnly = false;
    int minRating = 0;
    double bpmMin = 0, bpmMax = 0;   // 0 = no limit
    std::string sort = "name";       // name, bpm, key, rating, size
};

class Library {
public:
    std::vector<Sample> samples;
    std::string root;

    // Scan a folder tree. Existing user data (tags, favourites, ratings) for the same relative path is kept; folder names become tags once.
    int scan(const std::string& dir, bool folderTags = true) {
        namespace fs = std::filesystem; std::error_code ec; root = dir;
        std::map<std::string, Sample> old; for (auto& s : samples) old[s.rel] = s;
        std::vector<Sample> out;
        for (fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end && !ec; it.increment(ec)) {
            if (!it->is_regular_file(ec) || !isAudioFile(it->path().extension().string())) continue;
            Sample s; s.path = it->path().string(); s.rel = fs::relative(it->path(), dir, ec).generic_string();
            s.name = it->path().stem().string(); const auto par = fs::path(s.rel).parent_path().generic_string(); s.folder = par == "." ? "" : par;
            s.bytes = (double) it->file_size(ec);
            s.bpm = guessBpm(s.name); if (s.bpm == 0) s.bpm = guessBpm(s.folder); s.key = guessKey(s.name);
            auto o = old.find(s.rel);
            if (o != old.end()) { s.tags = o->second.tags; s.fav = o->second.fav; s.rating = o->second.rating; if (o->second.bpm > 0) s.bpm = o->second.bpm; if (!o->second.key.empty()) s.key = o->second.key; }
            else if (folderTags) { std::stringstream ss(s.folder); std::string p; while (std::getline(ss, p, '/')) { p = lower(p); p.erase(std::remove_if(p.begin(), p.end(), [](unsigned char c) { return !(std::isalnum(c) || c == ' ' || c == '-'); }), p.end()); if (!p.empty() && std::find(s.tags.begin(), s.tags.end(), p) == s.tags.end()) s.tags.push_back(p); } }
            out.push_back(std::move(s));
        }
        std::sort(out.begin(), out.end(), [](const Sample& a, const Sample& b) { return a.rel < b.rel; });
        samples = std::move(out); return (int) samples.size();
    }

    std::vector<const Sample*> find(const Query& q) const {
        std::vector<std::string> words; { std::stringstream ss(lower(q.text)); std::string w; while (ss >> w) words.push_back(w); }
        std::vector<const Sample*> r;
        for (const auto& s : samples) {
            if (!q.folder.empty() && !(s.folder == q.folder || s.folder.compare(0, q.folder.size() + 1, q.folder + "/") == 0)) continue;
            if (q.favOnly && !s.fav) continue; if (s.rating < q.minRating) continue;
            if (q.bpmMin > 0 && s.bpm < q.bpmMin) continue; if (q.bpmMax > 0 && (s.bpm <= 0 || s.bpm > q.bpmMax)) continue;
            bool ok = true; for (const auto& t : q.tags) if (std::find(s.tags.begin(), s.tags.end(), lower(t)) == s.tags.end()) { ok = false; break; } if (!ok) continue;
            if (!words.empty()) { std::string hay = lower(s.name + " " + s.key + " " + s.folder + " " + (s.bpm > 0 ? std::to_string((int) s.bpm) : "")); for (auto& t : s.tags) hay += " " + t; for (auto& w : words) if (hay.find(w) == std::string::npos) { ok = false; break; } if (!ok) continue; }
            r.push_back(&s);
        }
        auto byName = [](const Sample* a, const Sample* b) { return natural(a->name, b->name); };
        if (q.sort == "bpm") std::stable_sort(r.begin(), r.end(), [&](const Sample* a, const Sample* b) { return a->bpm != b->bpm ? a->bpm > b->bpm : byName(a, b); });
        else if (q.sort == "key") std::stable_sort(r.begin(), r.end(), [&](const Sample* a, const Sample* b) { return a->key != b->key ? a->key < b->key : byName(a, b); });
        else if (q.sort == "rating") std::stable_sort(r.begin(), r.end(), [&](const Sample* a, const Sample* b) { return a->rating != b->rating ? a->rating > b->rating : byName(a, b); });
        else if (q.sort == "size") std::stable_sort(r.begin(), r.end(), [&](const Sample* a, const Sample* b) { return a->bytes > b->bytes; });
        else std::stable_sort(r.begin(), r.end(), byName);
        return r;
    }
    // "kick 2" before "kick 10"; case-insensitive
    static bool natural(const std::string& a, const std::string& b) {
        size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (std::isdigit((unsigned char) a[i]) && std::isdigit((unsigned char) b[j])) {
                size_t i2 = i, j2 = j; while (i2 < a.size() && std::isdigit((unsigned char) a[i2])) ++i2; while (j2 < b.size() && std::isdigit((unsigned char) b[j2])) ++j2;
                const unsigned long long x = std::strtoull(a.substr(i, i2 - i).c_str(), nullptr, 10), y = std::strtoull(b.substr(j, j2 - j).c_str(), nullptr, 10);
                if (x != y) return x < y; i = i2; j = j2;
            } else { const char x = (char) std::tolower((unsigned char) a[i]), y = (char) std::tolower((unsigned char) b[j]); if (x != y) return x < y; ++i; ++j; }
        }
        return a.size() - i < b.size() - j;
    }
    // folder -> number of samples in it and below it
    std::map<std::string, int> folderCounts() const {
        std::map<std::string, int> c; for (auto& s : samples) { std::string acc; std::stringstream ss(s.folder); std::string p; while (std::getline(ss, p, '/')) { acc = acc.empty() ? p : acc + "/" + p; ++c[acc]; } } return c;
    }
    // tag -> count, most used first
    std::vector<std::pair<std::string, int>> tagCounts() const {
        std::map<std::string, int> c; for (auto& s : samples) for (auto& t : s.tags) ++c[t];
        std::vector<std::pair<std::string, int>> v(c.begin(), c.end()); std::stable_sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; }); return v;
    }
    Sample* byRel(const std::string& rel) { for (auto& s : samples) if (s.rel == rel) return &s; return nullptr; }
    static void addTag(Sample& s, std::string t) { t = lower(t); t.erase(std::remove_if(t.begin(), t.end(), [](unsigned char c) { return !(std::isalnum(c) || c == ' ' || c == '-'); }), t.end()); if (!t.empty() && std::find(s.tags.begin(), s.tags.end(), t) == s.tags.end()) s.tags.push_back(t); }
    static void removeTag(Sample& s, const std::string& t) { s.tags.erase(std::remove(s.tags.begin(), s.tags.end(), lower(t)), s.tags.end()); }

    // Your data only (not the audio): one line per sample, tab separated: rel, fav, rating, bpm, key, tags comma separated. Plain text, easy to back up and diff.
    bool saveMeta(const std::string& file) const {
        std::ofstream o(file, std::ios::trunc); if (!o) return false; o << "CATalog meta v1\n";
        for (auto& s : samples) { if (s.tags.empty() && !s.fav && s.rating == 0) continue; o << s.rel << '\t' << (s.fav ? 1 : 0) << '\t' << s.rating << '\t' << s.bpm << '\t' << s.key << '\t'; for (size_t i = 0; i < s.tags.size(); ++i) o << (i ? "," : "") << s.tags[i]; o << '\n'; }
        return (bool) o;
    }
    // Applies saved data to the samples already scanned. Returns how many samples were touched.
    int loadMeta(const std::string& file) {
        std::ifstream in(file); if (!in) return 0; std::string line; std::getline(in, line); if (line != "CATalog meta v1") return 0; int n = 0;
        while (std::getline(in, line)) {
            std::vector<std::string> f; std::string cur; for (char c : line) { if (c == '\t') { f.push_back(cur); cur.clear(); } else cur += c; } f.push_back(cur);
            if (f.size() < 6) continue; Sample* s = byRel(f[0]); if (!s) continue;
            s->fav = f[1] == "1"; s->rating = std::max(0, std::min(5, std::atoi(f[2].c_str()))); const double b = std::atof(f[3].c_str()); if (b > 0) s->bpm = b; if (!f[4].empty()) s->key = f[4];
            s->tags.clear(); std::stringstream ts(f[5]); std::string t; while (std::getline(ts, t, ',')) if (!t.empty()) s->tags.push_back(t); ++n;
        }
        return n;
    }
};
}
