#include "CatCrusher.h"
#include "../common/Viz.h"

using namespace catfx;

CatCrusher::CatCrusher()
    : FxBase("Cat Crusher",
        { { "tube", "Drive", " %", 0.f, 100.f, 30.f, 0.f, "Drive: how hard the selected distortion type is pushed. Higher = more saturation and harmonics." },
          { "mech", "Mech", " %", 0.f, 100.f, 0.f, 0.f, "Mech: harder, more aggressive digital-style crunch (folds and clips the wave). 0 = off." },
          { "cut", "Cutoff", " Hz", 40.f, 20000.f, 20000.f, 1500.f, "Cutoff: frequency of the 24 dB filter (low-pass, high-pass or band-pass). As a low-pass it rolls off the harsh highs that distortion creates." },
          { "res", "Res", " %", 0.f, 100.f, 10.f, 0.f, "Resonance: boosts the frequencies right at the cutoff for a vocal or squelchy edge." },
          { "comp", "Amount", " %", 0.f, 100.f, 30.f, 0.f, "Compressor amount: lowers the threshold and raises the ratio together. Make-up gain is automatic, so the level stays the same at any amount and only the density changes." },
          { "vol", "Volume", " dB", -24.f, 12.f, 0.f, 0.f, "Volume: final output level. Match it to the bypassed level when comparing." },
          { "mix", "Mix", " %", 0.f, 100.f, 100.f, 0.f, "Mix: dry/wet blend. Lower values keep the original punch (parallel crush)." } },
        { { "type", "Type", { "Tube", "Soft Sat", "Transistor", "Tape", "Diode", "Hard Clip", "Fuzz", "RAT", "Overdrive", "Rectifier" }, 0, "Distortion type. Tube: warm even harmonics. Soft Sat: smooth and symmetric, great on buses. Transistor: console-style, punchy and forward. Tape: gentle and rounded. Diode: edgy and asymmetric. Hard Clip: tight, loud and aggressive. Fuzz: thick, gated and woolly. RAT: hard-clipped, gritty distortion pedal style. Overdrive: smooth, mid-forward tube-pedal style. Rectifier: adds a bright octave-ish edge." },
          { "don", "Distortion", { "Off", "On" }, 1, "Distortion on/off." },
          { "ftype", "Filter type", { "Low-pass", "High-pass", "Band-pass" }, 0, "Filter type: Low-pass keeps the lows and cuts the highs, High-pass keeps the highs and cuts the lows, Band-pass keeps only a band around the cutoff." },
          { "fon", "Filter", { "Off", "On" }, 1, "Filter on/off." },
          { "con", "Compressor", { "Off", "On" }, 1, "Compressor on/off." },
          { "phat", "Phat mode", { "Off", "On" }, 0, "Phat mode: adds low-end weight before the compressor, a slower release and a soft ceiling. Great on kicks and bass." } }) {}

std::vector<catfx::PanelSpec> CatCrusher::panels() const
{
    return { { "Distortion", { "tube", "mech" }, { "don", "type" }, 1.f },
             { "Filter", { "cut", "res" }, { "ftype", "fon" }, 1.f },
             { "Compressor", { "comp" }, { "con", "phat" }, 1.f },
             { "Master", { "vol", "mix" }, {}, 1.f } };
}

std::vector<FxBase::Preset> CatCrusher::presets() const
{
    return {
        { "Init", { } },
        { "Warm Bus", { { "type", 3.f }, { "tube", 22.f }, { "comp", 25.f }, { "cut", 14000.f } } },
        { "Gritty Drums", { { "type", 2.f }, { "tube", 45.f }, { "mech", 25.f }, { "comp", 55.f }, { "mix", 70.f } } },
        { "Fat Bass", { { "type", 2.f }, { "tube", 55.f }, { "cut", 4200.f }, { "res", 15.f }, { "comp", 60.f }, { "phat", 1.f } } },
        { "Radio Crush", { { "tube", 40.f }, { "mech", 60.f }, { "cut", 3500.f }, { "res", 35.f }, { "comp", 70.f } } },
        { "Screech Lead", { { "tube", 70.f }, { "mech", 35.f }, { "cut", 7000.f }, { "res", 45.f }, { "comp", 40.f } } },
        { "Hardstyle Kick Crunch", { { "type", 4.f }, { "tube", 75.f }, { "mech", 20.f }, { "cut", 9000.f }, { "comp", 65.f }, { "phat", 1.f }, { "mix", 85.f } } },
        { "UK Hardcore Stab", { { "tube", 50.f }, { "mech", 55.f }, { "cut", 8500.f }, { "res", 25.f }, { "comp", 45.f } } },
        { "Smooth Lows", { { "type", 1.f }, { "tube", 30.f }, { "cut", 1800.f }, { "comp", 35.f }, { "phat", 1.f } } },
        { "Telephone Crunch", { { "ftype", 2.f }, { "tube", 45.f }, { "mech", 40.f }, { "cut", 1800.f }, { "res", 45.f }, { "comp", 50.f } } },
        { "Mech Mayhem", { { "tube", 30.f }, { "mech", 95.f }, { "cut", 5200.f }, { "res", 50.f }, { "comp", 80.f }, { "mix", 80.f } } } };
}

