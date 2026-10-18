#include "FatCat.h"

using namespace catfx;

FatCat::FatCat()
    : FxBase("Fat Cat",
        { { "drive", "Drive", " %", 0.f, 100.f, 40.f },
          { "fuzz", "Fuzz", " %", 0.f, 100.f, 0.f },
          { "color", "Color", " %", 0.f, 100.f, 0.f },
          { "turbo", "Stereo Turbo", " %", 0.f, 100.f, 0.f },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f },
          { "out", "Output", " dB", -24.f, 12.f, 0.f } },
        {}) {}

std::vector<FxBase::Preset> FatCat::presets() const
{
    return {
        { "Init", { } },
        { "Subtle Glue", { { "drive", 20.f }, { "mix", 60.f } } },
        { "Fat Drums", { { "drive", 55.f }, { "color", 25.f }, { "mix", 75.f } } },
        { "Chonky Bass", { { "drive", 70.f }, { "color", 15.f } } },
        { "Fuzzy Lead", { { "drive", 60.f }, { "fuzz", 55.f }, { "color", 60.f } } },
        { "Wide Pad", { { "drive", 30.f }, { "turbo", 65.f }, { "color", 40.f }, { "mix", 70.f } } },
        { "Wall Of Dirt", { { "drive", 95.f }, { "fuzz", 80.f }, { "color", 50.f } } } };
}

static float colourFreq(float color) { return 150.f * std::pow(40.f, color * 0.01f); } // 150 Hz to 6 kHz

void FatCat::prepare(double sr, int bs)
{
    os.initProcessing((size_t) bs);
    os.reset();
    latency = (int) std::lround(os.getLatencyInSamples());
    setLatencySamples(latency);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
    for (auto& f : band) { f.prepare(spec); f.reset(); f.setType(juce::dsp::StateVariableTPTFilterType::bandpass); }
    for (auto& f : fuzzHp) { f.prepare(spec); f.reset(); f.setType(juce::dsp::StateVariableTPTFilterType::highpass); f.setCutoffFrequency(1500.f); }
    for (auto& r : dryRing) r.assign(256, 0.f);
    for (auto& v : nBuf) v.assign((size_t) bs, 0.f);
    envRing.assign(256, 0.f);
    turboRing.assign(4096, 0.f);
    ringPos = turboPos = 0; env = 0.f; dc[0] = dc[1] = 0.f;
}

