#include "CatCrusher.h"

using namespace catfx;

CatCrusher::CatCrusher()
    : FxBase("Cat Crusher",
        { { "drive", "Drive", " dB", 0.f, 40.f, 12.f },
          { "comp", "Compress", " %", 0.f, 100.f, 40.f },
          { "bass", "Bass", " dB", 0.f, 12.f, 0.f },
          { "cut", "Cutoff", " Hz", 60.f, 18000.f, 12000.f, 1500.f },
          { "res", "Resonance", "", 0.f, 1.f, 0.2f },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f },
          { "out", "Output", " dB", -24.f, 12.f, 0.f } },
        { { "type", "Shape", { "Tube", "Fold", "Hard clip" }, 0 },
          { "ftype", "Filter", { "Low-pass", "Band-pass", "High-pass" }, 0 },
          { "fpos", "Filter position", { "After", "Before" }, 0 } }) {}

float CatCrusher::shape(int type, float x)
{
    switch (type)
    {
        case 0: return std::tanh(x + 0.15f) - std::tanh(0.15f);   // asymmetric: even harmonics
        case 1: return std::sin(x);                                // folds back when overdriven
        default: return x / std::pow(1.f + std::pow(std::abs(x), 8.f), 0.125f); // hard knee clip
    }
}

void CatCrusher::prepare(double sr, int bs)
{
    os.initProcessing((size_t) bs);
    os.reset();
    latency = (int) std::lround(os.getLatencyInSamples());
    setLatencySamples(latency);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
    for (auto& f : filt) { f.prepare(spec); f.reset(); }
    for (auto& s : shelf) { s.reset(); s.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 100.0, 0.7071f, 1.f); }
    for (auto& r : dryRing) r.assign(256, 0.f);
    for (auto& d : dryBuf) d.assign((size_t) bs, 0.f);
    ringPos = 0; env = 0.f; dc[0] = dc[1] = 0.f;
}

