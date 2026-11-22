#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

//==============================================================================
//  Paleta: negro carbón, acero cepillado y rojo sangre
//==============================================================================
namespace col
{
    inline const juce::Colour bg       { 0xff09090b };
    inline const juce::Colour panelTop { 0xff2b2c31 };
    inline const juce::Colour panelBot { 0xff131315 };
    inline const juce::Colour steelHi  { 0xffa0a4ac };
    inline const juce::Colour steel    { 0xff6c7078 };
    inline const juce::Colour steelDk  { 0xff2c2d31 };
    inline const juce::Colour blood    { 0xffc8101a };
    inline const juce::Colour glow     { 0xffff3b30 };
    inline const juce::Colour ember    { 0xffff7a1a };
    inline const juce::Colour bone     { 0xffe9e5dc };
    inline const juce::Colour text     { 0xffb4b7be };
    inline const juce::Colour dim      { 0xff7b7e86 };
    inline const juce::Colour screen   { 0xff0b0b0d };
}

namespace gfx
{
inline juce::Font titleFont (float h)
{
    juce::Font f (juce::FontOptions ("Impact", h, juce::Font::plain));
    f.setExtraKerningFactor (0.07f);
    return f;
}

inline juce::Font uiFont (float h, bool bold = true)
{
    juce::Font f (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    f.setExtraKerningFactor (0.04f);
    return f;
}

// Textura de "acero cepillado": se repite en mosaico sobre los paneles
inline juce::Image makeBrushedTexture()
{
    juce::Image img (juce::Image::ARGB, 256, 256, true);
    juce::Graphics g (img);
    juce::Random rnd (1337);
    for (int y = 0; y < 256; ++y)
        for (int x = 0; x < 256;)
        {
            const int len = 8 + rnd.nextInt (90);
            const bool light = rnd.nextBool();
            g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (rnd.nextFloat() * 0.6f));
            g.fillRect (x, y, len, 1);
            x += len;
        }
    return img;
}

inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float angle)
{
    const float r = 4.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb0b4bc), c.x - 2, c.y - 2,
                                              juce::Colour (0xff25262a), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colour (0xff09090a));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.f);
    juce::Path slot;
    slot.addRectangle (-r * 0.8f, -0.8f, r * 1.6f, 1.6f);
    g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

inline void drawMetalPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& tex)
{
    juce::Path p;
    p.addRoundedRectangle (r, 7.f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));            // sombra
    g.fillRoundedRectangle (r.translated (0.f, 2.f), 7.f);

    g.setGradientFill (juce::ColourGradient (col::panelTop, r.getX(), r.getY(), col::panelBot, r.getX(), r.getBottom(), false));
    g.fillPath (p);
    g.setTiledImageFill (tex, 0, 0, 0.11f);
    g.fillPath (p);

    g.setColour (juce::Colour (0x40ffffff));                         // brillo del borde superior
    g.drawLine (r.getX() + 8.f, r.getY() + 1.f, r.getRight() - 8.f, r.getY() + 1.f, 1.f);
    g.setColour (juce::Colour (0xff050506));
    g.strokePath (p, juce::PathStrokeType (1.6f));

    drawScrew (g, { r.getX() + 9.f,     r.getY() + 9.f },      0.4f);
    drawScrew (g, { r.getRight() - 9.f, r.getY() + 9.f },      1.9f);
    drawScrew (g, { r.getX() + 9.f,     r.getBottom() - 9.f }, 2.6f);
    drawScrew (g, { r.getRight() - 9.f, r.getBottom() - 9.f }, 0.9f);
}

inline void drawScreen (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141417), r.getX(), r.getY(), col::screen, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (juce::Colour (0xff000000));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.5f);
    g.setColour (juce::Colour (0x22ffffff));
    g.drawRoundedRectangle (r.expanded (1.f), 5.f, 1.f);
}
} // namespace gfx

