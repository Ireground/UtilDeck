#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Palettes: light = the original plugin's white/orange look, dark = night variant
static Palette lightPal {
    Colour (0xffeeeeee), Colour (0xffe4e4e4), Colour (0xffeeeeee), Colour (0xffd6d6d6), Colour (0xfff3f3f3), Colour (0xfff7f7f7),
    Colour (0xffffffff), Colour (0xffd9d9d9), Colour (0xfff1f1f1), Colour (0xffd2d2d2), Colour (0xff2a2a2a), Colour (0xff8a8a8a),
    Colour (0xfff26522),
    Colour (0xff3b3b3b), Colour (0xff4a4a4a), Colour (0xffffffff), Colour (0xff2b2b2b), Colour (0xff3d3d3d), Colour (0x40000000)
};
static Palette darkPal {
    Colour (0xff1e2024), Colour (0xff25282d), Colour (0xff2b2e33), Colour (0xff1a1c20), Colour (0xff1a1c1f), Colour (0xff17191c),
    Colour (0xff50545b), Colour (0xff2c2f34), Colour (0xff3a3d43), Colour (0xff1a1c1f), Colour (0xffe6e6e6), Colour (0xff8c9096),
    Colour (0xfff26522),
    Colour (0xff131416), Colour (0xff1c1e21), Colour (0xffffffff), Colour (0xff121315), Colour (0xff27292d), Colour (0x70000000)
};
static const Palette* currentPal = &lightPal;
const Palette& pal() { return *currentPal; }
void setDarkPalette (bool dark) { currentPal = dark ? &darkPal : &lightPal; }

static Font font (float size, bool bold = true)
{
    return Font (FontOptions (size, bold ? Font::bold : Font::plain));
}

static Path trapezoid (Rectangle<float> r, float inset, float corner, bool widerTop = true)
{
    Path p;
    if (widerTop)
    {
        p.startNewSubPath (r.getX(), r.getY());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (r.getRight() - inset, r.getBottom());
        p.lineTo (r.getX() + inset, r.getBottom());
    }
    else
    {
        p.startNewSubPath (r.getX() + inset, r.getY());
        p.lineTo (r.getRight() - inset, r.getY());
        p.lineTo (r.getRight(), r.getBottom());
        p.lineTo (r.getX(), r.getBottom());
    }
    p.closeSubPath();
    return p.createPathWithRoundedCorners (corner);
}

static void softShadow (Graphics& g, const Path& p, int radius = 8, Point<int> offset = { 0, 3 })
{
    DropShadow (pal().shadow, radius, offset).drawForPath (g, p);
}