float CatCrusher::distort(float x, float tube, float mech, int type, bool smooth)
{
    // Ten saturation shapes, all original maths. Every one is driven by the same gain and scaled by the same
    // normaliser so switching type keeps the level in the same ballpark. Mech (fold/clip) comes after.
    // smooth rounds the slope jump at zero in Transistor, Diode and Rectifier (oversampling cannot fix that kink).
    const float tg = 1.f + tube * 0.01f * 24.f, u = x * tg, au = std::abs(u);
    const float e = 0.04f * std::max(1.f, tg * 0.25f);
    const float sgn = smooth ? u / std::sqrt(u * u + e * e) : (u >= 0.f ? 1.f : -1.f);
    float y;
    switch (type)
    {
        case 1:  y = std::tanh(u); break;                                                            // soft sat: symmetric, odd harmonics
        case 2:  { const float a = u / (1.f + au), b = u / (1.f + 0.55f * au) * 1.25f; y = smooth ? 0.5f * ((1.f + sgn) * a + (1.f - sgn) * b) : (u >= 0.f ? a : b); y = juce::jlimit(-1.2f, 1.f, y); break; } // transistor: asymmetric knee
        case 3:  y = u / std::pow(1.f + std::pow(au, 1.6f), 1.f / 1.6f); break;                      // tape: wide, rounded knee
        case 4:  { const float b = -0.8f * (1.f - std::exp(0.5f * std::min(u, 0.f))), pa = 1.f - std::exp(-std::max(u, 0.f)); y = smooth ? 0.5f * ((1.f + sgn) * pa + (1.f - sgn) * b) : (u >= 0.f ? 1.f - std::exp(-u) : b); break; } // diode: exponential, uneven
        case 5:  y = u / std::pow(1.f + std::pow(au, 6.f), 1.f / 6.f); break;                        // hard clip: tight, sharp knee
        case 6:  { const float a = std::tanh(2.f * u + 0.35f) - std::tanh(0.35f); y = a / std::pow(1.f + std::pow(std::abs(1.6f * a), 4.f), 0.25f); break; }  // fuzz: two uneven stages
        case 7:  { const float w = u / std::sqrt(1.f + (u / 3.f) * (u / 3.f)); y = std::asinh(2.5f * w) * 0.37f; break; }                               // RAT: op-amp limit, then hard diodes
        case 8:  y = std::asinh(1.5f * u) * 0.55f; break;                                                                                                // overdrive: soft symmetric log clip
        case 9:  { const float sa = smooth ? std::sqrt(u * u + e * e) - e : au; y = 0.65f * std::tanh(u) + 0.35f * std::tanh(sa); break; }               // rectifier: folds half the wave up
        default: y = std::tanh(u + 0.12f * tube * 0.01f) - std::tanh(0.12f * tube * 0.01f); break;   // tube: biased tanh, even harmonics
    }
    y /= (1.f + 0.18f * (tg - 1.f)) * 0.9f + 0.1f;
    if (mech > 0.f)
    {
        const float m = mech * 0.01f, mg = 1.f + m * 14.f;
        const float z = std::sin(juce::jlimit(-1.6f, 1.6f, y * mg * 0.9f));
        y = y + (z / (0.6f + 0.4f * mg * 0.2f) - y) * juce::jlimit(0.f, 1.f, m * 1.6f);
    }
    return y;
}