//==============================================================================
//  LookAndFeel: knobs de acero dentados con aguja roja, botones con LED
//==============================================================================
class MetalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MetalLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, col::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0e0e10));
        setColour (juce::TextEditor::textColourId, col::bone);
        setColour (juce::TextEditor::outlineColourId, col::blood);
        setColour (juce::TextEditor::focusedOutlineColourId, col::glow);
        setColour (juce::TextEditor::highlightColourId, col::blood);
        setColour (juce::CaretComponent::caretColourId, col::glow);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        if (dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr)
            return gfx::uiFont (12.5f, false);
        return l.getFont();
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        using namespace juce;
        const auto bounds = Rectangle<int> (x, y, w, h).toFloat().reduced (1.f);
        const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const bool enabled = s.isEnabled();

        // --- carril + arco de valor ---
        const float arcR = r - 2.5f;
        Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.f, startAngle, endAngle, true);
        g.setColour (Colour (0xff040405));
        g.strokePath (track, PathStrokeType (4.5f, PathStrokeType::curved, PathStrokeType::rounded));

        float a0 = startAngle, a1 = angle;
        if (bipolar)
        {
            const float mid = startAngle + 0.5f * (endAngle - startAngle);
            a0 = jmin (mid, angle);  a1 = jmax (mid, angle);
        }
        if (a1 - a0 > 0.01f)
        {
            Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.f, a0, a1, true);
            g.setColour (col::glow.withAlpha (enabled ? 0.28f : 0.08f));
            g.strokePath (val, PathStrokeType (8.f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (enabled ? col::glow : col::dim);
            g.strokePath (val, PathStrokeType (3.f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // --- cuerpo del knob: engranaje dentado ("knurled") ---
        const float rk = arcR - 5.f;
        g.setColour (Colours::black.withAlpha (0.65f));
        g.fillEllipse (c.x - rk - 1.f, c.y - rk + 2.f, rk * 2.f + 2.f, rk * 2.f + 2.f);

        const int teeth = 22;
        Path gear;
        for (int i = 0; i < teeth * 2; ++i)
        {
            const float a  = (float) i * MathConstants<float>::pi / (float) teeth;
            const float rr = (i % 2 == 0) ? rk : rk - 3.2f;
            const float px = c.x + std::sin (a) * rr, py = c.y - std::cos (a) * rr;
            if (i == 0) gear.startNewSubPath (px, py); else gear.lineTo (px, py);
        }
        gear.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xff80848c), c.x - rk * 0.7f, c.y - rk * 0.9f,
                                           Colour (0xff141517), c.x + rk * 0.7f, c.y + rk, false));
        g.fillPath (gear);
        g.setColour (Colour (0xff050506));
        g.strokePath (gear, PathStrokeType (1.2f));

        // --- tapa central ---
        const float rc = rk * 0.66f;
        g.setGradientFill (ColourGradient (Colour (0xff1b1c1f), c.x, c.y - rc, Colour (0xff3d3f46), c.x, c.y + rc, false));
        g.fillEllipse (c.x - rc, c.y - rc, rc * 2.f, rc * 2.f);
        g.setColour (Colour (0x30ffffff));
        g.drawEllipse (c.x - rc, c.y - rc, rc * 2.f, rc * 2.f, 1.f);
        g.setColour (Colour (0xff050506));
        g.drawEllipse (c.x - rc - 1.f, c.y - rc - 1.f, rc * 2.f + 2.f, rc * 2.f + 2.f, 1.f);

        // --- aguja ---
        Path needle;
        needle.addRoundedRectangle (-1.8f, -rc * 0.95f, 3.6f, rc * 0.62f, 1.6f);
        const auto xf = AffineTransform::rotation (angle).translated (c.x, c.y);
        g.setColour (col::glow.withAlpha (enabled ? 0.35f : 0.f));
        g.strokePath (needle, PathStrokeType (4.f), xf);
        g.setColour (enabled ? col::bone : col::dim);
        g.fillPath (needle, xf);
        g.setColour (enabled ? col::glow : col::steel);
        Path tip;
        tip.addRoundedRectangle (-1.8f, -rc * 0.95f, 3.6f, rc * 0.25f, 1.4f);
        g.fillPath (tip, xf);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool /*down*/) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat();
        const float led = jmin (r.getHeight() - 8.f, 15.f);
        const Rectangle<float> ledR (r.getX() + 4.f, r.getCentreY() - led * 0.5f, led, led);
        const bool on = b.getToggleState();

        if (on)
        {
            g.setColour (col::glow.withAlpha (0.30f));
            g.fillEllipse (ledR.expanded (5.f));
        }
        g.setGradientFill (ColourGradient (on ? col::glow.brighter (0.5f) : Colour (0xff2a1a1a), ledR.getCentreX(), ledR.getY(),
                                           on ? col::blood.darker (0.5f) : Colours::black, ledR.getCentreX(), ledR.getBottom(), false));
        g.fillEllipse (ledR);
        g.setColour (Colour (0xff0b0b0d));
        g.drawEllipse (ledR, 2.f);
        g.setColour (Colour (0x40ffffff));
        g.drawEllipse (ledR.expanded (1.f), 1.f);

        g.setFont (gfx::titleFont (17.f));
        g.setColour (on ? col::bone : (over ? col::text : col::dim));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (ledR.getRight() + 8.f), Justification::centredLeft, false);
    }
};

