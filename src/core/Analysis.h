// CATalog: audio analysis and sample classification (pure C++17, no JUCE). Reads PCM wav itself; the app decodes the other formats
// (aiff, flac, mp3, ogg, m4a...) through JUCE and hands the mono samples to analyse(). Tempo comes from the onset curve, key from a
// chroma profile (Krumhansl), type from simple spectral and envelope measures. These are estimates, not certainties.
#pragma once
#include <vector>
#include <string>
#include <complex>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <array>

namespace catalog {

struct Audio { std::vector<float> mono; double sr = 0; };

// Plain PCM / float wav (8, 16, 24, 32 bit int and 32 bit float), any channel count, mixed down to mono. false if not readable.
inline bool readWav(const std::string& file, Audio& out) {
    std::ifstream f(file, std::ios::binary); if (!f) return false;
    char h[12]; f.read(h, 12); if (f.gcount() != 12 || std::memcmp(h, "RIFF", 4) || std::memcmp(h + 8, "WAVE", 4)) return false;
    int fmt = 0, ch = 0, bits = 0; uint32_t sr = 0; bool haveFmt = false;
    for (;;) {
        char id[4]; uint32_t sz; f.read(id, 4); f.read((char*) &sz, 4); if (!f) return false;
        if (!std::memcmp(id, "fmt ", 4)) {
            std::vector<char> b(sz); f.read(b.data(), sz); if (sz < 16) return false;
            uint16_t tag, c, bp; std::memcpy(&tag, &b[0], 2); std::memcpy(&c, &b[2], 2); std::memcpy(&sr, &b[4], 4); std::memcpy(&bp, &b[14], 2);
            if (tag == 0xFFFE && sz >= 26) std::memcpy(&tag, &b[24], 2);
            fmt = tag; ch = c; bits = bp; haveFmt = true; if (sz & 1) f.seekg(1, std::ios::cur);
        } else if (!std::memcmp(id, "data", 4)) {
            if (!haveFmt || ch < 1 || sr == 0) return false;
            if (!((fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32))) return false;
            std::vector<unsigned char> raw(sz); f.read((char*) raw.data(), sz); const size_t got = (size_t) f.gcount();
            const size_t bps = (size_t)(bits / 8), frames = got / (bps * (size_t) ch);
            out.sr = sr; out.mono.assign(frames, 0.f);
            for (size_t i = 0; i < frames; ++i) {
                double acc = 0;
                for (int c = 0; c < ch; ++c) {
                    const unsigned char* p = &raw[(i * (size_t) ch + (size_t) c) * bps]; double v;
                    if (fmt == 3) { float x; std::memcpy(&x, p, 4); v = x; }
                    else if (bits == 8) v = ((int) p[0] - 128) / 128.0;
                    else if (bits == 16) v = (int16_t)(p[0] | (p[1] << 8)) / 32768.0;
                    else if (bits == 24) { int32_t x = (p[0] << 8) | (p[1] << 16) | ((int32_t) p[2] << 24); v = x / 2147483648.0; }
                    else { int32_t x; std::memcpy(&x, p, 4); v = x / 2147483648.0; }
                    acc += v;
                }
                out.mono[i] = (float)(acc / ch);
            }
            return true;
        } else { f.seekg((std::streamoff) sz + (sz & 1), std::ios::cur); if (!f) return false; }
    }
}

struct AudioInfo {
    bool ok = false;
    double duration = 0;       // seconds
    double bpm = 0;            // 0 = no clear tempo (one shots, too few hits)
    std::string key;           // "Am", "C#", "" = not tonal enough
    double keyStrength = 0;
    std::string type;          // kick, snare, hat, ... (from the sound itself)
    double centroid = 0, lowShare = 0, highShare = 0, flatness = 0, decay = 0, f0 = 0;
    int onsets = 0; bool tonal = false;
};

namespace detail {
inline void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2 * 3.14159265358979323846 / (double) len; const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w(1); for (size_t k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; } }
    }
}
inline std::vector<double> mags(const std::vector<float>& x, size_t from, size_t N) {
    std::vector<std::complex<double>> a(N); for (size_t i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos(2 * 3.14159265358979323846 * (double) i / (double) N); a[i] = (from + i < x.size() ? x[from + i] : 0.f) * w; }
    fft(a); std::vector<double> m(N / 2); for (size_t i = 0; i < N / 2; ++i) m[i] = std::abs(a[i]); return m;
}
inline double corr(const double* a, const double* b, int n) { double ma = 0, mb = 0; for (int i = 0; i < n; ++i) { ma += a[i]; mb += b[i]; } ma /= n; mb /= n; double s = 0, va = 0, vb = 0; for (int i = 0; i < n; ++i) { s += (a[i] - ma) * (b[i] - mb); va += (a[i] - ma) * (a[i] - ma); vb += (b[i] - mb) * (b[i] - mb); } return (va > 0 && vb > 0) ? s / std::sqrt(va * vb) : 0; }
}

