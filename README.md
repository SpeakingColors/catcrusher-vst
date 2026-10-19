# Cat Crusher 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Free, open source distortion, compressor and filter plugin for Linux (VST3 and standalone). Bass shelf, compressor, drive into tube, fold or hard clip shapers (2x oversampled), and a multimode filter, with a live waveshaper and filter display.

## Status
Early scaffold. It builds in CI (Linux VST3 and standalone) but has not been tuned by ear yet.

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Docs
- [Roadmap](ROADMAP.md)

## Build
CMake 3.22+, a C++17 compiler and the usual JUCE Linux libraries. `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`. JUCE and the shared cat-ui look are fetched automatically.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Original design. Inspired by the idea of a one-box crusher; it contains no code, presets or artwork from any other product.
