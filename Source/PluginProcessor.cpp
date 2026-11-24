#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
static juce::NormalisableRange<float> skewRange (float lo, float hi, float centre, float step = 0.0f)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

static juce::String hzText (float v, int)
{
    if (v >= 1000.0f) return juce::String (v / 1000.0f, 2) + " kHz";
    return juce::String (juce::roundToInt (v)) + " Hz";
}

APVTS::ParameterLayout UtilDeckProcessor::createLayout()
{
    using namespace juce;
    APVTS::ParameterLayout l;
    auto b = [&] (const char* id, const char* name, bool def)
    { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto f = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit,
                  std::function<String (float, int)> toText = nullptr)
    {
        auto attr = AudioParameterFloatAttributes().withLabel (unit);
        if (toText) attr = attr.withStringFromValueFunction (toText);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attr));
    };

    b ("bypass", "Bypass", false);

    b ("invL", "Polarity L", false);
    b ("invR", "Polarity R", false);
    b ("invLink", "Polarity Link", false);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chMode", 1 }, "Channels",
                                                    StringArray { "Stereo", "Mono", "Left", "Right", "Swap" }, 0));

    b ("phaseOn", "Phase On", false);
    f ("phaseFreq", "Phase Freq", skewRange (20.0f, 20000.0f, 632.0f, 1.0f), 632.0f, "Hz", hzText);

    b ("hpOn", "Highpass On", false);
    f ("hpFreq", "Highpass Freq", skewRange (10.0f, 2000.0f, 140.0f, 1.0f), 30.0f, "Hz", hzText);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hpSlope", 1 }, "Highpass Slope",
                                                    StringArray { "6", "12", "18", "24", "36", "48" }, 1));

    b ("delayOn", "Delay On", false);
    f ("delayL", "Delay L", skewRange (0.0f, 500.0f, 20.0f, 0.01f), 0.0f, "ms");
    f ("delayR", "Delay R", skewRange (0.0f, 500.0f, 20.0f, 0.01f), 0.0f, "ms");
    b ("delayLink", "Delay Link", false);
    b ("delaySmp", "Delay in Samples", false);

    b ("widthOn", "Width On", true);
    f ("width", "Width", NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f, "%");
    b ("mono", "Mono", false);

    b ("bassOn", "Bassmono On", false);
    f ("bassFreq", "Bassmono Freq", skewRange (20.0f, 500.0f, 120.0f, 1.0f), 120.0f, "Hz", hzText);
    b ("bassSolo", "Bassmono Listen", false);

    f ("panL", "Pan L", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), -100.0f, "%");
    f ("panR", "Pan R", NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 100.0f, "%");
    b ("panMirror", "Pan Mirror (x2)", false);

    b ("muteL", "Mute L", false);
    b ("muteR", "Mute R", false);
    f ("gain", "Gain", skewRange (-60.0f, 12.0f, -12.0f, 0.1f), 0.0f, "dB");
    f ("mix", "Trim (Mix)", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, "%");

    return l;
}

//==============================================================================
UtilDeckProcessor::UtilDeckProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "UtilDeck", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pInvL = p ("invL"); pInvR = p ("invR"); pChMode = p ("chMode");
    pPhaseOn = p ("phaseOn"); pPhaseFreq = p ("phaseFreq");
    pHpOn = p ("hpOn"); pHpFreq = p ("hpFreq"); pHpSlope = p ("hpSlope");
    pDelayOn = p ("delayOn"); pDelayL = p ("delayL"); pDelayR = p ("delayR");
    pWidthOn = p ("widthOn"); pWidth = p ("width"); pMono = p ("mono");
    pBassOn = p ("bassOn"); pBassFreq = p ("bassFreq"); pBassSolo = p ("bassSolo");
    pPanL = p ("panL"); pPanR = p ("panR");
    pMuteL = p ("muteL"); pMuteR = p ("muteR"); pGain = p ("gain"); pMix = p ("mix");
}

bool UtilDeckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

void UtilDeckProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    const int needed = (int) std::ceil (0.5 * sr) + 8;
    int size = 1;
    while (size < needed) size <<= 1;
    for (auto& d : delBuf) d.assign ((size_t) size, 0.0f);
    delMask = size - 1;
    delWrite = 0;

    for (auto* s : { &smGain, &smWidth, &smMix, &smLL, &smLR, &smRL, &smRR })
        s->reset (sr, 0.02);
    delSmL.reset (sr, 0.05); delSmR.reset (sr, 0.05);
    delSmL.setCurrentAndTargetValue (pDelayL->load() * 0.001f * (float) sr);
    delSmR.setCurrentAndTargetValue (pDelayR->load() * 0.001f * (float) sr);
    smGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load(), -60.0f));
    smWidth.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    smMix.setCurrentAndTargetValue (pMix->load() * 0.01f);
    smLL.setCurrentAndTargetValue (1.0f); smLR.setCurrentAndTargetValue (0.0f);
    smRL.setCurrentAndTargetValue (0.0f); smRR.setCurrentAndTargetValue (1.0f);

    lastHpF = lastApF = lastBassF = -1; lastHpSlope = -1;
    for (int c = 0; c < 2; ++c)
    {
        for (auto& q : hp[c]) q.reset();
        hp1[c].reset(); ap[c].reset(); sideHp[c].reset(); midLp[c].reset();
    }
    updateFilters();
}

void UtilDeckProcessor::updateFilters()
{
    const double nyq = sr * 0.49;

    const double hf = juce::jmin ((double) pHpFreq->load(), nyq);
    const int slope = (int) pHpSlope->load();
    if (hf != lastHpF || slope != lastHpSlope)
    {
        lastHpF = hf; lastHpSlope = slope;
        // Butterworth Q tables per order
        static const double q2[] = { 0.70710678 };
        static const double q3[] = { 1.0 };
        static const double q4[] = { 0.54119610, 1.30656296 };
        static const double q6[] = { 0.51763809, 0.70710678, 1.93185165 };
        static const double q8[] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        const double* qs = nullptr;
        switch (slope)
        {
            case 0: hpFirst = true;  hpStages = 0; break;            // 6
            case 1: hpFirst = false; hpStages = 1; qs = q2; break;   // 12
            case 2: hpFirst = true;  hpStages = 1; qs = q3; break;   // 18
            case 3: hpFirst = false; hpStages = 2; qs = q4; break;   // 24
            case 4: hpFirst = false; hpStages = 3; qs = q6; break;   // 36
            default: hpFirst = false; hpStages = 4; qs = q8; break;  // 48
        }
        hp1[0].setHighpass1 (hf, sr); hp1[1].copyCoeffs (hp1[0]);
        for (int s = 0; s < hpStages; ++s)
        {
            hp[0][s].setHighpass (hf, qs[s], sr);
            hp[1][s].copyCoeffs (hp[0][s]);
        }
    }

    const double af = juce::jmin ((double) pPhaseFreq->load(), nyq);
    if (af != lastApF)
    {
        lastApF = af;
        ap[0].setAllpass (af, 0.70710678, sr); ap[1].copyCoeffs (ap[0]);
    }

    const double bf = juce::jmin ((double) pBassFreq->load(), nyq);
    if (bf != lastBassF)
    {
        lastBassF = bf;
        // Linkwitz-Riley 4th order = two identical 2nd-order Butterworth stages
        sideHp[0].setHighpass (bf, 0.70710678, sr); sideHp[1].copyCoeffs (sideHp[0]);
        midLp[0].setLowpass (bf, 0.70710678, sr);   midLp[1].copyCoeffs (midLp[0]);
    }
}

