# CATalog 🐈

<img src="assets/catalog-logo.png" width="160" alt="logo">

Sample manager for your studio (standalone and VST3): scan your sample folders, search, tag, rate, favourite, and listen.

## Status
Early. What exists today:
- A tested pure C++ library model (`src/core`): folder scan (wav, aiff, flac, ogg, mp3), BPM and key guessed from file names, folder names become tags, natural sort, search by words, tags, folder, favourites, rating, BPM range, and your tags / favourites / ratings kept in a plain text file that survives rescans.
- A JUCE app and plugin shell (`src/catalog`): choose a folder, search, click to listen, double click for favourite. JUCE's plain list for now.
- Not yet: the cat-ui editor from the [mockup](docs/mockup/) (folder tree, tag chips, waveform and spectrogram, key and BPM sync preview), drag to the DAW, analysing BPM and key from the audio itself, the shared single-cycle wave library with SupurrSaw. Nothing here has been tried by ear or with a real sample library yet.

Run the model tests: `sh tests/run.sh` (needs only g++).

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Docs
- [Interactive mockups](docs/README.md)
- [Roadmap](ROADMAP.md)

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Not affiliated with or endorsed by any hardware maker named here. All trademarks belong to their owners.
