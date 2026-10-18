#pragma once
#include "common/FxBase.h"

// Cat Crusher: bass shelf > compressor > drive and waveshaper (2x oversampled) > multimode filter.
// Original design; no code or presets from any other product.
class CatCrusher : public catfx::FxBase
{
public:
    CatCrusher();
    void prepare(double sr, int maxBlock) override;
    void process(juce::AudioBuffer<float>&, const juce::AudioBuffer<float>*) override;
    void paintViz(juce::Graphics&, juce::Rectangle<float>) override;
    juce::String vizCaption() const override { return "Waveshaper curve and filter response (live)"; }
    static float shape(int type, float x);
private:
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::StateVariableTPTFilter<float> filt[2];
    juce::dsp::IIR::Filter<float> shelf[2];
    float env = 0.f, dc[2] { 0.f, 0.f };
    std::vector<float> dryRing[2], dryBuf[2];
    int ringPos = 0, latency = 0;
    std::atomic<float> lastIn { 0.f };
};
