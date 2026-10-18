#include "CatCrusher.h"
#include "../common/Viz.h"

using namespace catfx;

CatCrusher::CatCrusher()
    : FxBase("Cat Crusher",
        { { "tube", "Tube", " %", 0.f, 100.f, 30.f, 0.f, "Tube: warm, smooth saturation with even harmonics. Higher = more drive and warmth." },
          { "mech", "Mech", " %", 0.f, 100.f, 0.f, 0.f, "Mech: harder, more aggressive digital-style crunch (folds and clips the wave). 0 = off." },
          { "cut", "Cutoff", " Hz", 40.f, 20000.f, 20000.f, 1500.f, "Cutoff: frequency of the 24 dB low-pass filter. Rolls off the harsh highs that distortion creates." },
          { "res", "Res", " %", 0.f, 100.f, 10.f, 0.f, "Resonance: boosts the frequencies right at the cutoff for a vocal or squelchy edge." },
          { "comp", "Amount", " %", 0.f, 100.f, 30.f, 0.f, "Compressor amount: lowers the threshold and raises the ratio together (and adds make-up gain). More = denser and louder." },
          { "vol", "Volume", " dB", -24.f, 12.f, 0.f, 0.f, "Volume: final output level. Match it to the bypassed level when comparing." },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f, 0.f, "Mix: dry/wet blend. Lower values keep the original punch (parallel crush)." } },
        { { "don", "Distortion", { "Off", "On" }, 1, "Distortion on/off." },
          { "fon", "Filter", { "Off", "On" }, 1, "Filter on/off." },
          { "con", "Compressor", { "Off", "On" }, 1, "Compressor on/off." },
          { "phat", "Phat mode", { "Off", "On" }, 0, "Phat mode: adds low-end weight before the compressor, a slower release and a soft ceiling. Great on kicks and bass." } }) {}

std::vector<catfx::PanelSpec> CatCrusher::panels() const
{
    return { { "Distortion", { "tube", "mech" }, { "don" }, 1.f },
             { "Filter", { "cut", "res" }, { "fon" }, 1.f },
             { "Compressor", { "comp" }, { "con", "phat" }, 1.f },
             { "Master", { "vol", "mix" }, {}, 1.f } };
}

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

