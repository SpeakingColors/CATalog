# Roadmap: CATalog

Sample manager.

## Done
- [x] Library model in pure C++ with unit tests (scan, name guesses, search, tags, favourites, ratings, saved metadata)
- [x] Release packaging: installers (Windows, macOS, Linux Flatpak extension), uninstallers and PDF manual built by the release workflow (test builds, not tested on real machines)
- [x] Import folders with all subfolders, more formats, type / genre / BPM / key analysis, stems found next to loops, Splice folder watch (core, tested)
- [x] JUCE app / plugin shell with folder choice, search and audition, CI (core tests, configure, build)

## Next
- [ ] Try it on a real sample folder; tune the name guesses
- [ ] Tune type, BPM and key analysis on real sample libraries
- [x] cat-ui editor v1 (library, filters, preview, drag to the DAW)
- [x] editor: Stems and Splice tabs, spectrum, level, pitch, volume and tempo sync preview (built, not heard yet)
- [ ] editor: send to SupurrSaw, saved searches (preset bar), Settings, Add files
- [ ] Drag a sample out to the DAW
- [ ] Waveform library shared with SupurrSaw (single-cycle WAVs)

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
