#include "CatCrusher.h"

using namespace catfx;

CatCrusher::CatCrusher()
    : FxBase("Cat Crusher",
        { { "tube", "Tube", " %", 0.f, 100.f, 30.f },
          { "mech", "Mech", " %", 0.f, 100.f, 0.f },
          { "cut", "Cutoff", " Hz", 40.f, 20000.f, 20000.f, 1500.f },
          { "res", "Res", " %", 0.f, 100.f, 10.f },
          { "comp", "Amount", " %", 0.f, 100.f, 30.f },
          { "vol", "Volume", " dB", -24.f, 12.f, 0.f },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f } },
        { { "don", "Distortion", { "Off", "On" }, 1 },
          { "fon", "Filter", { "Off", "On" }, 1 },
          { "con", "Compressor", { "Off", "On" }, 1 },
          { "phat", "Phat mode", { "Off", "On" }, 0 } }) {}

std::vector<FxBase::Preset> CatCrusher::presets() const
{
    return {
        { "Init", { } },
        { "Warm Bus", { { "tube", 22.f }, { "comp", 25.f }, { "cut", 14000.f } } },
        { "Gritty Drums", { { "tube", 45.f }, { "mech", 25.f }, { "comp", 55.f }, { "mix", 70.f } } },
        { "Fat Bass", { { "tube", 55.f }, { "cut", 4200.f }, { "res", 15.f }, { "comp", 60.f }, { "phat", 1.f } } },
        { "Radio Crush", { { "tube", 40.f }, { "mech", 60.f }, { "cut", 3500.f }, { "res", 35.f }, { "comp", 70.f } } },
        { "Screech Lead", { { "tube", 70.f }, { "mech", 35.f }, { "cut", 7000.f }, { "res", 45.f }, { "comp", 40.f } } },
        { "Hardstyle Kick Crunch", { { "tube", 75.f }, { "mech", 20.f }, { "cut", 9000.f }, { "comp", 65.f }, { "phat", 1.f }, { "mix", 85.f } } },
        { "UK Hardcore Stab", { { "tube", 50.f }, { "mech", 55.f }, { "cut", 8500.f }, { "res", 25.f }, { "comp", 45.f } } },
        { "Smooth Lows", { { "tube", 30.f }, { "cut", 1800.f }, { "comp", 35.f }, { "phat", 1.f } } },
        { "Mech Mayhem", { { "tube", 30.f }, { "mech", 95.f }, { "cut", 5200.f }, { "res", 50.f }, { "comp", 80.f }, { "mix", 80.f } } } };
}

float CatCrusher::distort(float x, float tube, float mech)
{
    // Tube: asymmetric tanh (even harmonics). Mech: harder, folded and clipped.
    const float tg = 1.f + tube * 0.01f * 24.f;
    float y = std::tanh(x * tg + 0.12f * tube * 0.01f) - std::tanh(0.12f * tube * 0.01f);
    y /= (1.f + 0.18f * (tg - 1.f)) * 0.9f + 0.1f;
    if (mech > 0.f)
    {
        const float m = mech * 0.01f, mg = 1.f + m * 14.f;
        const float z = std::sin(juce::jlimit(-1.6f, 1.6f, y * mg * 0.9f));
        y = y + (z / (0.6f + 0.4f * mg * 0.2f) - y) * juce::jlimit(0.f, 1.f, m * 1.6f);
    }
    return y;
}

void CatCrusher::prepare(double sr, int bs)
{
    os.initProcessing((size_t) bs);
    os.reset();
    latency = (int) std::lround(os.getLatencyInSamples());
    setLatencySamples(latency);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
    for (auto& f : filtA) { f.prepare(spec); f.reset(); f.setType(juce::dsp::StateVariableTPTFilterType::lowpass); }
    for (auto& f : filtB) { f.prepare(spec); f.reset(); f.setType(juce::dsp::StateVariableTPTFilterType::lowpass); }
    for (auto& s : shelf) { s.reset(); s.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 90.0, 0.7071f, 1.f); }
    for (auto& r : dryRing) r.assign(256, 0.f);
    for (auto& d : dryBuf) d.assign((size_t) bs, 0.f);
    ringPos = 0; env = 0.f; dc[0] = dc[1] = 0.f;
}