void CatCrusher::prepare(double sr, int)
{
    os.initProcessing((size_t) kChunk); os.reset();
    osClip.initProcessing((size_t) kChunk); osClip.reset();
    latency = (int) std::lround(os.getLatencyInSamples());                         // the dry path is aligned to the distortion oversampler only
    latencyTotal = latency + (int) std::lround(osClip.getLatencyInSamples());      // plus the hidden clipper's oversampler
    setLatencySamples(latencyTotal);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) kChunk, 1 };
    for (auto& f : filtA) { f.prepare(spec); f.reset(); f.setType(juce::dsp::StateVariableTPTFilterType::lowpass); }
    for (auto& f : filtB) { f.prepare(spec); f.reset(); f.setType(juce::dsp::StateVariableTPTFilterType::lowpass); }
    for (auto& s : shelf) { s.reset(); s.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, 90.0, 0.7071f, 1.f); }
    for (auto& r : dryRing) for (auto& v : r) v = 0.f;
    ringPos = 0; env = 0.f; dc[0] = dc[1] = 0.f; pIn = pOut = kIn = kOut = 0.f; mkGain = 1.f;
    const double sm = 0.015;                                                        // 15 ms: fast enough to feel instant, slow enough to avoid zipper noise
    smTube.reset(sr, sm); smMech.reset(sr, sm); smCut.reset(sr, sm); smRes.reset(sr, sm); smAmt.reset(sr, sm); smVol.reset(sr, sm); smMix.reset(sr, sm);
    smTube.setCurrentAndTargetValue(get("tube")); smMech.setCurrentAndTargetValue(get("mech")); smCut.setCurrentAndTargetValue(get("cut"));
    smRes.setCurrentAndTargetValue(get("res")); smAmt.setCurrentAndTargetValue(get("comp") / 100.f);
    smVol.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(get("vol"))); smMix.setCurrentAndTargetValue(get("mix") / 100.f);
}

void CatCrusher::process(juce::AudioBuffer<float>& buf, const juce::AudioBuffer<float>*)
{
    const int n = buf.getNumSamples(), nch = juce::jmin(2, buf.getNumChannels());
    lastIn = buf.getMagnitude(0, n);
    juce::dsp::AudioBlock<float> all(buf.getArrayOfWritePointers(), (size_t) nch, (size_t) n);
    for (int pos = 0; pos < n; pos += kChunk)
        processChunk(all.getSubBlock((size_t) pos, (size_t) juce::jmin(kChunk, n - pos)));
}