static void drawPowerIcon (Graphics& g, Rectangle<float> r, Colour c, float thick)
{
    auto cx = r.getCentreX(), cy = r.getCentreY(), rad = jmin (r.getWidth(), r.getHeight()) * 0.38f;
    Path arc;
    arc.addCentredArc (cx, cy, rad, rad, 0.0f, 0.75f, MathConstants<float>::twoPi - 0.75f, true);
    g.setColour (c);
    g.strokePath (arc, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    g.drawLine (cx, cy - rad * 1.15f, cx, cy - rad * 0.15f, thick);
}

static void drawLinkIcon (Graphics& g, Rectangle<float> r, Colour c)
{
    auto w = r.getWidth() * 0.55f, h = r.getHeight() * 0.36f;
    auto cx = r.getCentreX(), cy = r.getCentreY();
    g.setColour (c);
    for (int i = 0; i < 2; ++i)
    {
        Path p;
        p.addRoundedRectangle (-w * 0.5f, -h * 0.5f, w, h, h * 0.5f);
        const float dx = (i == 0 ? -1.0f : 1.0f) * w * 0.28f, dy = (i == 0 ? 1.0f : -1.0f) * h * 0.25f;
        p.applyTransform (AffineTransform::rotation (-0.5f).translated (cx + dx, cy + dy));
        g.strokePath (p, PathStrokeType (jmax (1.6f, r.getHeight() * 0.09f)));
    }
}

//==============================================================================
UDLookAndFeel::UDLookAndFeel()
{
    setColour (TooltipWindow::backgroundColourId, Colour (0xff3b3b3b));
    setColour (TooltipWindow::textColourId, Colours::white);
}

void UDLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                      float a0, float a1, Slider& s)
{
    auto* fs = dynamic_cast<FSlider*> (&s);
    const auto& P = pal();
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const bool ticks = fs && fs->ticks;
    const bool active = ! fs || fs->active;
    const float angle = a0 + pos * (a1 - a0);

    if (ticks)
    {
        g.setColour (P.textDim);
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 10.0f;
            const auto pt = c.getPointOnCircumference (R - 3.0f, a);
            g.fillEllipse (pt.x - 1.8f, pt.y - 1.8f, 3.6f, 3.6f);
        }
    }

    const float rb = ticks ? R * 0.82f : R * 0.94f;

    // body with shadow
    Path body; body.addEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    softShadow (g, body, (int) (rb * 0.25f), { 0, (int) (rb * 0.10f) });
    g.setGradientFill (ColourGradient (P.knobHi, c.x, c.y - rb, P.knobLo, c.x, c.y + rb, false));
    g.fillPath (body);

    // value arc
    const float ra = rb * 0.86f, thick = jmax (3.0f, rb * 0.11f);
    Path track; track.addCentredArc (c.x, c.y, ra, ra, 0.0f, a0, a1, true);
    g.setColour (P.track);
    g.strokePath (track, PathStrokeType (thick * 0.55f, PathStrokeType::curved, PathStrokeType::rounded));

    float from = a0;
    if (fs && fs->bipolar) from = (a0 + a1) * 0.5f;
    if (std::abs (angle - from) > 0.001f)
    {
        Path arc; arc.addCentredArc (c.x, c.y, ra, ra, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (active ? P.orange : P.textDim.withAlpha (0.6f));
        g.strokePath (arc, PathStrokeType (thick, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // cap
    const float rc = rb * 0.70f;
    Path cap; cap.addEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    DropShadow (P.shadow.withMultipliedAlpha (0.6f), (int) (rc * 0.12f) + 1, { 0, 1 }).drawForPath (g, cap);
    g.setGradientFill (ColourGradient (P.cap.brighter (0.05f), c.x, c.y - rc, P.cap.darker (0.06f), c.x, c.y + rc, false));
    g.fillPath (cap);

    // pointer dot
    const auto pd = c.getPointOnCircumference (rc * 0.82f, angle);
    const float dr = jmax (2.0f, rb * 0.055f);
    g.setColour (active ? P.orange : P.textDim);
    g.fillEllipse (pd.x - dr, pd.y - dr, dr * 2, dr * 2);

    // text
    g.setColour (P.text);
    if (fs && fs->fixedText.isNotEmpty())
    {
        g.setFont (font (rc * 0.62f, false));
        g.drawText (fs->fixedText, Rectangle<float> (c.x - rc, c.y - rc, rc * 2, rc * 2), Justification::centred);
    }
    else if (fs)
    {
        const bool hasUnit = fs->unit.isNotEmpty();
        auto num = fs->getNumber();
        auto unitTxt = fs->unit;
        if (unitTxt == "Hz" && s.getValue() >= 1000.0) unitTxt = "kHz";
        const float ns = num.length() > 4 ? rc * 0.40f : rc * 0.50f;
        g.setFont (font (ns, false));
        g.drawText (num, Rectangle<float> (c.x - rc, c.y - (hasUnit ? ns * 0.75f : ns * 0.5f), rc * 2, ns),
                    Justification::centred);
        if (hasUnit)
        {
            g.setColour (P.textDim);
            g.setFont (font (rc * 0.24f, false));
            g.drawText (unitTxt, Rectangle<float> (c.x - rc, c.y + ns * 0.28f, rc * 2, rc * 0.3f), Justification::centred);
        }
    }
}

int UDLookAndFeel::getSliderThumbRadius (Slider& s)
{
    if (s.getComponentID() == "fader") return 13;
    if (s.getComponentID() == "trim")  return 13;
    return LookAndFeel_V4::getSliderThumbRadius (s);
}

void UDLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos,
                                      float, float, Slider::SliderStyle style, Slider& s)
{
    const auto& P = pal();
    auto* fs = dynamic_cast<FSlider*> (&s);

    if (style == Slider::LinearBar)   // Bassmono frequency bar
    {
        auto r = s.getLocalBounds().toFloat().reduced (1.0f);
        Path shape = trapezoid (r, 6.0f, 7.0f, true);
        softShadow (g, shape, 6, { 0, 2 });
        g.setColour (P.dark); g.fillPath (shape);

        const bool active = ! fs || fs->active;
        g.saveState();
        g.reduceClipRegion (shape);
        g.setColour (active ? P.orange : P.textDim.withAlpha (0.5f));
        g.fillRect (Rectangle<float> (r.getX(), r.getY(), jmax (0.0f, pos - r.getX()), r.getHeight()));
        g.restoreState();

        g.setColour (P.darkText);
        g.setFont (font (r.getHeight() * 0.46f));
        String t = fs ? fs->getNumber() + " " + fs->unit : String (s.getValue());
        g.drawText (t, r, Justification::centred);
        return;
    }

    if (style == Slider::LinearVertical)   // gain fader
    {
        const float cx = (float) x + (float) w * 0.5f;
        g.setColour (P.meterBg);
        g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) h, 2.0f);
        g.setColour (P.orange);
        g.fillRect (Rectangle<float> (cx - 2.0f, pos, 4.0f, (float) (y + h) - pos));

        auto thumb = Rectangle<float> (cx - 28.0f, pos - 13.0f, 56.0f, 26.0f);
        Path tp; tp.addRoundedRectangle (thumb, 3.0f);
        softShadow (g, tp, 6, { 0, 2 });
        g.setGradientFill (ColourGradient (P.knobHi, 0, thumb.getY(), P.knobLo, 0, thumb.getBottom(), false));
        g.fillPath (tp);
        g.setColour (P.text);
        g.setFont (font (14.0f, false));
        auto v = s.getValue();
        g.drawText (v <= -59.95 ? String ("-inf") : String (v, 1), thumb, Justification::centred);
        return;
    }

    // horizontal "Trim" slider
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (P.meterBg);
    g.fillRoundedRectangle ((float) x, cy - 1.5f, (float) w, 3.0f, 1.5f);
    g.setColour (P.orange);
    g.fillRoundedRectangle ((float) x, cy - 1.5f, pos - (float) x, 3.0f, 1.5f);
    auto thumb = Rectangle<float> (pos - 13.0f, cy - 10.0f, 26.0f, 20.0f);
    Path tp; tp.addRoundedRectangle (thumb, 3.0f);
    softShadow (g, tp, 5, { 0, 2 });
    g.setGradientFill (ColourGradient (P.knobHi, 0, thumb.getY(), P.knobLo, 0, thumb.getBottom(), false));
    g.fillPath (tp);
}

void UDLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto& P = pal();
    auto r = Rectangle<float> (0, 0, (float) w, (float) h);

    if (box.getComponentID() == "slope")
    {
        g.setColour (P.text);
        Path p;
        p.startNewSubPath (2.0f, r.getBottom() - 5.0f);
        p.lineTo (r.getWidth() * 0.45f, 5.0f);
        p.lineTo (r.getWidth() * 0.62f, 5.0f);
        g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }

    r = r.reduced (2.0f);
    Path shape = trapezoid (r, 8.0f, 8.0f, true);
    softShadow (g, shape, 6, { 0, 2 });
    g.setColour (P.dark); g.fillPath (shape);

    // arrow
    const float ax = r.getRight() - 26.0f, ay = r.getCentreY();
    Path tri; tri.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (P.darkText); g.fillPath (tri);
}

void UDLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    if (box.getComponentID() == "slope")
    {
        label.setBounds (box.getWidth() / 2 - 2, box.getHeight() / 2 - 2, box.getWidth() / 2 + 4, box.getHeight() / 2 + 2);
        label.setFont (font (11.0f));
        label.setJustificationType (Justification::centredLeft);
        label.setBorderSize ({ 0, 0, 0, 0 });
        label.setColour (Label::textColourId, pal().text);
        return;
    }
    label.setBounds (14, 0, box.getWidth() - 46, box.getHeight());
    label.setFont (font (box.getHeight() * 0.42f));
    label.setJustificationType (Justification::centred);
    label.setColour (Label::textColourId, pal().darkText);
}

