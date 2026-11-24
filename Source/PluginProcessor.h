#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>

//==============================================================================
// Tiny, cheap filter building blocks (no allocation, no virtual calls).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    inline float process (float x) noexcept
    {
        const double in = x;
        const double y = b0 * in + z1;
        z1 = b1 * in - a1 * y + z2;
        z2 = b2 * in - a2 * y;
        return (float) y;
    }
    void reset() noexcept { z1 = z2 = 0; }
    void copyCoeffs (const Biquad& o) noexcept { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }

    void setHighpass (double f, double q, double fs) noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (1.0 + cs) * 0.5 / a0; b1 = -(1.0 + cs) / a0; b2 = b0;
        a1 = -2.0 * cs / a0; a2 = (1.0 - al) / a0;
    }
    void setLowpass (double f, double q, double fs) noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (1.0 - cs) * 0.5 / a0; b1 = (1.0 - cs) / a0; b2 = b0;
        a1 = -2.0 * cs / a0; a2 = (1.0 - al) / a0;
    }
    void setAllpass (double f, double q, double fs) noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (1.0 - al) / a0; b1 = -2.0 * cs / a0; b2 = 1.0;   // (1+al)/a0 == 1
        a1 = b1; a2 = b0;
    }
    void setHighpass1 (double f, double fs) noexcept   // first-order (6 dB/oct)
    {
        const double k = std::tan (juce::MathConstants<double>::pi * f / fs);
        b0 = 1.0 / (1.0 + k); b1 = -b0; b2 = 0;
        a1 = (k - 1.0) / (k + 1.0); a2 = 0;
    }
};

//==============================================================================
class UtilDeckProcessor : public juce::AudioProcessor
{
public:
    UtilDeckProcessor();
    ~UtilDeckProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UtilDeck"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- GUI <-> audio communication -------------------------------------
    std::atomic<bool>  editorOpen { false };
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
    std::atomic<float> correlation { 1.0f };

    // ---- remembered GUI state (message thread only) ----------------------
    static constexpr int baseW = 920, baseH = 652;
    int uiW = 782, uiH = 554;      // 85 % of the base size
    bool darkTheme = false;
    int presetIndex = 0;

private:
    void updateFilters();
    static void pushPeak (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load();
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }

    juce::AudioParameterBool* bypassParam = nullptr;

    // raw parameter pointers (fast access on audio thread)
    std::atomic<float> *pInvL, *pInvR, *pChMode, *pPhaseOn, *pPhaseFreq, *pHpOn, *pHpFreq, *pHpSlope,
                       *pDelayOn, *pDelayL, *pDelayR, *pWidthOn, *pWidth, *pMono, *pBassOn, *pBassFreq,
                       *pBassSolo, *pPanL, *pPanR, *pMuteL, *pMuteR, *pGain, *pMix;

    double sr = 44100.0;

    // filters, [channel][stage]
    Biquad hp[2][4];
    Biquad hp1[2];                 // first-order stage for 6 / 18 dB slopes
    int    hpStages = 0;
    bool   hpFirst = false;
    double lastHpF = -1; int lastHpSlope = -1;

    Biquad ap[2];
    double lastApF = -1;

    Biquad sideHp[2], midLp[2];    // bass mono: LR4 on side / mid (solo)
    double lastBassF = -1;

    bool wasHpOn = false, wasPhaseOn = false, wasDelayOn = false, wasBassOn = false;

    // delay
    std::vector<float> delBuf[2];
    int delMask = 0, delWrite = 0;
    juce::SmoothedValue<float> delSmL, delSmR;

    // smoothed gains
    juce::SmoothedValue<float> smGain, smWidth, smMix, smLL, smLR, smRL, smRR;

    float corrState = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UtilDeckProcessor)
};