void CatCrusher::paintPanel(int idx, juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace catfx::viz;
    g.setFont(juce::FontOptions(11.f));
    if (idx == 0)       // distortion curve
    {
        const float tube = get("tube"), mech = get("mech"); const bool on = get("don") > 0.5f;
        auto p = r.withTrimmedLeft(8.f).reduced(10.f, 14.f); p.removeFromBottom(10.f);
        g.setColour(grid()); g.drawRect(p); g.drawLine(p.getX(), p.getCentreY(), p.getRight(), p.getCentreY()); g.drawLine(p.getCentreX(), p.getY(), p.getCentreX(), p.getBottom());
        juce::Path path;
        for (int i = 0; i <= 200; ++i)
        {
            const float x = -1.f + (float) i / 100.f, y = juce::jlimit(-1.2f, 1.2f, on ? distort(x, tube, mech) : x);
            const float px = p.getX() + (x + 1.f) * 0.5f * p.getWidth(), py = p.getCentreY() - y / 1.2f * 0.5f * p.getHeight();
            i ? path.lineTo(px, py) : path.startNewSubPath(px, py);
        }
        g.setColour(accent()); g.strokePath(path, juce::PathStrokeType(2.f));
        const float lv = juce::jlimit(0.f, 1.f, lastIn.load()), yy = juce::jlimit(-1.2f, 1.2f, on ? distort(lv, tube, mech) : lv);
        g.setColour(juce::Colours::white); g.fillEllipse(p.getX() + (lv + 1.f) * 0.5f * p.getWidth() - 3.f, p.getCentreY() - yy / 1.2f * 0.5f * p.getHeight() - 3.f, 6.f, 6.f);
        g.setColour(dim()); g.drawText("input level", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(11), juce::Justification::centred);
        g.drawText("-1", (int) p.getX(), (int) p.getBottom() + 1, 20, 11, juce::Justification::left); g.drawText("+1", (int) p.getRight() - 20, (int) p.getBottom() + 1, 20, 11, juce::Justification::right);
        g.drawText("output", (int) r.getX() + 2, (int) r.getY() + 1, 60, 11, juce::Justification::left);
        g.drawText("gain x" + juce::String(1.f + tube * 0.24f, 1), (int) r.getRight() - 80, (int) r.getY() + 1, 76, 11, juce::Justification::right);
    }
    else if (idx == 1)  // filter response
    {
        const float fc = get("cut"), q = 0.55f + get("res") * 0.01f * 7.f; const bool on = get("fon") > 0.5f;
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(8.f).withTrimmedTop(16.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        dbAxis(g, p, -36.f, 18.f, 12.f); freqAxis(g, p);
        juce::Path f;
        for (int i = 0; i <= 200; ++i)
        {
            const float fr = 20.f * std::pow(1000.f, (float) i / 200.f), rr = fr / fc;
            auto lp = [&](float qq) { return 1.f / std::sqrt((1.f - rr * rr) * (1.f - rr * rr) + (rr / qq) * (rr / qq)); };
            const float mag = on ? lp(q) * lp(0.707f) : 1.f;
            const float db = juce::jlimit(-36.f, 18.f, 20.f * std::log10(std::max(mag, 1e-4f)));
            const float px = p.getX() + (float) i / 200.f * p.getWidth(), py = p.getBottom() - (db + 36.f) / 54.f * p.getHeight();
            i ? f.lineTo(px, py) : f.startNewSubPath(px, py);
        }
        g.setColour(green()); g.strokePath(f, juce::PathStrokeType(2.f));
        g.setColour(dim()); g.drawText("cutoff " + juce::String((int) fc) + " Hz", (int) r.getRight() - 120, (int) r.getY() + 1, 116, 11, juce::Justification::right);
    }
    else if (idx == 2)  // compressor transfer curve and GR meter
    {
        const float am = get("comp") / 100.f, thr = -am * 30.f, ratio = 1.f + am * 7.f, mk = am * (get("phat") > 0.5f ? 9.f : 6.f);
        const bool on = get("con") > 0.5f;
        auto grR = r.removeFromRight(46.f).reduced(2.f, 6.f);
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(6.f).withTrimmedTop(34.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        dbAxis(g, p, -48.f, 12.f, 12.f, false);
        for (float v : { -48.f, -36.f, -24.f, -12.f, 0.f })
        { const float x = p.getX() + (v + 48.f) / 48.f * p.getWidth(); g.setColour(grid()); g.drawVerticalLine((int) x, p.getY(), p.getBottom()); g.setColour(dim()); g.drawText(juce::String((int) v), (int) x - 14, (int) p.getBottom() + 1, 28, 11, juce::Justification::centred); }
        auto yOf = [&](float db) { return p.getBottom() - (juce::jlimit(-48.f, 12.f, db) + 48.f) / 60.f * p.getHeight(); };
        g.setColour(dim().withAlpha(0.6f)); g.drawLine(p.getX(), yOf(-48.f), p.getRight(), yOf(0.f), 1.f);
        juce::Path c;
        for (int i = 0; i <= 96; ++i)
        {
            const float inDb = -48.f + (float) i / 2.f, outDb = on ? (inDb > thr ? thr + (inDb - thr) / ratio : inDb) + mk : inDb;
            const float px = p.getX() + (float) i / 96.f * p.getWidth();
            i ? c.lineTo(px, yOf(outDb)) : c.startNewSubPath(px, yOf(outDb));
        }
        g.setColour(accent()); g.strokePath(c, juce::PathStrokeType(2.f));
        if (on) { const float tx = p.getX() + (thr + 48.f) / 48.f * p.getWidth(); g.setColour(red().withAlpha(0.7f)); g.drawVerticalLine((int) tx, p.getY(), p.getBottom()); }
        const float inDb = juce::Decibels::gainToDecibels(lastIn.load(), -60.f), outDb = on ? (inDb > thr ? thr + (inDb - thr) / ratio : inDb) + mk : inDb;
        g.setColour(juce::Colours::white); g.fillEllipse(p.getX() + (juce::jlimit(-48.f, 0.f, inDb) + 48.f) / 48.f * p.getWidth() - 3.f, yOf(outDb) - 3.f, 6.f, 6.f);
        g.setColour(dim());
        g.drawText(on ? "thr " + juce::String(thr, 0) + " dB   ratio " + juce::String(ratio, 1) + ":1" : "compressor off", (int) r.getX() + 4, (int) r.getY() + 1, (int) r.getWidth(), 11, juce::Justification::left);
        if (on) g.drawText("makeup +" + juce::String(mk, 1) + " dB", (int) r.getX() + 4, (int) r.getY() + 13, (int) r.getWidth(), 11, juce::Justification::left);
        g.drawText("in dB", (int) p.getRight() - 40, (int) p.getBottom() - 14, 40, 11, juce::Justification::right);
        catfx::viz::grMeter(g, grR, grShown.load());
    }
    else                // master: input/output meters and the dry/wet balance
    {
        const float mix = get("mix"), vol = get("vol");
        auto area = r.reduced(6.f, 6.f);
        auto bottom = area.removeFromBottom(34.f);
        auto left = area.removeFromLeft(area.getWidth() * 0.5f);
        meter(g, left, inLevel.load(), "IN dB");
        meter(g, area, outLevel.load(), "OUT dB");
        g.setColour(grid()); g.fillRect(bottom.withHeight(10.f).withY(bottom.getY() + 14.f));
        g.setColour(blue().withAlpha(0.8f)); g.fillRect(bottom.withHeight(10.f).withY(bottom.getY() + 14.f).withWidth(bottom.getWidth() * (1.f - mix * 0.01f)));
        g.setColour(green()); g.fillRect(bottom.withHeight(10.f).withY(bottom.getY() + 14.f).withTrimmedLeft(bottom.getWidth() * (1.f - mix * 0.01f)));
        g.setColour(dim()); g.setFont(juce::FontOptions(10.f));
        g.drawText("dry " + juce::String(100 - (int) mix) + "%", bottom.toNearestInt().withHeight(12), juce::Justification::left);
        g.drawText("wet " + juce::String((int) mix) + "%", bottom.toNearestInt().withHeight(12), juce::Justification::right);
        g.drawText("volume " + juce::String(vol > 0.f ? "+" : "") + juce::String(vol, 1) + " dB", bottom.toNearestInt().withTrimmedTop(24), juce::Justification::centred);
    }
}