Font UDLookAndFeel::getComboBoxFont (ComboBox& box) { return font (box.getHeight() * 0.42f); }

void UDLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (pal().dark);
    g.setColour (pal().orange.withAlpha (0.4f));
    g.drawRect (0, 0, w, h);
}

void UDLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive,
                                       bool isHighlighted, bool isTicked, bool hasSubMenu, const String& text,
                                       const String&, const Drawable*, const Colour*)
{
    const auto& P = pal();
    if (isSeparator)
    {
        g.setColour (Colours::white.withAlpha (0.15f));
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (2, 0);
    if (isHighlighted && isActive)
    {
        g.setColour (P.orange);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
    }
    g.setColour (isActive ? Colours::white : Colours::white.withAlpha (0.4f));
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.withTrimmedLeft (24).withTrimmedRight (16), Justification::centredLeft);
    if (isTicked)
    {
        g.setColour (isHighlighted ? Colours::white : P.orange);
        g.fillEllipse ((float) r.getX() + 8.0f, (float) r.getCentreY() - 3.5f, 7.0f, 7.0f);
    }
    if (hasSubMenu)
    {
        Path tri; const float ax = (float) r.getRight() - 10.0f, ay = (float) r.getCentreY();
        tri.addTriangle (ax - 3, ay - 4, ax - 3, ay + 4, ax + 2, ay);
        g.fillPath (tri);
    }
}

Font UDLookAndFeel::getPopupMenuFont() { return font (15.0f, false); }

void UDLookAndFeel::drawTooltip (Graphics& g, const String& text, int w, int h)
{
    g.fillAll (Colour (0xff3b3b3b));
    g.setColour (Colours::white);
    g.setFont (font (13.0f, false));
    g.drawFittedText (text, Rectangle<int> (w, h).reduced (6, 2), Justification::centredLeft, 3);
}

//==============================================================================
void PillButton::paintButton (Graphics& g, bool over, bool down)
{
    const auto& P = pal();
    auto r = getLocalBounds().toFloat().reduced (3.0f, 3.0f);
    const bool on = getToggleState();

    if (dark)
    {
        Path shape = trapezoid (r, 5.0f, 4.0f, true);
        g.setColour (on ? P.orange : (over ? P.darkHi : P.dark));
        g.fillPath (shape);
        return;
    }

    Path shape = trapezoid (r, 8.0f, 8.0f, true);
    if (! (on || down)) softShadow (g, shape, 7, { 0, 3 });
    if (on)
        g.setGradientFill (ColourGradient (P.knobLo, 0, r.getY(), P.knobHi, 0, r.getBottom(), false));
    else
        g.setGradientFill (ColourGradient (P.knobHi, 0, r.getY(), P.knobLo.brighter (over ? 0.1f : 0.0f), 0, r.getBottom(), false));
    g.fillPath (shape);
    g.setColour (P.shadow.withMultipliedAlpha (0.5f));
    g.strokePath (shape, PathStrokeType (0.8f));

    g.setColour (on ? P.orange : P.textDim);
    g.setFont (font (r.getHeight() * 0.42f));
    g.drawText (text, r, Justification::centred);
}

void PowerButton::paintButton (Graphics& g, bool over, bool)
{
    const auto& P = pal();
    auto c = getToggleState() ? P.orange : P.textDim.withAlpha (over ? 1.0f : 0.7f);
    drawPowerIcon (g, getLocalBounds().toFloat().reduced (2.0f), c, 1.8f);
}

void IconButton::paintButton (Graphics& g, bool over, bool down)
{
    const auto& P = pal();
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState() || lit;
    auto iconCol = on ? P.orange : P.textDim.withAlpha (over ? 1.0f : 0.8f);

    switch (kind)
    {
        case linkRound:
        {
            auto cr = r.reduced (3.0f);
            Path p; p.addEllipse (cr);
            if (! down) softShadow (g, p, 6, { 0, 2 });
            g.setGradientFill (ColourGradient (P.knobHi, 0, cr.getY(), P.knobLo, 0, cr.getBottom(), false));
            g.fillPath (p);
            drawLinkIcon (g, cr.reduced (cr.getWidth() * 0.18f), iconCol);
            break;
        }
        case link:
            drawLinkIcon (g, r, iconCol);
            break;
        case msText:
            g.setColour (P.text.withAlpha (over ? 1.0f : 0.75f));
            g.setFont (font (r.getHeight() * 0.62f));
            g.drawText (getToggleState() ? "smp" : "ms", r, Justification::centred);
            break;
        case x2:
            g.setColour (getToggleState() ? P.orange : P.textDim.withAlpha (over ? 1.0f : 0.8f));
            g.setFont (font (r.getHeight() * 0.62f));
            g.drawText ("x2", r, Justification::centred);
            break;
        case headphones:
        {
            auto ir = r.reduced (r.getWidth() * 0.15f);
            Path band;
            band.addCentredArc (ir.getCentreX(), ir.getCentreY() + ir.getHeight() * 0.1f, ir.getWidth() * 0.4f,
                                ir.getHeight() * 0.42f, 0.0f, -MathConstants<float>::halfPi, MathConstants<float>::halfPi, true);
            g.setColour (iconCol);
            g.strokePath (band, PathStrokeType (1.8f));
            const float cupW = ir.getWidth() * 0.2f, cupH = ir.getHeight() * 0.36f, cy = ir.getCentreY() + ir.getHeight() * 0.1f;
            g.fillRoundedRectangle (ir.getCentreX() - ir.getWidth() * 0.4f - cupW * 0.5f, cy, cupW, cupH, 2.0f);
            g.fillRoundedRectangle (ir.getCentreX() + ir.getWidth() * 0.4f - cupW * 0.5f, cy, cupW, cupH, 2.0f);
            break;
        }
        case gear:
        case power:
        {
            auto cr = r.reduced (2.0f);
            Path p; p.addEllipse (cr);
            if (! down) softShadow (g, p, 6, { 0, 2 });
            g.setGradientFill (ColourGradient (P.knobHi, 0, cr.getY(), P.knobLo, 0, cr.getBottom(), false));
            g.fillPath (p);
            auto ic = cr.reduced (cr.getWidth() * 0.26f);
            if (kind == power)
            {
                drawPowerIcon (g, ic, lit ? P.orange : P.textDim, 2.0f);
            }
            else
            {
                g.setColour (P.text.withAlpha (over ? 1.0f : 0.8f));
                const auto c = ic.getCentre();
                const float ro = ic.getWidth() * 0.5f, ri = ro * 0.72f;
                Path gp;
                for (int i = 0; i < 16; ++i)
                {
                    const float a = MathConstants<float>::twoPi * (float) i / 16.0f;
                    const float rr = (i % 2 == 0) ? ro : ri;
                    auto pt = c.getPointOnCircumference (rr, a);
                    if (i == 0) gp.startNewSubPath (pt); else gp.lineTo (pt);
                    pt = c.getPointOnCircumference (rr, a + MathConstants<float>::twoPi / 16.0f * 0.7f);
                    gp.lineTo (pt);
                }
                gp.closeSubPath();
                g.strokePath (gp, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
                g.drawEllipse (c.x - ro * 0.32f, c.y - ro * 0.32f, ro * 0.64f, ro * 0.64f, 1.6f);
            }
            break;
        }
    }
}

void MeterBar::paint (Graphics& g)
{
    const auto& P = pal();
    auto r = getLocalBounds().toFloat();
    Path frame; frame.addRoundedRectangle (r, 3.0f);
    g.setColour (P.meterBg); g.fillPath (frame);

    auto inner = r.reduced (5.0f, 6.0f);
    const int segs = 40;
    const float gap = 1.6f;
    const float segH = (inner.getHeight() - gap * (float) (segs - 1)) / (float) segs;
    const float frac = jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);
    const int lit = roundToInt (frac * (float) segs);
    for (int i = 0; i < segs; ++i)
    {
        const float yy = inner.getBottom() - (float) (i + 1) * segH - (float) i * gap;
        g.setColour (i < lit ? P.orange : P.meterOff);
        g.fillRect (inner.getX(), yy, inner.getWidth(), segH);
    }
}