void CatCrusher::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>*)
{
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    const int type = (int) get("type"), ftype = (int) get("ftype"), fpre = (int) get("fpos");
    const float drive = juce::Decibels::decibelsToGain(get("drive")), amount = get("comp") / 100.f;
    const float mix = get("mix") / 100.f, outG = juce::Decibels::decibelsToGain(get("out"));
    const float bassDb = get("bass"), cut = get("cut"), q = 0.707f + get("res") * 8.f;
    for (int c = 0; c < nch; ++c)
        shelf[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(fs, 100.0, 0.7071f, juce::Decibels::decibelsToGain(bassDb));
    for (auto& f : filt)
    {
        f.setCutoffFrequency(cut); f.setResonance(q);
        f.setType(ftype == 0 ? juce::dsp::StateVariableTPTFilterType::lowpass
                  : ftype == 1 ? juce::dsp::StateVariableTPTFilterType::bandpass : juce::dsp::StateVariableTPTFilterType::highpass);
    }
    // dry copy for the mix, delayed to match the oversampling latency
    float* dryOut[2] = { nullptr, nullptr };
    for (int c = 0; c < nch; ++c)
    {
        for (int i = 0; i < n; ++i)
        {
            auto& r = dryRing[c];
            r[(size_t) ((ringPos + i) & 255)] = buf.getReadPointer(c)[i];
            dryBuf[c][(size_t) i] = r[(size_t) ((ringPos + i - latency) & 255)];
        }
        dryOut[c] = dryBuf[c].data();
    }
    ringPos = (ringPos + n) & 255;

    // bass shelf and compressor (stereo linked)
    const float thr = -amount * 30.f, ratio = 1.f + amount * 6.f, makeup = juce::Decibels::decibelsToGain(amount * 6.f);
    const float aA = std::exp(-1.f / (0.003f * (float) fs)), aR = std::exp(-1.f / (0.12f * (float) fs));
    for (int i = 0; i < n; ++i)
    {
        float pk = 0.f;
        for (int c = 0; c < nch; ++c) { auto* d = buf.getWritePointer(c); d[i] = shelf[c].processSample(d[i]); pk = std::max(pk, std::abs(d[i])); }
        const float a = pk > env ? aA : aR; env = a * env + (1.f - a) * pk;
        const float db = juce::Decibels::gainToDecibels(env, -100.f);
        const float grDb = db > thr ? (db - thr) * (1.f - 1.f / ratio) : 0.f;
        const float g = juce::Decibels::decibelsToGain(-grDb) * makeup;
        for (int c = 0; c < nch; ++c) buf.getWritePointer(c)[i] *= g;
        if (i == 0) lastIn = pk;
    }
    auto runFilter = [&]() { for (int c = 0; c < nch; ++c) { auto* d = buf.getWritePointer(c); for (int i = 0; i < n; ++i) d[i] = filt[c].processSample(0, d[i]); } };
    if (fpre == 1) runFilter();

    juce::dsp::AudioBlock<float> block(buf.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
    auto up = os.processSamplesUp(block);
    for (size_t c = 0; c < up.getNumChannels(); ++c)
    {
        auto* d = up.getChannelPointer(c);
        for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = shape(type, d[i] * drive);
    }
    os.processSamplesDown(block);
    // DC blocker (the tube shape is asymmetric)
    for (int c = 0; c < nch; ++c)
    {
        auto* d = buf.getWritePointer(c);
        for (int i = 0; i < n; ++i) { const float x = d[i]; dc[c] += 0.0005f * (x - dc[c]); d[i] = x - dc[c]; }
    }
    if (fpre == 0) runFilter();
    for (int c = 0; c < nch; ++c)
    {
        auto* d = buf.getWritePointer(c);
        for (int i = 0; i < n; ++i) d[i] = (d[i] * mix + dryOut[c][i] * (1.f - mix)) * outG;
    }
}

void CatCrusher::paintViz(juce::Graphics& g, juce::Rectangle<float> r)
{
    auto left = r.removeFromLeft(r.getWidth() * 0.45f).reduced(4.f, 18.f);
    r.removeFromLeft(12.f);
    auto right = r.withTrimmedRight(20.f).reduced(4.f, 18.f);
    const int type = (int) get("type");
    const float drive = juce::Decibels::decibelsToGain(get("drive"));
    g.setColour(juce::Colour(0xff33403f)); g.drawRect(left); g.drawLine(left.getX(), left.getCentreY(), left.getRight(), left.getCentreY());
    g.drawLine(left.getCentreX(), left.getY(), left.getCentreX(), left.getBottom());
    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = -1.f + (float) i / 100.f, y = juce::jlimit(-1.2f, 1.2f, shape(type, x * drive));
        const float px = left.getX() + (x + 1.f) * 0.5f * left.getWidth(), py = left.getCentreY() - y / 1.2f * 0.5f * left.getHeight();
        i ? p.lineTo(px, py) : p.startNewSubPath(px, py);
    }
    g.setColour(juce::Colour(0xffe8a33d)); g.strokePath(p, juce::PathStrokeType(2.f));
    const float lv = juce::jlimit(0.f, 1.f, lastIn.load()), yy = juce::jlimit(-1.2f, 1.2f, shape(type, lv * drive));
    g.setColour(juce::Colours::white);
    g.fillEllipse(left.getX() + (lv + 1.f) * 0.5f * left.getWidth() - 3.f, left.getCentreY() - yy / 1.2f * 0.5f * left.getHeight() - 3.f, 6.f, 6.f);
    // filter response
    const float fc = get("cut"), q = 0.707f + get("res") * 8.f; const int ft = (int) get("ftype");
    g.setColour(juce::Colour(0xff33403f)); g.drawRect(right);
    juce::Path f;
    for (int i = 0; i <= 200; ++i)
    {
        const float fr = 20.f * std::pow(1000.f, (float) i / 200.f), rr = fr / fc;
        const float den = std::sqrt((1.f - rr * rr) * (1.f - rr * rr) + (rr / q) * (rr / q));
        const float mag = ft == 0 ? 1.f / den : ft == 1 ? (rr / q) / den : rr * rr / den;
        const float db = juce::jlimit(-36.f, 18.f, 20.f * std::log10(std::max(mag, 1e-4f)));
        const float px = right.getX() + (float) i / 200.f * right.getWidth(), py = right.getBottom() - (db + 36.f) / 54.f * right.getHeight();
        i ? f.lineTo(px, py) : f.startNewSubPath(px, py);
    }
    g.setColour(juce::Colour(0xff58e08a)); g.strokePath(f, juce::PathStrokeType(2.f));
}
