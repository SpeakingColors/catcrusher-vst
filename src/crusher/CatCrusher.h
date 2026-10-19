#pragma once
#include "common/FxBase.h"

// Cat Crusher: Distortion (10 types + Mech, 4x oversampled, smoothed zero-crossing) > Filter (24 dB low-pass, high-pass or band-pass) > Compressor (auto make-up, Phat mode) > Master > hidden soft clipper.
// Four sections, each with its own ON switch, plus presets and Randomize. Original design and code;
// the layout idea is a one-box crusher, nothing is copied from any other product.
class CatCrusher : public catfx::FxBase
{
public:
    CatCrusher();
    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) override;
    std::vector<catfx::PanelSpec> panels() const override;
    void paintPanel(int index, juce::Graphics&, juce::Rectangle<float>) override;
    std::vector<Preset> presets() const override;
    juce::StringArray randomExclude() const override { return { "vol", "don", "fon", "con" }; }
    static float distort(float x, float tube, float mech, int type = 0, bool smooth = true);
private:
    static constexpr int kChunk = 512;                 // fixed work size: no allocation and no dependence on the host block size
    void processChunk(juce::dsp::AudioBlock<float> block);
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false };   // 4x, linear phase: constant latency
    juce::dsp::Oversampling<float> osClip { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false }; // 2x for the hidden clipper
    juce::dsp::StateVariableTPTFilter<float> filtA[2], filtB[2];
    juce::dsp::IIR::Filter<float> shelf[2];
    // parameter smoothing, 15 ms (never above 20 ms so a knob move is heard at once)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smTube, smMech, smCut, smRes, smAmt, smVol, smMix;
    float tubeA[kChunk], mechA[kChunk];
    float env = 0.f, dc[2] { 0.f, 0.f }, grMeter = 0.f;
    float pIn = 0.f, pOut = 0.f, kIn = 0.f, kOut = 0.f, mkGain = 1.f;   // auto make-up followers
    float dryRing[2][1024] {};
    float dryBuf[2][kChunk] {};
    int ringPos = 0, latency = 0, latencyTotal = 0;
    std::atomic<float> lastIn { 0.f }, grShown { 0.f };
};