void PeakReadout::paint (Graphics& g)
{
    const auto& P = pal();
    auto r = getLocalBounds().toFloat();
    g.setColour (P.meterBg); g.fillRoundedRectangle (r, 3.0f);
    g.setColour (P.orange);
    g.setFont (font (r.getHeight() * 0.52f));
    g.drawText (holdDb <= -99.0f ? String ("-inf") : String (holdDb, 1), r, Justification::centred);
}

void CorrMeter::paint (Graphics& g)
{
    const auto& P = pal();
    auto r = getLocalBounds().toFloat();
    Path shape = trapezoid (r, 6.0f, 6.0f, true);
    g.setColour (P.dark); g.fillPath (shape);
    auto inner = r.reduced (12.0f, 4.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.fillRect (inner.getCentreX() - 0.5f, inner.getY(), 1.0f, inner.getHeight());
    const float x = inner.getX() + (jlimit (-1.0f, 1.0f, value) + 1.0f) * 0.5f * inner.getWidth();
    g.setColour (value < 0.0f ? Colour (0xffff3b30) : P.orange);
    g.fillRect (x - 2.5f, inner.getY(), 5.0f, inner.getHeight());
}

void PresetBar::paint (Graphics& g)
{
    const auto& P = pal();
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    Path shape = trapezoid (r, 10.0f, 9.0f, false);
    softShadow (g, shape, 6, { 0, 2 });
    g.setColour (P.dark); g.fillPath (shape);
    g.setColour (P.darkText);
    Path l, rr;
    const float cy = r.getCentreY();
    l.addTriangle (r.getX() + 24, cy, r.getX() + 31, cy - 5, r.getX() + 31, cy + 5);
    rr.addTriangle (r.getRight() - 24, cy, r.getRight() - 31, cy - 5, r.getRight() - 31, cy + 5);
    g.fillPath (l); g.fillPath (rr);
    g.setFont (font (r.getHeight() * 0.48f));
    g.drawText (name, r.reduced (44.0f, 0.0f), Justification::centred);
}

void PresetBar::mouseDown (const MouseEvent& e)
{
    if (e.x < 48)                     { if (onStep) onStep (-1); }
    else if (e.x > getWidth() - 48)  { if (onStep) onStep (1); }
    else if (onMenu) onMenu();
}

void ThemeToggle::paint (Graphics& g)
{
    const auto& P = pal();
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    Path pill; pill.addRoundedRectangle (r, r.getHeight() * 0.5f);
    softShadow (g, pill, 5, { 0, 2 });
    g.setColour (P.dark); g.fillPath (pill);

    const float d = r.getHeight() - 6.0f;
    auto sunArea  = Rectangle<float> (r.getX() + 3.0f, r.getY() + 3.0f, d, d);
    auto moonArea = Rectangle<float> (r.getRight() - 3.0f - d, r.getY() + 3.0f, d, d);
    g.setColour (P.orange);
    g.fillEllipse (dark ? moonArea : sunArea);

    // sun
    {
        auto c = sunArea.getCentre();
        g.setColour (Colours::white.withAlpha (dark ? 0.6f : 1.0f));
        g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7.0f, 7.0f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 8.0f;
            auto p1 = c.getPointOnCircumference (5.5f, a), p2 = c.getPointOnCircumference (8.0f, a);
            g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.4f);
        }
    }
    // moon
    {
        auto c = moonArea.getCentre();
        Path m; m.addEllipse (c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f);
        Path cut; cut.addEllipse (c.x - 2.0f, c.y - 9.0f, 12.0f, 12.0f);
        g.setColour (Colours::white.withAlpha (dark ? 1.0f : 0.6f));
        g.fillPath (m);
        g.setColour (dark ? P.orange : P.dark);
        g.fillPath (cut);
    }
}