void CatCrusher::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>*)
{
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    const bool dOn = get("don") > 0.5f, fOn = get("fon") > 0.5f, cOn = get("con") > 0.5f, phat = get("phat") > 0.5f;
    const float tube = get("tube"), mech = get("mech"), amount = get("comp") / 100.f;
    const float mix = get("mix") / 100.f, outG = juce::Decibels::decibelsToGain(get("vol"));
    const float cut = get("cut"), q = 0.55f + get("res") * 0.01f * 7.f;
    for (int c = 0; c < nch; ++c)
        shelf[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(fs, 90.0, 0.7071f, phat ? 2.0f : 1.f);
    for (int c = 0; c < nch; ++c)
    {
        filtA[c].setCutoffFrequency(cut); filtA[c].setResonance(q);
        filtB[c].setCutoffFrequency(cut); filtB[c].setResonance(0.707f);
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
    lastIn = buf.getMagnitude(0, n);

    // 1. distortion (2x oversampled); always run the oversampler so the latency stays constant
    juce::dsp::AudioBlock<float> block(buf.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
    auto up = os.processSamplesUp(block);
    if (dOn)
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer(c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = distort(d[i], tube, mech);
        }
    os.processSamplesDown(block);
    if (dOn)
        for (int c = 0; c < nch; ++c)
        {
            auto* d = buf.getWritePointer(c);
            for (int i = 0; i < n; ++i) { const float x = d[i]; dc[c] += 0.0005f * (x - dc[c]); d[i] = x - dc[c]; }
        }
    // 2. filter: 24 dB low-pass, resonance on the first stage
    if (fOn)
        for (int c = 0; c < nch; ++c)
        {
            auto* d = buf.getWritePointer(c);
            for (int i = 0; i < n; ++i) d[i] = filtB[c].processSample(0, filtA[c].processSample(0, d[i]));
        }
    // 3. compressor (stereo linked); Phat adds low shelf weight, slower release and a soft ceiling
    if (cOn)
    {
        const float thr = -amount * 30.f, ratio = 1.f + amount * 7.f;
        const float makeup = juce::Decibels::decibelsToGain(amount * (phat ? 9.f : 6.f));
        const float aA = std::exp(-1.f / (0.003f * (float) fs)), aR = std::exp(-1.f / ((phat ? 0.22f : 0.12f) * (float) fs));
        float grMax = 0.f;
        for (int i = 0; i < n; ++i)
        {
            float pk = 0.f;
            for (int c = 0; c < nch; ++c) { auto* d = buf.getWritePointer(c); if (phat) d[i] = shelf[c].processSample(d[i]); pk = std::max(pk, std::abs(d[i])); }
            const float a = pk > env ? aA : aR; env = a * env + (1.f - a) * pk;
            const float db = juce::Decibels::gainToDecibels(env, -100.f);
            const float grDb = db > thr ? (db - thr) * (1.f - 1.f / ratio) : 0.f;
            grMax = std::max(grMax, grDb);
            const float g = juce::Decibels::decibelsToGain(-grDb) * makeup;
            for (int c = 0; c < nch; ++c)
            {
                float& s = buf.getWritePointer(c)[i];
                s *= g;
                if (phat) s = std::tanh(s * 0.9f) / 0.9f * 0.95f;
            }
        }
        grShown = grMax;
    }
    else grShown = 0.f;
    // 4. master: mix and volume
    for (int c = 0; c < nch; ++c)
    {
        auto* d = buf.getWritePointer(c);
        for (int i = 0; i < n; ++i) d[i] = (d[i] * mix + dryOut[c][i] * (1.f - mix)) * outG;
    }
}

void CatCrusher::paintViz(juce::Graphics& g, juce::Rectangle<float> r)
{
    auto left = r.removeFromLeft(r.getWidth() * 0.40f).reduced(4.f, 18.f);
    r.removeFromLeft(12.f);
    auto right = r.withTrimmedRight(20.f).reduced(4.f, 18.f);
    auto grBar = right.removeFromRight(10.f); right.removeFromRight(8.f);
    const float tube = get("tube"), mech = get("mech");
    const bool dOn = get("don") > 0.5f;
    g.setColour(juce::Colour(0xff33403f)); g.drawRect(left); g.drawLine(left.getX(), left.getCentreY(), left.getRight(), left.getCentreY());
    g.drawLine(left.getCentreX(), left.getY(), left.getCentreX(), left.getBottom());
    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = -1.f + (float) i / 100.f, y = juce::jlimit(-1.2f, 1.2f, dOn ? distort(x, tube, mech) : x);
        const float px = left.getX() + (x + 1.f) * 0.5f * left.getWidth(), py = left.getCentreY() - y / 1.2f * 0.5f * left.getHeight();
        i ? p.lineTo(px, py) : p.startNewSubPath(px, py);
    }
    g.setColour(juce::Colour(0xffe8a33d)); g.strokePath(p, juce::PathStrokeType(2.f));
    const float lv = juce::jlimit(0.f, 1.f, lastIn.load()), yy = juce::jlimit(-1.2f, 1.2f, dOn ? distort(lv, tube, mech) : lv);
    g.setColour(juce::Colours::white);
    g.fillEllipse(left.getX() + (lv + 1.f) * 0.5f * left.getWidth() - 3.f, left.getCentreY() - yy / 1.2f * 0.5f * left.getHeight() - 3.f, 6.f, 6.f);
    // filter response (two poles pairs: resonant first stage, plain second)
    const float fc = get("cut"), q = 0.55f + get("res") * 0.01f * 7.f; const bool fOn = get("fon") > 0.5f;
    g.setColour(juce::Colour(0xff33403f)); g.drawRect(right);
    juce::Path f;
    for (int i = 0; i <= 200; ++i)
    {
        const float fr = 20.f * std::pow(1000.f, (float) i / 200.f), rr = fr / fc;
        auto lp = [&](float qq) { return 1.f / std::sqrt((1.f - rr * rr) * (1.f - rr * rr) + (rr / qq) * (rr / qq)); };
        const float mag = fOn ? lp(q) * lp(0.707f) : 1.f;
        const float db = juce::jlimit(-36.f, 18.f, 20.f * std::log10(std::max(mag, 1e-4f)));
        const float px = right.getX() + (float) i / 200.f * right.getWidth(), py = right.getBottom() - (db + 36.f) / 54.f * right.getHeight();
        i ? f.lineTo(px, py) : f.startNewSubPath(px, py);
    }
    g.setColour(juce::Colour(0xff58e08a)); g.strokePath(f, juce::PathStrokeType(2.f));
    // gain reduction bar
    g.setColour(juce::Colour(0xff33403f)); g.drawRect(grBar);
    const float gr = juce::jlimit(0.f, 24.f, grShown.load()) / 24.f;
    g.setColour(juce::Colour(0xffff6b6b)); g.fillRect(grBar.withHeight(grBar.getHeight() * gr));
    g.setColour(juce::Colour(0xff9a9690)); g.setFont(juce::FontOptions(10.f)); g.drawText("GR", grBar.getX() - 6.f, grBar.getBottom() + 1.f, grBar.getWidth() + 12.f, 12.f, juce::Justification::centred);
}
