#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
struct Palette
{
    juce::Colour bg, panel, panelMid, notch, header, footer,
                 knobHi, knobLo, cap, track, text, textDim, orange,
                 dark, darkHi, darkText, meterBg, meterOff, shadow;
};
const Palette& pal();
void setDarkPalette (bool dark);

//==============================================================================
// Slider that knows how to describe its own value for the custom drawing.
class FSlider : public juce::Slider
{
public:
    FSlider() { setTextBoxStyle (NoTextBox, false, 0, 0); setPopupMenuEnabled (false); }

    std::function<juce::String (double)> numberText;    // big text
    juce::String unit;                                   // small text under it
    juce::String fixedText;                              // e.g. "L" / "R" on the delay knobs
    bool ticks = false, bipolar = false, active = true;

    juce::String getNumber() const
    {
        if (numberText) return numberText (getValue());
        return juce::String (juce::roundToInt (getValue()));
    }
};

//==============================================================================
class UDLookAndFeel : public juce::LookAndFeel_V4
{
public:
    UDLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
};

//==============================================================================
// Small custom widgets
class PillButton : public juce::Button
{
public:
    PillButton (const juce::String& t, bool darkStyle = false) : Button (t), text (t), dark (darkStyle)
    { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::String text; bool dark;
};

class PowerButton : public juce::Button
{
public:
    PowerButton() : Button ("power") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class IconButton : public juce::Button
{
public:
    enum Kind { link, linkRound, headphones, gear, power, msText, x2 };
    IconButton (Kind k) : Button ("icon"), kind (k) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    Kind kind; bool lit = false; juce::String label;
};

class MeterBar : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float levelDb = -100.0f;
};

class PeakReadout : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { holdDb = -100.0f; repaint(); }
    float holdDb = -100.0f;
};

class CorrMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    float value = 1.0f;
};

class PresetBar : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (int)> onStep;     // -1 / +1
    std::function<void()> onMenu;
    juce::String name;
};

class ThemeToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    std::function<void()> onClick;
    bool dark = false;
};

//==============================================================================
class UtilDeckEditor;

class Content : public juce::Component
{
public:
    Content (UtilDeckProcessor&, UtilDeckEditor&);
    void paint (juce::Graphics&) override;
    void syncFromParams();
    void tickMeters();

    UtilDeckProcessor& proc;
    UtilDeckEditor& editor;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    PresetBar presetBar;
    ThemeToggle themeToggle;
    IconButton gearBtn { IconButton::gear }, powerBtn { IconButton::power };

    PillButton invL { juce::String::fromUTF8 ("\xc3\xb8L") }, invR { juce::String::fromUTF8 ("\xc3\xb8R") };
    IconButton invLink { IconButton::linkRound };
    juce::ComboBox chMode;
    FSlider phaseKnob, hpKnob;
    PowerButton phaseOn, hpOn;
    juce::ComboBox hpSlope;

    FSlider delayL, delayR;
    IconButton delayLink { IconButton::link }, delayUnit { IconButton::msText };
    PowerButton delayOn;
    FSlider widthKnob;
    PowerButton widthOn;
    PillButton monoBtn { "Mono" };
    FSlider bassBar;
    PowerButton bassOn;
    IconButton bassSolo { IconButton::headphones };

    FSlider panL, panR;
    IconButton panMirror { IconButton::x2 };
    PillButton muteL { "", true }, muteR { "", true };
    PeakReadout readL, readR;
    MeterBar meterL, meterR;
    FSlider fader;
    CorrMeter corr;
    FSlider trim;

    std::vector<std::unique_ptr<SA>> sAtt;
    std::vector<std::unique_ptr<BA>> bAtt;
    std::vector<std::unique_ptr<CA>> cAtt;

private:
    void setParam (const juce::String& id, float plain);
    float getParam (const juce::String& id) const;
    void applyPreset (int index);
    void showPresetMenu();
    void showGearMenu();
    void setupKnob (FSlider&, const juce::String& id, const juce::String& tip);

    bool linking = false;
    float lastDelayL = -1, lastDelayR = -1; bool lastSmp = false;
};

//==============================================================================
class UtilDeckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit UtilDeckEditor (UtilDeckProcessor&);
    ~UtilDeckEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void restoreFromProcessor();
    void setScale (float s);

private:
    void timerCallback() override;

    UtilDeckProcessor& proc;
    UDLookAndFeel lnf;
    Content content;
    juce::TooltipWindow tooltips { this, 600 };
    bool ready = false;
};