void FatCat::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>*)
{
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    const float drive = get("drive") * 0.01f, fuzz = get("fuzz") * 0.01f, color = get("color"), turbo = get("turbo") * 0.01f;
    const float mix = get("mix") * 0.01f, outG = juce::Decibels::decibelsToGain(get("out"));
    const float g = 1.f + drive * 24.f, norm = 1.f / std::tanh(g);
    const float aA = std::exp(-1.f / (0.001f * (float) fs)), aR = std::exp(-1.f / (0.05f * (float) fs));
    const float colourK = color * 0.01f * 1.6f;
    for (int c = 0; c < nch; ++c) { band[c].setCutoffFrequency(colourFreq(color)); band[c].setResonance(1.4f); }

    // dry (delayed to match latency), colour emphasis and envelope normalisation at the base rate
    std::vector<float>& e = envRing;
    for (int i = 0; i < n; ++i)
    {
        float pk = 0.f;
        for (int c = 0; c < nch; ++c)
        {
            const float x = buf.getReadPointer(c)[i];
            dryRing[c][(size_t) ((ringPos + i) & 255)] = x;
            const float y = x + colourK * band[c].processSample(0, x);
            buf.getWritePointer(c)[i] = y;
            pk = std::max(pk, std::abs(y));
        }
        const float a = pk > env ? aA : aR; env = a * env + (1.f - a) * pk;
        e[(size_t) ((ringPos + i) & 255)] = env;
        const float inv = 1.f / (env + 1.0e-5f);
        for (int c = 0; c < nch; ++c) buf.getWritePointer(c)[i] *= inv;
    }
    lastN = std::abs(buf.getSample(0, 0));

    juce::dsp::AudioBlock<float> block(buf.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
    auto up = os.processSamplesUp(block);
    for (size_t c = 0; c < up.getNumChannels(); ++c)
    {
        auto* d = up.getChannelPointer(c);
        for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = std::tanh(juce::jlimit(-8.f, 8.f, d[i]) * g) * norm;
    }
    os.processSamplesDown(block);

    // re-apply the (latency aligned) envelope, add fuzz, mix, turbo, output
    const int dl = std::min(turboRing.size() - 1, (size_t) (turbo * 0.012f * fs));
    for (int i = 0; i < n; ++i)
    {
        const float ev = e[(size_t) ((ringPos + i - latency) & 255)];
        float wet[2] { 0.f, 0.f };
        for (int c = 0; c < nch; ++c)
        {
            float w = buf.getReadPointer(c)[i] * ev;
            dc[c] += 0.0005f * (w - dc[c]); w -= dc[c];
            if (fuzz > 0.f)
            {
                const float h = fuzzHp[c].processSample(0, w);
                w += fuzz * 0.5f * std::tanh(h / (ev + 1.0e-5f) * (4.f + 24.f * fuzz)) * ev;
            }
            const float dry = dryRing[c][(size_t) ((ringPos + i - latency) & 255)];
            wet[c] = (w * mix + dry * (1.f - mix)) * outG;
        }
        if (nch == 2 && dl > 0)
        {
            turboRing[(size_t) turboPos] = wet[1];
            wet[1] = turboRing[(size_t) ((turboPos - dl + (int) turboRing.size()) % (int) turboRing.size())];
            turboPos = (turboPos + 1) % (int) turboRing.size();
        }
        for (int c = 0; c < nch; ++c) buf.getWritePointer(c)[i] = wet[c];
    }
    ringPos = (ringPos + n) & 255;
}

void FatCat::paintViz(juce::Graphics& g, juce::Rectangle<float> r)
{
    auto left = r.removeFromLeft(r.getWidth() * 0.42f).reduced(4.f, 18.f);
    r.removeFromLeft(12.f);
    auto right = r.withTrimmedRight(20.f).reduced(4.f, 18.f);
    const float gn = 1.f + get("drive") * 0.01f * 24.f, norm = 1.f / std::tanh(gn);
    g.setColour(juce::Colour(0xff33403f)); g.drawRect(left);
    g.drawLine(left.getX(), left.getBottom(), left.getRight(), left.getY());   // straight line = no change
    juce::Path p;
    for (int i = 0; i <= 160; ++i)
    {
        const float x = (float) i / 160.f, y = std::tanh(x * gn) * norm;
        const float px = left.getX() + x * left.getWidth(), py = left.getBottom() - y * left.getHeight();
        i ? p.lineTo(px, py) : p.startNewSubPath(px, py);
    }
    g.setColour(juce::Colour(0xffe8a33d)); g.strokePath(p, juce::PathStrokeType(2.f));
    const float lv = juce::jlimit(0.f, 1.f, lastN.load()), yy = std::tanh(lv * gn) * norm;
    g.setColour(juce::Colours::white);
    g.fillEllipse(left.getX() + lv * left.getWidth() - 3.f, left.getBottom() - yy * left.getHeight() - 3.f, 6.f, 6.f);
    // colour band (where the emphasis sits)
    g.setColour(juce::Colour(0xff33403f)); g.drawRect(right);
    const float fc = colourFreq(get("color")), k = get("color") * 0.01f * 1.6f;
    juce::Path f;
    for (int i = 0; i <= 200; ++i)
    {
        const float fr = 20.f * std::pow(1000.f, (float) i / 200.f), rr = fr / fc, q = 1.4f;
        const float bp = (rr / q) / std::sqrt((1.f - rr * rr) * (1.f - rr * rr) + (rr / q) * (rr / q));
        const float db = juce::jlimit(-6.f, 18.f, 20.f * std::log10(std::max(1.f + k * bp, 1e-3f)));
        const float px = right.getX() + (float) i / 200.f * right.getWidth(), py = right.getBottom() - (db + 6.f) / 24.f * right.getHeight();
        i ? f.lineTo(px, py) : f.startNewSubPath(px, py);
    }
    g.setColour(juce::Colour(0xff58e08a)); g.strokePath(f, juce::PathStrokeType(2.f));
}
