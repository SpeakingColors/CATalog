# Roadmap: CATalog

Sample manager.

## Done
- [x] Library model in pure C++ with unit tests (scan, name guesses, search, tags, favourites, ratings, saved metadata)
- [x] Import folders with all subfolders, more formats, type / genre / BPM / key analysis, stems found next to loops, Splice folder watch (core, tested)
- [x] JUCE app / plugin shell with folder choice, search and audition, CI (core tests, configure, build)

## Next
- [ ] Try it on a real sample folder; tune the name guesses
- [ ] Tune type, BPM and key analysis on real sample libraries
- [ ] cat-ui editor from the mockup (folder tree, tag chips, waveform and spectrogram, preview with pitch and sync) drawn through `catui::RenderGate`
- [ ] Drag a sample out to the DAW
- [ ] Waveform library shared with SupurrSaw (single-cycle WAVs)

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