void CatCrusher::processChunk(juce::dsp::AudioBlock<float> block)
{
    const int n = (int) block.getNumSamples(), nch = (int) block.getNumChannels();
    const bool dOn = get("don") > 0.5f, fOn = get("fon") > 0.5f, cOn = get("con") > 0.5f, phat = get("phat") > 0.5f;
    const int dtype = (int) get("type"), ftype = (int) get("ftype");
    smTube.setTargetValue(get("tube")); smMech.setTargetValue(get("mech")); smCut.setTargetValue(get("cut")); smRes.setTargetValue(get("res"));
    smAmt.setTargetValue(get("comp") / 100.f); smVol.setTargetValue(juce::Decibels::decibelsToGain(get("vol"))); smMix.setTargetValue(get("mix") / 100.f);
    for (int i = 0; i < n; ++i) { tubeA[i] = smTube.getNextValue(); mechA[i] = smMech.getNextValue(); }
    for (int c = 0; c < nch; ++c)
        shelf[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(fs, 90.0, 0.7071f, phat ? 2.0f : 1.f);
    // dry copy for the mix, delayed to match the distortion oversampler latency
    for (int c = 0; c < nch; ++c)
    {
        const float* in = block.getChannelPointer((size_t) c);
        for (int i = 0; i < n; ++i)
        {
            dryRing[c][(ringPos + i) & 1023] = in[i];
            dryBuf[c][i] = dryRing[c][(ringPos + i - latency) & 1023];
        }
    }
    ringPos = (ringPos + n) & 1023;

    // 1. distortion (4x oversampled, smoothed zero-crossing); always run the oversampler so the latency stays constant
    auto up = os.processSamplesUp(block);
    if (dOn)
    {
        const int ov = (int) (up.getNumSamples() / (size_t) n);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer(c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) { const int k = (int) i / ov; d[i] = distort(d[i], tubeA[k], mechA[k], dtype, true); }
        }
    }
    os.processSamplesDown(block);
    if (dOn)
        for (int c = 0; c < nch; ++c)
        {
            auto* d = block.getChannelPointer((size_t) c);
            for (int i = 0; i < n; ++i) { const float x = d[i]; dc[c] += 0.0005f * (x - dc[c]); d[i] = x - dc[c]; }
        }
    // 2. filter: 24 dB low-pass, high-pass or band-pass, resonance on the first stage; coefficients follow the smoothed knobs every 16 samples
    if (fOn)
    {
        const auto t = ftype == 1 ? juce::dsp::StateVariableTPTFilterType::highpass : ftype == 2 ? juce::dsp::StateVariableTPTFilterType::bandpass : juce::dsp::StateVariableTPTFilterType::lowpass;
        for (int c = 0; c < nch; ++c) { filtA[c].setType(t); filtB[c].setType(t); }
        for (int s0 = 0; s0 < n; s0 += 16)
        {
            const int len = std::min(16, n - s0);
            const float cut = smCut.skip(len) , res = smRes.skip(len);   // value after len samples: one step ahead, still inside the 15 ms ramp
            const float fc = juce::jlimit(20.f, (float) fs * 0.45f, cut);
            const float q = ftype == 2 ? 0.7f + res * 0.03f : 0.55f + res * 0.01f * 7.f;
            for (int c = 0; c < nch; ++c)
            {
                filtA[c].setCutoffFrequency(fc); filtA[c].setResonance(q);
                filtB[c].setCutoffFrequency(fc); filtB[c].setResonance(ftype == 2 ? q : 0.707f);
                auto* d = block.getChannelPointer((size_t) c) + s0;
                if (ftype == 2) { const float nm = 1.f / (q * q); for (int i = 0; i < len; ++i) d[i] = filtB[c].processSample(0, filtA[c].processSample(0, d[i])) * nm; }
                else for (int i = 0; i < len; ++i) d[i] = filtB[c].processSample(0, filtA[c].processSample(0, d[i]));
            }
        }
    }
    else { smCut.skip(n); smRes.skip(n); }
    // 3. compressor (stereo linked); Phat adds low shelf weight, slower release and a soft ceiling.
    //    Make-up gain is automatic: a slow RMS match (limited by a peak guard) keeps the level the same at any amount.
    if (cOn)
    {
        const float aE = std::exp(-1.f / (0.4f * (float) fs)), aK = std::exp(-1.f / (0.25f * (float) fs));
        const float gA = std::exp(-1.f / (0.05f * (float) fs)), gR = std::exp(-1.f / (0.3f * (float) fs));
        const float aA = std::exp(-1.f / (0.003f * (float) fs)), aR = std::exp(-1.f / ((phat ? 0.22f : 0.12f) * (float) fs));
        float grMax = 0.f;
        for (int i = 0; i < n; ++i)
        {
            const float amount = smAmt.getNextValue(), thr = -amount * 30.f, ratio = 1.f + amount * 7.f;
            float pk = 0.f, inPw = 0.f, inPk = 0.f;
            for (int c = 0; c < nch; ++c)
            {
                auto* d = block.getChannelPointer((size_t) c); const float raw = d[i];
                inPw += raw * raw; inPk = std::max(inPk, std::abs(raw));
                if (phat) d[i] = shelf[c].processSample(d[i]);
                pk = std::max(pk, std::abs(d[i]));
            }
            inPw /= (float) nch;
            const float a = pk > env ? aA : aR; env = a * env + (1.f - a) * pk;
            const float db = juce::Decibels::gainToDecibels(env, -100.f);
            const float grDb = db > thr ? (db - thr) * (1.f - 1.f / ratio) : 0.f;
            grMax = std::max(grMax, grDb);
            const float g = juce::Decibels::decibelsToGain(-grDb);
            float outPw = 0.f, outPk = 0.f;
            for (int c = 0; c < nch; ++c) { auto* d = block.getChannelPointer((size_t) c); d[i] *= g; outPw += d[i] * d[i]; outPk = std::max(outPk, std::abs(d[i])); }
            outPw /= (float) nch;
            pIn = aE * pIn + (1.f - aE) * inPw; pOut = aE * pOut + (1.f - aE) * outPw;
            kIn = std::max(inPk, kIn * aK); kOut = std::max(outPk, kOut * aK);
            const float tg = (pOut > 1e-10f && pIn > 1e-10f) ? juce::jlimit(1.f, 12.f, std::min(std::sqrt(pIn / pOut), kOut > 1e-6f ? 4.f * kIn / kOut : 1.f)) : 1.f;
            const float kk = tg > mkGain ? gA : gR; mkGain = kk * mkGain + (1.f - kk) * tg;
            for (int c = 0; c < nch; ++c)
            {
                float& s = block.getChannelPointer((size_t) c)[i];
                s *= mkGain;
                if (phat) s = std::tanh(s * 0.9f) / 0.9f * 0.95f;
            }
        }
        grShown = grMax;
    }
    else { smAmt.skip(n); grShown = 0.f; }
    // 4. master: mix and volume
    for (int i = 0; i < n; ++i)
    {
        const float mix = smMix.getNextValue(), outG = smVol.getNextValue();
        for (int c = 0; c < nch; ++c) { auto* d = block.getChannelPointer((size_t) c); d[i] = (d[i] * mix + dryBuf[c][i] * (1.f - mix)) * outG; }
    }
    // 5. hidden soft clipper (2x oversampled, no knob): shaves only the tallest peaks before the safety limiter
    {
        auto upc = osClip.processSamplesUp(block);
        const float k = 0.7f, cl = 0.97f;
        for (size_t c = 0; c < upc.getNumChannels(); ++c)
        {
            auto* d = upc.getChannelPointer(c);
            for (size_t i = 0; i < upc.getNumSamples(); ++i) { const float m = std::abs(d[i]); if (m > k) d[i] = std::copysign(k + (cl - k) * std::tanh((m - k) / (cl - k)), d[i]); }
        }
        osClip.processSamplesDown(block);
    }
}