//==============================================================================
// Presets
struct PresetDef { const char* name; std::vector<std::pair<const char*, float>> values; };
static const std::vector<PresetDef>& presets()
{
    static const std::vector<PresetDef> list {
        { "Default Preset",          {} },
        { "Mono Check",              { { "mono", 1 } } },
        { "Bass Mono 120 Hz",        { { "bassOn", 1 }, { "bassFreq", 120 } } },
        { "Bass Mono 200 Hz",        { { "bassOn", 1 }, { "bassFreq", 200 } } },
        { "Rumble Cut 30 Hz",        { { "hpOn", 1 }, { "hpFreq", 30 }, { "hpSlope", 3 } } },
        { "Clean Low End",           { { "hpOn", 1 }, { "hpFreq", 25 }, { "hpSlope", 3 }, { "bassOn", 1 }, { "bassFreq", 150 } } },
        { "Vocal Highpass 100 Hz",   { { "hpOn", 1 }, { "hpFreq", 100 }, { "hpSlope", 1 } } },
        { "Haas Widen",              { { "delayOn", 1 }, { "delayR", 12 } } },
        { "Widen 140 %",             { { "width", 140 } } },
        { "Narrow 60 %",             { { "width", 60 } } },
        { "Flip Polarity",           { { "invL", 1 }, { "invR", 1 }, { "invLink", 1 } } },
        { "Swap L/R",                { { "chMode", 4 } } },
        { "Left Only",               { { "chMode", 2 } } },
        { "Right Only",              { { "chMode", 3 } } },
        { "Mono Source to Stereo",   { { "chMode", 1 } } },
        { "Trim -6 dB",              { { "gain", -6 } } },
    };
    return list;
}