//==============================================================================
//  Knob con nombre arriba y valor abajo (editable con doble clic en el número)
//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const char* paramId, const juce::String& name, bool bipolar = false)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        slider.getProperties().set ("bipolar", bipolar);
        addAndMakeVisible (slider);

        label.setText (name.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (gfx::uiFont (11.f));
        label.setColour (juce::Label::textColourId, col::steelHi);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }

    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
//  Medidor LED segmentado (nivel, o reducción de ganancia que cae desde arriba)
//==============================================================================
class MeterBar : public juce::Component
{
public:
    MeterBar (int numChannels, bool isGainReduction, float rangeDb)
        : channels (numChannels), gr (isGainReduction), range (rangeDb)
    {
        disp.fill (0.f);  peak.fill (0.f);  hold.fill (0.f);
        setInterceptsMouseClicks (false, false);
    }

    // Para medidores de nivel: valores lineales. Para GR: dB positivos.
    void update (float v0, float v1, float dt)
    {
        const float in[2] = { v0, v1 };
        for (int c = 0; c < channels; ++c)
        {
            float v = gr ? in[c] : juce::Decibels::gainToDecibels (in[c], -range);   // GR: 0..range | nivel: -range..0
            if (! gr) v += range;                                                    // 0..range
            v = juce::jlimit (0.f, range, v);

            disp[(size_t) c] = std::max (v, disp[(size_t) c] - (gr ? 70.f : 40.f) * dt);
            if (v >= peak[(size_t) c]) { peak[(size_t) c] = v; hold[(size_t) c] = 0.9f; }
            else if ((hold[(size_t) c] -= dt) <= 0.f) peak[(size_t) c] = std::max (0.f, peak[(size_t) c] - 25.f * dt);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        gfx::drawScreen (g, r);
        r = r.reduced (3.f);

        const int nSeg = 30;
        const float gap = 1.5f;
        const float chW = (r.getWidth() - gap * (float) (channels - 1)) / (float) channels;
        const float segH = (r.getHeight() - gap * (float) (nSeg - 1)) / (float) nSeg;

        for (int c = 0; c < channels; ++c)
        {
            const float x = r.getX() + (float) c * (chW + gap);
            const int lit = (int) std::ceil (disp[(size_t) c] / range * (float) nSeg - 0.01f);
            const int pk  = (int) std::ceil (peak[(size_t) c] / range * (float) nSeg - 0.01f);

            for (int s = 0; s < nSeg; ++s)
            {
                const float frac = (float) s / (float) (nSeg - 1);              // 0 = abajo
                Colour on = gr ? col::ember : (frac < 0.62f ? col::blood : frac < 0.86f ? col::ember : Colour (0xffffd23f));
                const bool isLit = s < lit;      // nivel: sube desde abajo · GR: baja desde arriba (la posición y ya lo gestiona)
                const bool isPeak = ! gr && pk > 0 && s == pk - 1;
                const float y = gr ? r.getY() + (float) s * (segH + gap)
                                   : r.getBottom() - (float) (s + 1) * segH - (float) s * gap;
                g.setColour (isLit || isPeak ? on.brighter (isPeak ? 0.4f : 0.f) : Colour (0xff1c1214));
                g.fillRect (x, y, chW, segH);
            }
        }
    }

private:
    int channels;
    bool gr;
    float range;
    std::array<float, 2> disp, peak, hold;
};

//==============================================================================
//  Gráfico con histórico: nivel de entrada (gris) y reducción de ganancia (rojo, desde arriba)
//==============================================================================
class HistoryDisplay : public juce::Component
{
public:
    HistoryDisplay (float grRangeDb, float floorDb = -60.f) : grRange (grRangeDb), floor (floorDb)
    {
        level.assign ((size_t) N, floorDb);
        grv.assign ((size_t) N, 0.f);
        setInterceptsMouseClicks (false, false);
    }