//==============================================================================
void UtilDeckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0 || buffer.getNumChannels() < 2) return;
    if (bypassParam->get()) return;

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    updateFilters();

    // ---- read parameters once per block --------------------------------
    const bool invL = *pInvL > 0.5f, invR = *pInvR > 0.5f;
    const int  chMode = (int) pChMode->load();
    const bool hpOn = *pHpOn > 0.5f, phaseOn = *pPhaseOn > 0.5f, delayOn = *pDelayOn > 0.5f;
    const bool widthOn = *pWidthOn > 0.5f, mono = *pMono > 0.5f;
    const bool bassOn = *pBassOn > 0.5f, bassSolo = bassOn && *pBassSolo > 0.5f;
    const bool muteL = *pMuteL > 0.5f, muteR = *pMuteR > 0.5f;

    // reset filter memory when a section is switched on, so stale state never pops
    if (hpOn && ! wasHpOn)       { for (int c = 0; c < 2; ++c) { for (auto& q : hp[c]) q.reset(); hp1[c].reset(); } }
    if (phaseOn && ! wasPhaseOn) { ap[0].reset(); ap[1].reset(); }
    if (bassOn && ! wasBassOn)   { for (int c = 0; c < 2; ++c) { sideHp[c].reset(); midLp[c].reset(); } }
    if (delayOn && ! wasDelayOn)
    {
        for (auto& d : delBuf) std::fill (d.begin(), d.end(), 0.0f);
        delSmL.setCurrentAndTargetValue (pDelayL->load() * 0.001f * (float) sr);
        delSmR.setCurrentAndTargetValue (pDelayR->load() * 0.001f * (float) sr);
    }
    wasHpOn = hpOn; wasPhaseOn = phaseOn; wasDelayOn = delayOn; wasBassOn = bassOn;

    smGain.setTargetValue (juce::Decibels::decibelsToGain (pGain->load(), -60.0f));
    smWidth.setTargetValue (widthOn ? pWidth->load() * 0.01f : 1.0f);
    smMix.setTargetValue (pMix->load() * 0.01f);
    delSmL.setTargetValue (pDelayL->load() * 0.001f * (float) sr);
    delSmR.setTargetValue (pDelayR->load() * 0.001f * (float) sr);

    const float panL = pPanL->load() * 0.01f, panR = pPanR->load() * 0.01f;
    {
        const float tl = (panL + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float tr = (panR + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        smLL.setTargetValue (std::cos (tl)); smLR.setTargetValue (std::sin (tl));
        smRL.setTargetValue (std::cos (tr)); smRR.setTargetValue (std::sin (tr));
    }
    const bool panActive = panL > -0.999f || panR < 0.999f || smLL.isSmoothing() || smRR.isSmoothing();
    const bool msActive  = mono || bassOn || smWidth.isSmoothing() || std::abs (smWidth.getTargetValue() - 1.0f) > 1.0e-6f;
    const bool mixActive = smMix.isSmoothing() || smMix.getTargetValue() < 0.9999f;

    const float bL = muteL ? 0.0f : (invL ? -1.0f : 1.0f);
    const float bR = muteR ? 0.0f : (invR ? -1.0f : 1.0f);
    const float delBufMax = (float) (delMask - 4);

    const bool meter = editorOpen.load (std::memory_order_relaxed);
    float pkL = 0, pkR = 0;
    double sLR = 0, sLL = 0, sRR = 0;

    for (int i = 0; i < n; ++i)
    {
        const float dryL = L[i], dryR = R[i];
        float l = dryL * bL, r = dryR * bR;

        switch (chMode)
        {
            case 1: { const float m = 0.5f * (l + r); l = r = m; } break;
            case 2: r = l; break;
            case 3: l = r; break;
            case 4: std::swap (l, r); break;
            default: break;
        }

        if (hpOn)
        {
            if (hpFirst) { l = hp1[0].process (l); r = hp1[1].process (r); }
            for (int s = 0; s < hpStages; ++s) { l = hp[0][s].process (l); r = hp[1][s].process (r); }
        }

        if (phaseOn) { l = ap[0].process (l); r = ap[1].process (r); }

        if (delayOn)
        {
            delBuf[0][(size_t) delWrite] = l;
            delBuf[1][(size_t) delWrite] = r;
            auto readAt = [&] (const std::vector<float>& b, float d) noexcept
            {
                d = juce::jlimit (0.0f, delBufMax, d);
                const int   di = (int) d;
                const float fr = d - (float) di;
                const float a  = b[(size_t) ((delWrite - di) & delMask)];
                const float c  = b[(size_t) ((delWrite - di - 1) & delMask)];
                return a + fr * (c - a);
            };
            l = readAt (delBuf[0], delSmL.getNextValue());
            r = readAt (delBuf[1], delSmR.getNextValue());
            delWrite = (delWrite + 1) & delMask;
        }

        if (msActive)
        {
            float m = 0.5f * (l + r), s = 0.5f * (l - r);
            s *= smWidth.getNextValue();
            if (bassOn)
            {
                if (bassSolo)
                {
                    m = midLp[1].process (midLp[0].process (m));
                    s = 0.0f;
                }
                else
                {
                    s = sideHp[1].process (sideHp[0].process (s));
                }
            }
            if (mono) s = 0.0f;
            l = m + s; r = m - s;
        }

        if (panActive)
        {
            const float ll = smLL.getNextValue(), lr = smLR.getNextValue();
            const float rl = smRL.getNextValue(), rr = smRR.getNextValue();
            const float nl = l * ll + r * rl;
            const float nr = l * lr + r * rr;
            l = nl; r = nr;
        }

        const float g = smGain.getNextValue();
        l *= g; r *= g;

        if (mixActive)
        {
            const float w = smMix.getNextValue();
            l = dryL + w * (l - dryL);
            r = dryR + w * (r - dryR);
        }

        L[i] = l; R[i] = r;

        if (meter)
        {
            pkL = juce::jmax (pkL, std::abs (l));
            pkR = juce::jmax (pkR, std::abs (r));
            sLR += (double) l * r; sLL += (double) l * l; sRR += (double) r * r;
        }
    }

    // keep smoothers in step when their sections were skipped
    if (! msActive)  smWidth.skip (n);
    if (! panActive) { smLL.skip (n); smLR.skip (n); smRL.skip (n); smRR.skip (n); }
    if (! mixActive) smMix.skip (n);
    if (! delayOn)   { delSmL.skip (n); delSmR.skip (n); }

    if (meter)
    {
        pushPeak (peakL, pkL);
        pushPeak (peakR, pkR);
        if (sLL > 1.0e-9 && sRR > 1.0e-9)
        {
            const float c = (float) (sLR / std::sqrt (sLL * sRR));
            const float coeff = 1.0f - std::exp (-(float) n / (float) (0.15 * sr));
            corrState += coeff * (c - corrState);
        }
        else if (sLL > 1.0e-9 || sRR > 1.0e-9)
        {
            // one side silent: no correlation information, drift to 0
            const float coeff = 1.0f - std::exp (-(float) n / (float) (0.15 * sr));
            corrState += coeff * (0.0f - corrState);
        }
        correlation.store (corrState);
    }
}

//==============================================================================
void UtilDeckProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiW", uiW, nullptr);
    state.setProperty ("uiH", uiH, nullptr);
    state.setProperty ("dark", darkTheme, nullptr);
    state.setProperty ("preset", presetIndex, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void UtilDeckProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType (apvts.state.getType()))
        {
            uiW = (int) state.getProperty ("uiW", uiW);
            uiH = (int) state.getProperty ("uiH", uiH);
            darkTheme = (bool) state.getProperty ("dark", darkTheme);
            presetIndex = (int) state.getProperty ("preset", presetIndex);
            apvts.replaceState (state);

            if (auto* ed = dynamic_cast<UtilDeckEditor*> (getActiveEditor()))
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<UtilDeckEditor> (ed)]
                                                 { if (safe) safe->restoreFromProcessor(); });
        }
    }
}

juce::AudioProcessorEditor* UtilDeckProcessor::createEditor() { return new UtilDeckEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UtilDeckProcessor(); }