//==============================================================================
Content::Content (UtilDeckProcessor& p, UtilDeckEditor& e) : proc (p), editor (e)
{
    setSize (UtilDeckProcessor::baseW, UtilDeckProcessor::baseH);
    auto& st = proc.apvts;

    // ---- header ----
    addAndMakeVisible (presetBar);
    presetBar.onStep = [this] (int d)
    {
        const int n = (int) presets().size();
        applyPreset ((proc.presetIndex + d + n) % n);
    };
    presetBar.onMenu = [this] { showPresetMenu(); };

    addAndMakeVisible (themeToggle);
    themeToggle.onClick = [this]
    {
        proc.darkTheme = ! proc.darkTheme;
        editor.restoreFromProcessor();
    };
    themeToggle.setTooltip ("Light / dark theme");

    addAndMakeVisible (gearBtn);
    gearBtn.onClick = [this] { showGearMenu(); };
    gearBtn.setTooltip ("Settings");

    addAndMakeVisible (powerBtn);
    powerBtn.onClick = [this] { setParam ("bypass", getParam ("bypass") > 0.5f ? 0.0f : 1.0f); };
    powerBtn.setTooltip ("Plugin on / bypass");

    // ---- left: polarity, channels, phase, highpass ----
    for (auto* b : { &invL, &invR }) addAndMakeVisible (b);
    bAtt.push_back (std::make_unique<BA> (st, "invL", invL));
    bAtt.push_back (std::make_unique<BA> (st, "invR", invR));
    invL.setTooltip ("Flip polarity of the left channel");
    invR.setTooltip ("Flip polarity of the right channel");
    invL.onClick = [this] { if (getParam ("invLink") > 0.5f) setParam ("invR", invL.getToggleState() ? 1.0f : 0.0f); };
    invR.onClick = [this] { if (getParam ("invLink") > 0.5f) setParam ("invL", invR.getToggleState() ? 1.0f : 0.0f); };

    addAndMakeVisible (invLink);
    invLink.setClickingTogglesState (true);
    bAtt.push_back (std::make_unique<BA> (st, "invLink", invLink));
    invLink.setTooltip ("Link polarity buttons");

    addAndMakeVisible (chMode);
    chMode.addItemList ({ "Stereo", "Mono", "Left", "Right", "Swap" }, 1);
    cAtt.push_back (std::make_unique<CA> (st, "chMode", chMode));
    chMode.setTooltip ("Channel mode");

    auto hz = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, v >= 10000.0 ? 1 : 2) : String (roundToInt (v)); };

    setupKnob (phaseKnob, "phaseFreq", "Phase rotation (allpass) centre frequency");
    phaseKnob.ticks = true; phaseKnob.unit = "Hz"; phaseKnob.numberText = hz;
    addAndMakeVisible (phaseOn); bAtt.push_back (std::make_unique<BA> (st, "phaseOn", phaseOn));
    phaseOn.setTooltip ("Phase rotation on/off");

    setupKnob (hpKnob, "hpFreq", "Highpass frequency");
    hpKnob.ticks = true; hpKnob.unit = "Hz"; hpKnob.numberText = hz;
    addAndMakeVisible (hpOn); bAtt.push_back (std::make_unique<BA> (st, "hpOn", hpOn));
    hpOn.setTooltip ("Highpass on/off");

    hpSlope.setComponentID ("slope");
    hpSlope.addItemList ({ "6", "12", "18", "24", "36", "48" }, 1);
    addAndMakeVisible (hpSlope);
    cAtt.push_back (std::make_unique<CA> (st, "hpSlope", hpSlope));
    hpSlope.setTooltip ("Highpass slope (dB/oct)");

    // ---- middle: delay, width, mono, bass mono ----
    setupKnob (delayL, "delayL", "Left channel delay");
    setupKnob (delayR, "delayR", "Right channel delay");
    delayL.fixedText = "L"; delayR.fixedText = "R";
    auto linkDelay = [this] (FSlider& from, const char* otherId)
    {
        if (linking || ! from.isMouseButtonDown() || getParam ("delayLink") < 0.5f) return;
        linking = true; setParam (otherId, (float) from.getValue()); linking = false;
    };
    delayL.onValueChange = [this, linkDelay] { linkDelay (delayL, "delayR"); repaint(); };
    delayR.onValueChange = [this, linkDelay] { linkDelay (delayR, "delayL"); repaint(); };

    addAndMakeVisible (delayLink); delayLink.setClickingTogglesState (true);
    bAtt.push_back (std::make_unique<BA> (st, "delayLink", delayLink));
    delayLink.setTooltip ("Link L and R delay");
    addAndMakeVisible (delayUnit); delayUnit.setClickingTogglesState (true);
    bAtt.push_back (std::make_unique<BA> (st, "delaySmp", delayUnit));
    delayUnit.setTooltip ("Show delay in milliseconds or samples");
    delayUnit.onClick = [this] { repaint(); };
    addAndMakeVisible (delayOn); bAtt.push_back (std::make_unique<BA> (st, "delayOn", delayOn));
    delayOn.setTooltip ("Delay on/off");

    setupKnob (widthKnob, "width", "Stereo width (0 % = mono, 100 % = unchanged, 200 % = extra wide)");
    widthKnob.ticks = true; widthKnob.unit = "%";
    addAndMakeVisible (widthOn); bAtt.push_back (std::make_unique<BA> (st, "widthOn", widthOn));
    widthOn.setTooltip ("Width on/off");

    addAndMakeVisible (monoBtn);
    bAtt.push_back (std::make_unique<BA> (st, "mono", monoBtn));
    monoBtn.setTooltip ("Sum to mono (quick mono check)");

    bassBar.setSliderStyle (Slider::LinearBar);
    bassBar.unit = "Hz"; bassBar.numberText = [] (double v) { return String (roundToInt (v)); };
    bassBar.setTooltip ("Everything below this frequency is made mono");
    addAndMakeVisible (bassBar);
    sAtt.push_back (std::make_unique<SA> (st, "bassFreq", bassBar));
    bassBar.setDoubleClickReturnValue (true, 120.0);
    addAndMakeVisible (bassOn); bAtt.push_back (std::make_unique<BA> (st, "bassOn", bassOn));
    bassOn.setTooltip ("Bass mono on/off");
    addAndMakeVisible (bassSolo); bassSolo.setClickingTogglesState (true);
    bAtt.push_back (std::make_unique<BA> (st, "bassSolo", bassSolo));
    bassSolo.setTooltip ("Listen to the mono bass only");

    // ---- right: pan, meters, gain ----
    setupKnob (panL, "panL", "Pan of the left channel");
    setupKnob (panR, "panR", "Pan of the right channel");
    for (auto* k : { &panL, &panR }) { k->bipolar = true; k->unit = "%"; }
    auto mirror = [this] (FSlider& from, const char* otherId)
    {
        if (linking || ! from.isMouseButtonDown() || getParam ("panMirror") < 0.5f) return;
        linking = true; setParam (otherId, (float) -from.getValue()); linking = false;
    };
    panL.onValueChange = [mirror, this] { mirror (panL, "panR"); };
    panR.onValueChange = [mirror, this] { mirror (panR, "panL"); };
    addAndMakeVisible (panMirror); panMirror.setClickingTogglesState (true);
    bAtt.push_back (std::make_unique<BA> (st, "panMirror", panMirror));
    panMirror.setTooltip ("Mirror mode: moving one pan knob moves the other the opposite way");

    for (auto* b : { &muteL, &muteR }) addAndMakeVisible (b);
    bAtt.push_back (std::make_unique<BA> (st, "muteL", muteL));
    bAtt.push_back (std::make_unique<BA> (st, "muteR", muteR));
    muteL.setTooltip ("Mute left channel"); muteR.setTooltip ("Mute right channel");

    for (auto* c : { (Component*) &readL, (Component*) &readR, (Component*) &meterL, (Component*) &meterR, (Component*) &corr })
        addAndMakeVisible (c);
    readL.setTooltip ("Peak hold - click to reset");  // tooltips need SettableTooltipClient; harmless no-op otherwise
    readR.setTooltip ("Peak hold - click to reset");
    corr.setTooltip ("Phase correlation: +1 = mono compatible, -1 = out of phase");

    fader.setComponentID ("fader");
    fader.setSliderStyle (Slider::LinearVertical);
    fader.setTooltip ("Output gain (dB)");
    addAndMakeVisible (fader);
    sAtt.push_back (std::make_unique<SA> (st, "gain", fader));
    fader.setDoubleClickReturnValue (true, 0.0);

    trim.setComponentID ("trim");
    trim.setSliderStyle (Slider::LinearHorizontal);
    trim.setTooltip ("Trim: dry/wet mix of the whole plugin");
    trim.onValueChange = [this] { repaint (800, 606, 120, 46); };
    addAndMakeVisible (trim);
    sAtt.push_back (std::make_unique<SA> (st, "mix", trim));
    trim.setDoubleClickReturnValue (true, 100.0);

    // ---- layout (base coordinates, the whole component is scaled) ----
    presetBar.setBounds (258, 22, 402, 38);
    themeToggle.setBounds (697, 24, 62, 34);
    gearBtn.setBounds (780, 21, 40, 40);
    powerBtn.setBounds (836, 21, 40, 40);

    invL.setBounds (42, 94, 96, 52);
    invLink.setBounds (139, 99, 42, 42);
    invR.setBounds (182, 94, 96, 52);
    chMode.setBounds (92, 156, 136, 48);
    phaseKnob.setBounds (80, 222, 156, 156);
    phaseOn.setBounds (88, 377, 22, 22);
    hpKnob.setBounds (86, 414, 144, 144);
    hpOn.setBounds (80, 565, 22, 22);
    hpSlope.setBounds (201, 562, 36, 26);

    delayL.setBounds (336, 83, 80, 80);
    delayR.setBounds (500, 83, 80, 80);
    delayLink.setBounds (444, 90, 28, 24);
    delayUnit.setBounds (434, 116, 48, 28);
    delayOn.setBounds (412, 166, 22, 22);
    widthKnob.setBounds (380, 222, 156, 156);
    widthOn.setBounds (396, 377, 22, 22);
    monoBtn.setBounds (408, 433, 100, 54);
    bassBar.setBounds (350, 506, 214, 38);
    bassOn.setBounds (372, 565, 22, 22);
    bassSolo.setBounds (514, 561, 28, 28);

    panL.setBounds (636, 83, 80, 80);
    panR.setBounds (800, 83, 80, 80);
    panMirror.setBounds (766, 166, 30, 24);
    muteL.setBounds (686, 214, 56, 26);
    muteR.setBounds (775, 214, 56, 26);
    readL.setBounds (688, 248, 52, 30);
    readR.setBounds (777, 248, 52, 30);
    meterL.setBounds (688, 292, 52, 226);
    meterR.setBounds (777, 292, 52, 226);
    fader.setBounds (730, 280, 58, 250);
    corr.setBounds (682, 560, 152, 30);

    trim.setBounds (600, 616, 208, 26);

    syncFromParams();
}

