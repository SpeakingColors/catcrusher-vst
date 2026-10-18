# Cat Crusher 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Free, open source distortion, compressor and filter plugin for Linux (VST3 and standalone). Bass shelf, compressor, drive into tube, fold or hard clip shapers (2x oversampled), and a multimode filter, with a live waveshaper and filter display.

## Status
Early scaffold. It builds in CI (Linux VST3 and standalone) but has not been tuned by ear yet. Free and open source. Donations welcome (link coming).

## Docs
- [Roadmap](ROADMAP.md)

## Build
CMake 3.22+, a C++17 compiler and the usual JUCE Linux libraries. `cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build`. JUCE and the shared cat-ui look are fetched automatically.

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Original design. Inspired by the idea of a one-box crusher; it contains no code, presets or artwork from any other product.