    void push (float levelDb, float grDb)
    {
        level[(size_t) pos] = levelDb;
        grv[(size_t) pos]   = grDb;
        pos = (pos + 1) % N;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        gfx::drawScreen (g, r);
        r = r.reduced (2.f);
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());

        g.setColour (Colour (0x14ffffff));                                   // rejilla
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.f), r.getX(), r.getRight());

        Path lv, gp;
        lv.startNewSubPath (r.getX(), r.getBottom());
        gp.startNewSubPath (r.getX(), r.getY());
        for (int i = 0; i < N; ++i)
        {
            const int idx = (pos + i) % N;
            const float x = r.getX() + r.getWidth() * (float) i / (float) (N - 1);
            const float ly = jmap (jlimit (floor, 0.f, level[(size_t) idx]), floor, 0.f, r.getBottom(), r.getY());
            const float gy = r.getY() + r.getHeight() * jlimit (0.f, 1.f, grv[(size_t) idx] / grRange);
            lv.lineTo (x, ly);
            gp.lineTo (x, gy);
        }
        lv.lineTo (r.getRight(), r.getBottom());  lv.closeSubPath();
        gp.lineTo (r.getRight(), r.getY());       gp.closeSubPath();

        g.setGradientFill (ColourGradient (Colour (0xff8a8e96).withAlpha (0.55f), 0, r.getY(),
                                           Colour (0xff3a3c42).withAlpha (0.20f), 0, r.getBottom(), false));
        g.fillPath (lv);
        g.setGradientFill (ColourGradient (col::glow.withAlpha (0.85f), 0, r.getY(),
                                           col::blood.withAlpha (0.55f), 0, r.getBottom(), false));
        g.fillPath (gp);
        g.setColour (col::glow);
        g.strokePath (gp, PathStrokeType (1.2f));
        g.restoreState();
    }

private:
    static constexpr int N = 300;
    float grRange, floor;
    std::vector<float> level, grv;
    int pos = 0;
};

//==============================================================================
//  Curva de transferencia del compresor con punto de nivel en vivo
//==============================================================================
class CompCurve : public juce::Component
{
public:
    explicit CompCurve (juce::AudioProcessorValueTreeState& a) : apvts (a) { setInterceptsMouseClicks (false, false); }