void Content::setupKnob (FSlider& k, const String& id, const String& tip)
{
    k.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    k.setMouseDragSensitivity (220);
    k.setVelocityBasedMode (false);
    k.setTooltip (tip + "  (double-click to reset)");
    addAndMakeVisible (k);
    sAtt.push_back (std::make_unique<SA> (proc.apvts, id, k));
    if (auto* p = proc.apvts.getParameter (id))
        k.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Content::setParam (const String& id, float plain)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

float Content::getParam (const String& id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void Content::applyPreset (int index)
{
    const auto& list = presets();
    index = jlimit (0, (int) list.size() - 1, index);
    for (auto* ap : proc.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (ap))
            if (rp->getParameterID() != "bypass")
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
    for (auto& [id, v] : list[(size_t) index].values)
        setParam (id, v);
    proc.presetIndex = index;
    presetBar.name = list[(size_t) index].name;
    presetBar.repaint();
}

void Content::showPresetMenu()
{
    PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    const auto& list = presets();
    for (int i = 0; i < (int) list.size(); ++i)
        m.addItem (i + 1, list[(size_t) i].name, true, i == proc.presetIndex);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetBar),
                     [safe = SafePointer<Content> (this)] (int r) { if (safe && r > 0) safe->applyPreset (r - 1); });
}

void Content::showGearMenu()
{
    PopupMenu m, sizes;
    m.setLookAndFeel (&getLookAndFeel());
    sizes.setLookAndFeel (&getLookAndFeel());
    const float cur = (float) editor.getWidth() / (float) UtilDeckProcessor::baseW;
    for (int pct : { 50, 65, 75, 85, 100, 125, 150, 175 })
        sizes.addItem (1000 + pct, String (pct) + " %", true, std::abs (cur * 100.0f - (float) pct) < 1.5f);
    m.addSubMenu ("Window size", sizes);
    m.addItem (2, "Dark theme", true, proc.darkTheme);
    m.addItem (3, "Reset peak hold");
    m.addSeparator();
    m.addItem (4, "Tip: double-click any control to reset it", false, false);
    m.addItem (5, "Tip: click a peak readout to reset it", false, false);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&gearBtn),
        [safe = SafePointer<Content> (this)] (int r)
        {
            if (! safe || r == 0) return;
            if (r >= 1000) safe->editor.setScale ((float) (r - 1000) / 100.0f);
            else if (r == 2) { safe->proc.darkTheme = ! safe->proc.darkTheme; safe->editor.restoreFromProcessor(); }
            else if (r == 3) { safe->readL.holdDb = safe->readR.holdDb = -100.0f; safe->readL.repaint(); safe->readR.repaint(); }
        });
}

void Content::syncFromParams()
{
    auto setActive = [] (FSlider& s, bool a) { if (s.active != a) { s.active = a; s.repaint(); } };
    setActive (phaseKnob, getParam ("phaseOn") > 0.5f);
    setActive (hpKnob,    getParam ("hpOn") > 0.5f);
    setActive (delayL,    getParam ("delayOn") > 0.5f);
    setActive (delayR,    getParam ("delayOn") > 0.5f);
    setActive (widthKnob, getParam ("widthOn") > 0.5f);
    setActive (bassBar,   getParam ("bassOn") > 0.5f);

    const bool on = getParam ("bypass") < 0.5f;
    if (powerBtn.lit != on) { powerBtn.lit = on; powerBtn.repaint(); }

    const auto& list = presets();
    auto nm = String (list[(size_t) jlimit (0, (int) list.size() - 1, proc.presetIndex)].name);
    if (presetBar.name != nm) { presetBar.name = nm; presetBar.repaint(); }
    if (themeToggle.dark != proc.darkTheme) { themeToggle.dark = proc.darkTheme; themeToggle.repaint(); }

    const float dl = getParam ("delayL"), dr = getParam ("delayR");
    const bool smp = getParam ("delaySmp") > 0.5f;
    if (dl != lastDelayL || dr != lastDelayR || smp != lastSmp)
    {
        lastDelayL = dl; lastDelayR = dr; lastSmp = smp;
        repaint (330, 160, 260, 36);
    }
}

void Content::tickMeters()
{
    auto toDb = [] (float g) { return g > 0.0f ? Decibels::gainToDecibels (g, -100.0f) : -100.0f; };
    const float l = toDb (proc.peakL.exchange (0.0f)), r = toDb (proc.peakR.exchange (0.0f));

    auto upd = [] (MeterBar& m, PeakReadout& rd, float db)
    {
        const float shown = jmax (db, m.levelDb - 1.2f);   // ~36 dB/s release
        if (std::abs (shown - m.levelDb) > 0.05f) { m.levelDb = shown; m.repaint(); }
        if (db > rd.holdDb + 0.05f) { rd.holdDb = db; rd.repaint(); }
    };
    upd (meterL, readL, l);
    upd (meterR, readR, r);

    const float c = proc.correlation.load();
    if (std::abs (c - corr.value) > 0.005f) { corr.value = c; corr.repaint(); }
}

