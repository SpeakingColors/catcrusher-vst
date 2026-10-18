#pragma once
#include "common/FxBase.h"

// Cat Crusher: Distortion (Tube + Mech, 2x oversampled) > Filter (24 dB low-pass) > Compressor (with Phat mode) > Master.
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
    static float distort(float x, float tube, float mech);
private:
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::StateVariableTPTFilter<float> filtA[2], filtB[2];
    juce::dsp::IIR::Filter<float> shelf[2];
    float env = 0.f, dc[2] { 0.f, 0.f }, grMeter = 0.f;
    std::vector<float> dryRing[2], dryBuf[2];
    int ringPos = 0, latency = 0;
    std::atomic<float> lastIn { 0.f }, grShown { 0.f };
};