void CatCrusher::paintPanel(int idx, juce::Graphics& g, juce::Rectangle<float> r)
{
    using namespace catfx::viz;
    g.setFont(juce::FontOptions(11.f));
    if (idx == 0)       // distortion curve
    {
        const int dtype = (int) get("type");
        const float tube = get("tube"), mech = get("mech"); const bool on = get("don") > 0.5f;
        auto p = r.withTrimmedLeft(8.f).reduced(10.f, 14.f); p.removeFromBottom(10.f);
        g.setColour(grid()); g.drawRect(p); g.drawLine(p.getX(), p.getCentreY(), p.getRight(), p.getCentreY()); g.drawLine(p.getCentreX(), p.getY(), p.getCentreX(), p.getBottom());
        juce::Path path;
        for (int i = 0; i <= 200; ++i)
        {
            const float x = -1.f + (float) i / 100.f, y = juce::jlimit(-1.2f, 1.2f, on ? distort(x, tube, mech, dtype, true) : x);
            const float px = p.getX() + (x + 1.f) * 0.5f * p.getWidth(), py = p.getCentreY() - y / 1.2f * 0.5f * p.getHeight();
            i ? path.lineTo(px, py) : path.startNewSubPath(px, py);
        }
        g.setColour(accent()); g.strokePath(path, juce::PathStrokeType(2.f));
        const float lv = juce::jlimit(0.f, 1.f, lastIn.load()), yy = juce::jlimit(-1.2f, 1.2f, on ? distort(lv, tube, mech, dtype, true) : lv);
        g.setColour(juce::Colours::white); g.fillEllipse(p.getX() + (lv + 1.f) * 0.5f * p.getWidth() - 3.f, p.getCentreY() - yy / 1.2f * 0.5f * p.getHeight() - 3.f, 6.f, 6.f);
        g.setColour(blue()); g.drawText("input level", p.toNearestInt().withY((int) p.getBottom() + 1).withHeight(11), juce::Justification::centred);
        g.drawText("-1", (int) p.getX(), (int) p.getBottom() + 1, 20, 11, juce::Justification::left); g.drawText("+1", (int) p.getRight() - 20, (int) p.getBottom() + 1, 20, 11, juce::Justification::right);
        g.drawText("output", (int) r.getX() + 2, (int) r.getY() + 1, 60, 11, juce::Justification::left);
        g.setColour(accent()); g.drawText("gain x" + juce::String(1.f + tube * 0.24f, 1), (int) r.getRight() - 80, (int) r.getY() + 1, 76, 11, juce::Justification::right);
    }
    else if (idx == 1)  // filter response
    {
        const float fc = get("cut"), q = 0.55f + get("res") * 0.01f * 7.f; const bool on = get("fon") > 0.5f; const int ft = (int) get("ftype");
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(8.f).withTrimmedTop(16.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        dbAxis(g, p, -36.f, 18.f, 12.f); freqAxis(g, p);
        juce::Path f;
        for (int i = 0; i <= 200; ++i)
        {
            const float fr = 20.f * std::pow(1000.f, (float) i / 200.f), rr = fr / fc;
            auto lp = [&](float qq) { return 1.f / std::sqrt((1.f - rr * rr) * (1.f - rr * rr) + (rr / qq) * (rr / qq)); };
            auto hp = [&](float qq) { return rr * rr * lp(qq); };
            auto bp = [&](float qq) { return (rr / qq) * lp(qq); };
            const float qbp = 0.7f + get("res") * 0.03f;
            const float mag = !on ? 1.f : ft == 1 ? hp(q) * hp(0.707f) : ft == 2 ? bp(qbp) * bp(qbp) : lp(q) * lp(0.707f);
            const float db = juce::jlimit(-36.f, 18.f, 20.f * std::log10(std::max(mag, 1e-4f)));
            const float px = p.getX() + (float) i / 200.f * p.getWidth(), py = p.getBottom() - (db + 36.f) / 54.f * p.getHeight();
            i ? f.lineTo(px, py) : f.startNewSubPath(px, py);
        }
        g.setColour(green()); g.strokePath(f, juce::PathStrokeType(2.f));
        g.setColour(green()); g.drawText("cutoff " + juce::String((int) fc) + " Hz", (int) r.getRight() - 120, (int) r.getY() + 1, 116, 11, juce::Justification::right);
    }
    else if (idx == 2)  // compressor transfer curve and GR meter
    {
        const float am = get("comp") / 100.f, thr = -am * 30.f, ratio = 1.f + am * 7.f, mk = 0.f;   // make-up is automatic, so the curve shows the compression only
        const bool on = get("con") > 0.5f;
        auto grR = r.removeFromRight(46.f).reduced(2.f, 6.f);
        auto p = r.withTrimmedLeft(34.f).withTrimmedRight(6.f).withTrimmedTop(34.f).withTrimmedBottom(14.f);
        g.setColour(bg().brighter(0.04f)); g.fillRect(p);
        dbAxis(g, p, -48.f, 12.f, 12.f, false);
        for (float v : { -48.f, -36.f, -24.f, -12.f, 0.f })
        { const float x = p.getX() + (v + 48.f) / 48.f * p.getWidth(); g.setColour(grid()); g.drawVerticalLine((int) x, p.getY(), p.getBottom()); g.setColour(green()); g.drawText(juce::String((int) v), (int) x - 14, (int) p.getBottom() + 1, 28, 11, juce::Justification::centred); }
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
        g.setColour(red()); g.drawText(on ? "thr " + juce::String(thr, 0) + " dB   ratio " + juce::String(ratio, 1) + ":1" : "compressor off", p.toNearestInt().withY((int) r.getY() + 1).withHeight(11), juce::Justification::centred);
        g.setColour(accent()); if (on) g.drawText("auto make-up keeps the level", p.toNearestInt().withY((int) r.getY() + 13).withHeight(11), juce::Justification::centred);
        g.setColour(blue()); g.drawText("in dB", (int) p.getRight() - 40, (int) p.getBottom() - 14, 40, 11, juce::Justification::right);
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
        g.setFont(juce::FontOptions(10.f));
        g.setColour(blue()); g.drawText("dry " + juce::String(100 - (int) mix) + "%", bottom.toNearestInt().withHeight(12), juce::Justification::left);
        g.setColour(green()); g.drawText("wet " + juce::String((int) mix) + "%", bottom.toNearestInt().withHeight(12), juce::Justification::right);
        g.setColour(accent()); g.drawText("volume " + juce::String(vol > 0.f ? "+" : "") + juce::String(vol, 1) + " dB", bottom.toNearestInt().withTrimmedTop(24), juce::Justification::centred);
    }
}