void Content::paint (Graphics& g)
{
    const auto& P = pal();
    const float W = (float) UtilDeckProcessor::baseW, H = (float) UtilDeckProcessor::baseH;

    g.fillAll (P.bg);

    // header
    g.setColour (P.header); g.fillRect (0.0f, 0.0f, W, 68.0f);

    // panels
    g.setColour (P.panel);
    g.fillRoundedRectangle (12.0f, 70.0f, 298.0f, 532.0f, 5.0f);
    g.fillRoundedRectangle (606.0f, 70.0f, 302.0f, 532.0f, 5.0f);
    g.setColour (P.panelMid);
    g.fillRoundedRectangle (312.0f, 70.0f, 292.0f, 532.0f, 5.0f);
    g.setColour (P.notch);
    g.fillPath (trapezoid ({ 312.0f, 210.0f, 14.0f, 240.0f }, 0.0f, 3.0f));
    g.fillPath (trapezoid ({ 590.0f, 210.0f, 14.0f, 240.0f }, 0.0f, 3.0f));
    // little tabs under each panel
    for (float cx : { 161.0f, 458.0f, 757.0f })
        g.fillPath (trapezoid ({ cx - 90.0f, 596.0f, 180.0f, 8.0f }, 5.0f, 2.0f));

    // footer
    g.setColour (P.footer); g.fillRect (0.0f, 606.0f, W, H - 606.0f);
    g.setColour (P.notch);  g.fillRect (0.0f, 606.0f, W, 1.0f);

    // logo
    {
        const float cx = 46.0f, cy = 41.0f, r = 11.0f;
        g.setColour (P.orange);
        Path ring; ring.addCentredArc (cx, cy, r, r, 0.0f, 0.0f, MathConstants<float>::twoPi * 0.75f, true);
        g.strokePath (ring, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (P.text);
        g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
        g.setFont (font (21.0f).withExtraKerningFactor (0.18f));
        g.drawText ("UTILDECK", Rectangle<float> (66.0f, 26.0f, 180.0f, 30.0f), Justification::centredLeft);
    }

    // footer text
    g.setColour (P.text);
    g.setFont (font (17.0f).withExtraKerningFactor (0.12f));
    g.drawText ("UTILDECK", Rectangle<float> (34.0f, 614.0f, 130.0f, 30.0f), Justification::centredLeft);
    g.setColour (P.textDim);
    g.setFont (font (12.0f, false));
    g.drawText ("by Ireground", Rectangle<float> (150.0f, 618.0f, 120.0f, 24.0f), Justification::centredLeft);

    g.setColour (P.text);
    g.setFont (font (17.0f, false));
    g.drawText ("Trim", Rectangle<float> (520.0f, 616.0f, 72.0f, 26.0f), Justification::centredRight);
    g.drawText (String (roundToInt (getParam ("mix"))) + " %", Rectangle<float> (812.0f, 616.0f, 80.0f, 26.0f),
                Justification::centredLeft);

    // section labels
    auto label = [&] (const String& t, float x, float y, float w, Justification j = Justification::centredLeft, bool dim = false)
    {
        g.setColour (dim ? P.textDim : P.text);
        g.setFont (font (18.0f, false));
        g.drawText (t, Rectangle<float> (x, y, w, 26.0f), j);
    };
    label ("Phase",    114.0f, 375.0f, 120.0f);
    label ("Highpass", 106.0f, 563.0f, 100.0f);
    label ("Delay",    438.0f, 164.0f, 70.0f);
    label ("Width",    422.0f, 375.0f, 90.0f);
    label ("Bassmono", 398.0f, 563.0f, 120.0f);
    label ("L",        636.0f, 164.0f, 80.0f, Justification::centred);
    label ("Pan",      718.0f, 164.0f, 46.0f, Justification::centred);
    label ("R",        800.0f, 164.0f, 80.0f, Justification::centred);
    label ("L",        690.0f, 520.0f, 50.0f, Justification::centred);
    label ("Gain",     728.0f, 520.0f, 60.0f, Justification::centred);
    label ("R",        777.0f, 520.0f, 50.0f, Justification::centred);

    // delay values
    {
        const bool smp = getParam ("delaySmp") > 0.5f;
        auto fmt = [&] (float ms)
        {
            if (smp) return String (roundToInt (ms * 0.001 * proc.getSampleRate()));
            return ms < 10.0f ? String (ms, 2) : (ms < 100.0f ? String (ms, 1) : String (roundToInt (ms)));
        };
        const bool on = getParam ("delayOn") > 0.5f;
        g.setColour (on ? P.orange : P.textDim);
        g.setFont (font (16.0f, false));
        g.drawText (fmt (getParam ("delayL")), Rectangle<float> (336.0f, 166.0f, 80.0f, 24.0f), Justification::centred);
        g.drawText (fmt (getParam ("delayR")), Rectangle<float> (500.0f, 166.0f, 80.0f, 24.0f), Justification::centred);
    }

    // meter scale
    g.setFont (font (11.5f, false));
    g.setColour (P.textDim);
    {
        const auto m = meterL.getBounds().toFloat().reduced (0.0f, 6.0f);
        for (int db = 0; db >= -60; db -= 12)
        {
            const float y = m.getY() + (float) -db / 60.0f * m.getHeight();
            g.drawText (String (db), Rectangle<float> (640.0f, y - 8.0f, 38.0f, 16.0f), Justification::centredRight);
            g.drawText (String (db), Rectangle<float> (838.0f, y - 8.0f, 38.0f, 16.0f), Justification::centredLeft);
        }
    }
    g.drawText ("-1", Rectangle<float> (650.0f, 563.0f, 28.0f, 24.0f), Justification::centredRight);
    g.drawText ("+1", Rectangle<float> (838.0f, 563.0f, 28.0f, 24.0f), Justification::centredLeft);
}

//==============================================================================
UtilDeckEditor::UtilDeckEditor (UtilDeckProcessor& p)
    : AudioProcessorEditor (p), proc (p), content (p, *this)
{
    setDarkPalette (proc.darkTheme);
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    const double ratio = (double) UtilDeckProcessor::baseW / (double) UtilDeckProcessor::baseH;
    setResizable (true, true);
    setResizeLimits (UtilDeckProcessor::baseW / 2, UtilDeckProcessor::baseH / 2,
                     UtilDeckProcessor::baseW * 2, UtilDeckProcessor::baseH * 2);
    getConstrainer()->setFixedAspectRatio (ratio);
    setSize (proc.uiW, proc.uiH);
    ready = true;
    resized();

    proc.editorOpen = true;
    startTimerHz (30);
}

UtilDeckEditor::~UtilDeckEditor()
{
    proc.editorOpen = false;
    setLookAndFeel (nullptr);
}

void UtilDeckEditor::paint (Graphics& g) { g.fillAll (pal().bg); }

void UtilDeckEditor::resized()
{
    const float s = (float) getWidth() / (float) UtilDeckProcessor::baseW;
    content.setTransform (AffineTransform::scale (s));
    if (ready)
    {
        proc.uiW = getWidth();
        proc.uiH = getHeight();
    }
}

void UtilDeckEditor::setScale (float s)
{
    setSize (roundToInt (UtilDeckProcessor::baseW * s), roundToInt (UtilDeckProcessor::baseH * s));
}

void UtilDeckEditor::restoreFromProcessor()
{
    setDarkPalette (proc.darkTheme);
    if (getWidth() != proc.uiW || getHeight() != proc.uiH)
        setSize (proc.uiW, proc.uiH);
    content.syncFromParams();
    repaint();
    content.repaint();
    for (auto* c : content.getChildren()) c->repaint();
}

void UtilDeckEditor::timerCallback()
{
    content.syncFromParams();
    content.tickMeters();
}