// Key from a chroma profile, returned as "Am" / "C#" (same spelling the filename guesser uses, sharps only).
inline std::string detectKey(const std::vector<float>& x, double sr, double* strengthOut = nullptr) {
    static const char* nm[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const double maj[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 }, mnr[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    const size_t N = 8192; if (x.size() < N / 2 || sr <= 0) return "";
    double ch[12] = {}; const double bh = sr / (double) N; const size_t lo = (size_t)(60 / bh), hi = (size_t)(std::min(2500.0, sr / 2 - 100) / bh);
    const size_t hop = std::max<size_t>(4096, x.size() / 40);
    for (size_t p = 0; p == 0 || p + N <= x.size(); p += hop) {
        auto m = detail::mags(x, p, N); double mx = 0; for (size_t b = lo; b < hi && b < m.size(); ++b) mx = std::max(mx, m[b]); if (mx <= 0) continue;
        for (size_t b = lo + 1; b + 1 < hi && b + 1 < m.size(); ++b) if (m[b] > m[b - 1] && m[b] >= m[b + 1] && m[b] > 0.1 * mx) {
            const double den = m[b - 1] - 2 * m[b] + m[b + 1], d = 0.5 * (m[b - 1] - m[b + 1]) / (den != 0 ? den : 1), f = ((double) b + d) * bh;
            const int mi = (int) std::lround(69 + 12 * std::log2(f / 440)); ch[((mi % 12) + 12) % 12] += m[b] * m[b];
        }
    }
    double tot = 0; for (double v : ch) tot += v; if (tot <= 0) return "";
    double best = -2, second = -2; std::string name;
    for (int r = 0; r < 12; ++r) {
        double pm[12], pn[12]; for (int i = 0; i < 12; ++i) { pm[i] = maj[(i - r + 12) % 12]; pn[i] = mnr[(i - r + 12) % 12]; }
        const double a = detail::corr(ch, pm, 12), b = detail::corr(ch, pn, 12);
        for (int k = 0; k < 2; ++k) { const double v = k ? b : a; if (v > best) { second = best; best = v; name = std::string(nm[r]) + (k ? "m" : ""); } else if (v > second) second = v; }
    }
    if (strengthOut) *strengthOut = best;
    return best > 0.55 ? name : "";
}

// Tempo from the onset curve (spectral flux, then autocorrelation, 70..180 BPM). 0 when there are fewer than 4 clear hits or no clear tempo.
inline double detectBpm(const std::vector<float>& xin, double sr, int* onsetsOut = nullptr) {
    const size_t N = 1024, hop = (size_t) std::max(1.0, std::floor(sr * 0.010)); const double dt = (double) hop / sr;
    const size_t len = std::min(xin.size(), (size_t)(sr * 40)); if (len < N + hop * 16) return 0;
    std::vector<float> x(xin.begin(), xin.begin() + (long) len);
    std::vector<double> prev, fl; 
    for (size_t p = 0; p + N <= len; p += hop) {
        auto m = detail::mags(x, p, N); for (auto& v : m) v = std::log1p(v * 4.0 / (double) N * 1000.0);
        double f = 0; if (!prev.empty()) for (size_t b = 1; b < m.size() / 2; ++b) f += std::max(0.0, m[b] - prev[b]);
        fl.push_back(f); prev = std::move(m);
    }
    double mean = 0, mx = 0; for (double v : fl) { mean += v; mx = std::max(mx, v); } mean /= (double) fl.size();
    double var = 0; for (double v : fl) var += (v - mean) * (v - mean); const double sd = std::sqrt(var / (double) fl.size());
    const double thr = std::max({ 20.0, mean + 2.5 * sd, 0.25 * mx });
    int n = 0; long last = -100;
    for (size_t k = 1; k + 1 < fl.size(); ++k) if (fl[k] > thr && fl[k] >= fl[k - 1] && fl[k] >= fl[k + 1] && (long) k - last > (long) std::max(1.0, 0.08 / dt)) { ++n; last = (long) k; }
    if (onsetsOut) *onsetsOut = n; if (n < 4) return 0;
    for (auto& v : fl) v = std::max(0.0, v - mean);
    double bestB = 0, bs = 0, tot = 0; int cnt = 0;
    for (double bpm = 70; bpm <= 180; bpm += 0.25) {
        const double lag = 60.0 / bpm / dt; double s = 0; int c = 0;
        for (size_t k = 0; k + (size_t) std::ceil(lag) + 1 < fl.size(); ++k) { const double pos = (double) k + lag; const size_t j = (size_t) pos; const double fr = pos - (double) j; s += fl[k] * (fl[j] * (1 - fr) + fl[j + 1] * fr); ++c; }
        if (c < 4) continue; s /= c; tot += s; ++cnt; if (s > bs) { bs = s; bestB = bpm; }
    }
    if (cnt == 0 || bs <= 1.6 * tot / cnt) return 0;       // no clear peak in the autocorrelation
    return std::round(bestB * 2) / 2;
}

inline AudioInfo analyse(const std::vector<float>& x, double sr) {
    AudioInfo r; if (x.size() < 256 || sr < 8000) return r; r.ok = true; r.duration = (double) x.size() / sr;
    double peak = 0; for (float v : x) peak = std::max(peak, (double) std::fabs(v)); if (peak < 1e-4) { r.type = "other"; return r; }
    // envelope: peak time and decay to -30 dB
    const size_t win = (size_t)(sr * 0.005); std::vector<double> e; for (size_t i = 0; i + win <= x.size(); i += win) { double m = 0; for (size_t j = 0; j < win; ++j) m = std::max(m, (double) std::fabs(x[i + j])); e.push_back(m); }
    size_t pi = 0; for (size_t i = 0; i < e.size(); ++i) if (e[i] > e[pi]) pi = i;
    size_t di = pi; while (di < e.size() && e[di] > e[pi] * 0.0316) ++di; r.decay = (double)(di - pi) * (double) win / sr;
    // spectrum of the loudest 0.4 s (or the whole file)
    const size_t N = 4096; size_t from = pi * win > N / 2 ? pi * win - N / 4 : 0; double lo = 0, hi = 0, all = 0, cn = 0, lg = 0; int nb = 0; const double bh = sr / (double) N;
    for (size_t p = from; p < x.size() && p < from + (size_t)(sr * 0.4); p += N / 2) {
        auto m = detail::mags(x, p, N);
        for (size_t b = 1; b < m.size(); ++b) { const double f = (double) b * bh, pw = m[b] * m[b]; all += pw; cn += pw * f; if (f < 150) lo += pw; if (f > 6000) hi += pw; if (f > 60 && f < 12000) { lg += std::log(pw + 1e-12); ++nb; } }
    }
    if (all > 0) { r.centroid = cn / all; r.lowShare = lo / all; r.highShare = hi / all; }
    { double am = 0; int c = 0; for (size_t p = from; p < x.size() && p < from + (size_t)(sr * 0.4); p += N / 2) { auto m = detail::mags(x, p, N); for (size_t b = (size_t)(60 / bh); b < m.size() && (double) b * bh < 12000; ++b) { am += m[b] * m[b]; ++c; } } if (c && nb) { const double geo = std::exp(lg / nb), ar = am / (double) std::max(1, c) ; r.flatness = ar > 0 ? std::min(1.0, geo / ar) : 0; } }
    // pitch: strongest low peak of the same window
    { auto m = detail::mags(x, from, N); size_t bb = 0; double bv = 0, tot = 0; for (size_t b = (size_t)(30 / bh); b < m.size() && (double) b * bh < 2000; ++b) { tot += m[b]; if (m[b] > bv) { bv = m[b]; bb = b; } } r.f0 = bb * bh; r.tonal = tot > 0 && bv / tot > 0.08; }
    int on = 0; r.bpm = detectBpm(x, sr, &on); r.onsets = on;
    const bool loop = r.duration >= 1.6 && on >= 4;
    if ((r.tonal || loop) && !(loop && r.lowShare > 0.6)) { double st = 0; r.key = detectKey(x, sr, &st); r.keyStrength = st; }   // a kick pattern has no key
    // type from the sound itself
    if (loop) r.type = (r.lowShare + r.highShare > 0.5 && r.flatness > 0.02 && !r.tonal) ? "drum loop" : "loop";
    else if (r.duration < 1.6) {
        if (r.lowShare > 0.55 && r.centroid < 700 && r.decay < 1.5) r.type = r.f0 < 90 && r.decay > 0.5 && r.tonal ? "bass" : "kick";
        else if (r.highShare > 0.35 && r.centroid > 4500 && r.decay < 0.8) r.type = "hat";
        else if (r.tonal && r.decay < 0.35 && r.centroid > 300) r.type = "stab";
        else if (!r.tonal && r.centroid > 1200 && r.centroid < 6000 && r.decay < 0.6) r.type = r.flatness > 0.15 ? "snare" : "perc";
        else if (r.tonal && r.f0 < 140) r.type = "bass";
        else if (r.tonal) r.type = "synth";
        else r.type = r.decay < 0.5 ? "perc" : "fx";
    } else {
        if (r.tonal && r.f0 < 140 && r.centroid < 900) r.type = "bass";
        else if (r.tonal) r.type = r.decay > 2.5 ? "pad" : "synth";
        else r.type = "fx";
    }
    return r;
}

}