    void setInputDb (float db) { inDb = db; repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto full = getLocalBounds().toFloat();
        gfx::drawScreen (g, full);
        const auto r = full.reduced (6.f);

        const float thr   = apvts.getRawParameterValue (ids::cThresh)->load();
        const float ratio = apvts.getRawParameterValue (ids::cRatio)->load();
        const float knee  = apvts.getRawParameterValue (ids::cKnee)->load();
        const bool on     = apvts.getRawParameterValue (ids::cOn)->load() > 0.5f;

        auto X = [&] (float db) { return jmap (db, -60.f, 0.f, r.getX(), r.getRight()); };
        auto Y = [&] (float db) { return jmap (db, -60.f, 0.f, r.getBottom(), r.getY()); };

        g.setColour (Colour (0x14ffffff));
        for (int d = -48; d < 0; d += 12)
        {
            g.drawVerticalLine ((int) X ((float) d), r.getY(), r.getBottom());
            g.drawHorizontalLine ((int) Y ((float) d), r.getX(), r.getRight());
        }
        g.setColour (Colour (0x30ffffff));                               // diagonal 1:1
        g.drawLine (X (-60.f), Y (-60.f), X (0.f), Y (0.f), 1.f);

        Path curve;
        for (int px = 0; px <= (int) r.getWidth(); ++px)
        {
            const float i = -60.f + 60.f * (float) px / r.getWidth();
            const float o = ifg::Compressor::staticCurveDb (i, thr, ratio, knee);
            const float yy = Y (jlimit (-60.f, 0.f, o));
            if (px == 0) curve.startNewSubPath (r.getX(), yy); else curve.lineTo (r.getX() + (float) px, yy);
        }
        const float a = on ? 1.f : 0.35f;
        g.setColour (col::glow.withAlpha (0.25f * a));
        g.strokePath (curve, PathStrokeType (6.f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (col::glow.withAlpha (a));
        g.strokePath (curve, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));

        g.setColour (col::dim);                                          // marca del threshold
        g.fillRect (X (thr) - 1.f, r.getBottom() - 5.f, 2.f, 5.f);

        if (on && inDb > -60.f)
        {
            const float o = ifg::Compressor::staticCurveDb (inDb, thr, ratio, knee);
            const Point<float> p (X (inDb), Y (jlimit (-60.f, 0.f, o)));
            g.setColour (col::bone.withAlpha (0.3f));
            g.fillEllipse (p.x - 7.f, p.y - 7.f, 14.f, 14.f);
            g.setColour (col::bone);
            g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.f, 7.f);
        }
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    float inDb = -100.f;
};

//==============================================================================
//  Pantalla del EQ: analizador de espectro + curva de respuesta + nodos arrastrables
//    arrastrar nodo = frecuencia/ganancia · rueda = Q · doble clic = ganancia a 0
//==============================================================================
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (IronForgeProcessor& p)
        : proc (p), apvts (p.apvts), fft (fftOrder),
          window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        spectrum.fill (-120.f);
        rolling.fill (0.f);
    }

    //-------------------------------------------------------------- análisis
    void tick()
    {
        const int ready = proc.scopeFifo.getNumReady();
        if (ready > 0)
        {
            int s1, z1, s2, z2;
            proc.scopeFifo.prepareToRead (ready, s1, z1, s2, z2);
            for (int i = 0; i < z1; ++i) pushSample (proc.scopeData[(size_t) (s1 + i)]);
            for (int i = 0; i < z2; ++i) pushSample (proc.scopeData[(size_t) (s2 + i)]);
            proc.scopeFifo.finishedRead (z1 + z2);
        }

        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = rolling[(size_t) ((wp + i) % fftSize)];
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        for (int k = 0; k < fftSize / 2; ++k)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] * 4.f / (float) fftSize, -120.f);
            auto& s = spectrum[(size_t) k];
            s += (db - s) * (db > s ? 0.55f : 0.10f);
        }
        repaint();
    }

    //-------------------------------------------------------------- dibujo
    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto full = getLocalBounds().toFloat();
        gfx::drawScreen (g, full);
        const auto r = plotArea();
        const bool eqOn = apvts.getRawParameterValue (ids::eqOn)->load() > 0.5f;
        const float sr = (float) (proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0);

        g.saveState();
        g.reduceClipRegion (r.getSmallestIntegerContainer());

        // rejilla
        const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (float f : freqs)
        {
            g.setColour (Colour ((int) f == 100 || (int) f == 1000 || (int) f == 10000 ? 0x22ffffff : 0x0fffffff));
            g.drawVerticalLine ((int) freqToX (f, r), r.getY(), r.getBottom());
        }
        for (int d = -18; d <= 18; d += 6)
        {
            g.setColour (Colour (d == 0 ? 0x38ffffff : 0x0fffffff));
            g.drawHorizontalLine ((int) dbToY ((float) d, r), r.getX(), r.getRight());
        }

        // espectro
        {
            Path sp;
            sp.startNewSubPath (r.getX(), r.getBottom());
            for (float x = r.getX(); x <= r.getRight() + 1.f; x += 2.f)
            {
                const float f = xToFreq (x, r);
                const float bin = f * (float) fftSize / sr;
                const int i0 = jlimit (0, fftSize / 2 - 2, (int) bin);
                const float frac = jlimit (0.f, 1.f, bin - (float) i0);
                float db = spectrum[(size_t) i0] + (spectrum[(size_t) i0 + 1] - spectrum[(size_t) i0]) * frac;
                db += 3.f * std::log2 (f / 1000.f);                                  // inclinación de +3 dB/oct
                sp.lineTo (x, jmap (jlimit (-100.f, -10.f, db), -100.f, -10.f, r.getBottom(), r.getY()));
            }
            sp.lineTo (r.getRight(), r.getBottom());
            sp.closeSubPath();
            g.setGradientFill (ColourGradient (Colour (0xff9a9ea6).withAlpha (0.42f), 0, r.getY(),
                                               Colour (0xff3a3c42).withAlpha (0.08f), 0, r.getBottom(), false));
            g.fillPath (sp);
            g.setColour (Colour (0x55c8ccd4));
            g.strokePath (sp, PathStrokeType (1.f));
        }

        // curva del EQ (suma de todos los filtros)
        {
            std::array<ifg::BiquadCoeffs, 4> bc;
            for (size_t i = 0; i < 4; ++i)
                bc[i] = ifg::makeBiquad (ifg::Equalizer::bandType[i], sr, val (ids::bandFreq[i]), val (ids::bandGain[i]), val (ids::bandQ[i]));
            const bool hpfOn = val (ids::hpf) > 20.5f;
            const auto h1 = ifg::makeBiquad (ifg::FilterType::HighPass, sr, val (ids::hpf), 0.0, ifg::Equalizer::hpfQ1);
            const auto h2 = ifg::makeBiquad (ifg::FilterType::HighPass, sr, val (ids::hpf), 0.0, ifg::Equalizer::hpfQ2);

            Path curve, fill;
            bool first = true;
            const float y0 = dbToY (0.f, r);
            fill.startNewSubPath (r.getX(), y0);
            for (float x = r.getX(); x <= r.getRight() + 1.f; x += 2.f)
            {
                const double f = xToFreq (x, r);
                double db = 0;
                for (auto& c : bc) db += ifg::biquadMagnitudeDb (c, f, sr);
                if (hpfOn) db += ifg::biquadMagnitudeDb (h1, f, sr) + ifg::biquadMagnitudeDb (h2, f, sr);
                const float y = dbToY (jlimit (-24.f, 24.f, (float) db), r);
                if (first) { curve.startNewSubPath (x, y); first = false; } else curve.lineTo (x, y);
                fill.lineTo (x, y);
            }
            fill.lineTo (r.getRight(), y0);
            fill.closeSubPath();

            const float a = eqOn ? 1.f : 0.3f;
            g.setColour (col::blood.withAlpha (0.22f * a));
            g.fillPath (fill);
            g.setColour (col::glow.withAlpha (0.25f * a));
            g.strokePath (curve, PathStrokeType (6.f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (col::glow.withAlpha (a));
            g.strokePath (curve, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // nodos
        for (int i = 0; i < 4; ++i)
        {
            const auto c = nodePos (i, r);
            const bool active = (i == dragging) || (dragging < 0 && i == hover);
            const float rad = active ? 10.f : 8.f;
            g.setColour (col::glow.withAlpha (active ? 0.5f : 0.22f));
            g.fillEllipse (c.x - rad - 5.f, c.y - rad - 5.f, (rad + 5.f) * 2.f, (rad + 5.f) * 2.f);
            g.setGradientFill (ColourGradient (active ? col::bone : col::steelHi, c.x, c.y - rad,
                                               active ? col::steelHi : col::steelDk, c.x, c.y + rad, false));
            g.fillEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f);
            g.setColour (eqOn ? col::blood : col::dim);
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f, 2.f);
            g.setColour (Colour (0xff0b0b0d));
            g.setFont (gfx::uiFont (11.f));
            g.drawText (String (i + 1), Rectangle<float> (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f), Justification::centred, false);
        }
        g.restoreState();

        // etiquetas
        g.setFont (gfx::uiFont (10.f, false));
        g.setColour (col::dim);
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
            g.drawText (f >= 1000.f ? String ((int) (f / 1000.f)) + "k" : String ((int) f),
                        Rectangle<float> (freqToX (f, r) - 20.f, r.getBottom() + 1.f, 40.f, 14.f), Justification::centred, false);
        for (int d = -18; d <= 18; d += 6)
            g.drawText ((d > 0 ? "+" : "") + String (d), Rectangle<float> (full.getX() + 2.f, dbToY ((float) d, r) - 7.f, 24.f, 14.f),
                        Justification::centredRight, false);

        if (hover >= 0 || dragging >= 0)
        {
            const int i = dragging >= 0 ? dragging : hover;
            g.setFont (gfx::uiFont (12.f));
            g.setColour (col::bone);
            g.drawText (String (val (ids::bandFreq[i]), 0) + " Hz   " + String (val (ids::bandGain[i]), 1) + " dB   Q " + String (val (ids::bandQ[i]), 2),
                        Rectangle<float> (r.getX() + 8.f, r.getY() + 4.f, 300.f, 16.f), Justification::centredLeft, false);
        }
    }

    //-------------------------------------------------------------- ratón
    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = hitTest (e.position);
        if (h != hover) { hover = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = hitTest (e.position);
        if (dragging >= 0)
        {
            gesture (ids::bandFreq[dragging], true);
            gesture (ids::bandGain[dragging], true);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;
        const auto r = plotArea();
        const float x = juce::jlimit (r.getX(), r.getRight(), e.position.x);
        const float y = juce::jlimit (r.getY(), r.getBottom(), e.position.y);
        setParam (ids::bandFreq[dragging], xToFreq (x, r));
        setParam (ids::bandGain[dragging], std::round (yToDb (y, r) * 10.f) / 10.f);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging >= 0)
        {
            gesture (ids::bandFreq[dragging], false);
            gesture (ids::bandGain[dragging], false);
        }
        dragging = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = hitTest (e.position);
        if (h < 0) return;
        gesture (ids::bandGain[h], true);
        setParam (ids::bandGain[h], 0.f);
        gesture (ids::bandGain[h], false);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int h = hitTest (e.position);
        if (h < 0) return;
        gesture (ids::bandQ[h], true);
        setParam (ids::bandQ[h], val (ids::bandQ[h]) * std::exp (w.deltaY * 1.2f));
        gesture (ids::bandQ[h], false);
        repaint();
    }

private:
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    static constexpr float dbRange = 20.f;

    juce::Rectangle<float> plotArea() const
    {
        return getLocalBounds().toFloat().withTrimmedLeft (30.f).withTrimmedBottom (16.f).reduced (0.f, 4.f).withTrimmedRight (6.f);
    }
    static float freqToX (float f, juce::Rectangle<float> r) { return r.getX() + r.getWidth() * std::log (f / 20.f) / std::log (1000.f); }
    static float xToFreq (float x, juce::Rectangle<float> r) { return 20.f * std::exp (std::log (1000.f) * (x - r.getX()) / r.getWidth()); }
    static float dbToY (float db, juce::Rectangle<float> r)  { return r.getCentreY() - db / (2.f * dbRange) * r.getHeight(); }
    static float yToDb (float y, juce::Rectangle<float> r)   { return (r.getCentreY() - y) / r.getHeight() * 2.f * dbRange; }

    float val (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    void setParam (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    void gesture (const char* id, bool begin)
    {
        if (auto* p = apvts.getParameter (id))
        {
            if (begin) p->beginChangeGesture(); else p->endChangeGesture();
        }
    }

    juce::Point<float> nodePos (int i, juce::Rectangle<float> r) const
    {
        return { freqToX (val (ids::bandFreq[i]), r), dbToY (val (ids::bandGain[i]), r) };
    }

    int hitTest (juce::Point<float> p) const
    {
        const auto r = plotArea();
        int best = -1;
        float bestD = 14.f;
        for (int i = 0; i < 4; ++i)
        {
            const float d = nodePos (i, r).getDistanceFrom (p);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void pushSample (float s) { rolling[(size_t) wp] = s; wp = (wp + 1) % fftSize; }

    IronForgeProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize> rolling {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2 + 1> spectrum {};
    int wp = 0, hover = -1, dragging = -1;
};
