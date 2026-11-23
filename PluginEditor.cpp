#include "PluginEditor.h"
#include "UI.h"

using namespace juce;

//==============================================================================
//  Panel principal: tamaño lógico fijo (1100 x 680) que se escala al redimensionar la ventana
//==============================================================================
class MainPanel : public Component, private Timer
{
public:
    static constexpr int W = 1290, H = 680;

    explicit MainPanel (IronHitsProcessor& p)
        : proc (p), eqDisplay (p), compCurve (p.apvts),
          compHistory (24.f), limHistory (12.f),
          inMeter (2, false, 48.f), outMeter (2, false, 48.f),
          compGrMeter (1, true, 24.f), limGrMeter (1, true, 12.f),
          loudM (1, false, 54.f), loudS (1, false, 54.f), rmsLMeter (1, false, 54.f), rmsRMeter (1, false, 54.f)
    {
        setLookAndFeel (&laf);
        texture = gfx::makeBrushedTexture();

        for (auto* b : { &eqBtn, &compBtn, &limBtn })
            addAndMakeVisible (*b);
        using BA = AudioProcessorValueTreeState::ButtonAttachment;
        eqAtt   = std::make_unique<BA> (p.apvts, ids::eqOn, eqBtn);
        compAtt = std::make_unique<BA> (p.apvts, ids::cOn,  compBtn);
        limAtt  = std::make_unique<BA> (p.apvts, ids::lOn,  limBtn);

        for (auto* c : std::initializer_list<Component*> { &eqDisplay, &compCurve, &compHistory, &limHistory,
                                                           &inMeter, &outMeter, &compGrMeter, &limGrMeter,
                                                           &loudM, &loudS, &rmsLMeter, &rmsRMeter, &resetBtn })
            addAndMakeVisible (*c);

        for (auto* m : { &loudM, &loudS, &rmsLMeter, &rmsRMeter })
            m->setFallRate (22.f);
        resetBtn.setButtonText ("RESET");
        resetBtn.onClick = [this] { proc.loudness.requestReset(); };

        // --- EQ ---
        hpfKnob = addKnob (ids::hpf, "Freq");
        const char* suffix[3] = { "Freq", "Gain", "Q" };
        for (int b = 0; b < 4; ++b)
        {
            bandKnobs[b][0] = addKnob (ids::bandFreq[b], suffix[0]);
            bandKnobs[b][1] = addKnob (ids::bandGain[b], suffix[1], true);
            bandKnobs[b][2] = addKnob (ids::bandQ[b],    suffix[2]);
        }
        // --- Compresor ---
        const std::pair<const char*, const char*> cp[7] = {
            { ids::cThresh, "Thresh" }, { ids::cRatio, "Ratio" }, { ids::cKnee, "Knee" },
            { ids::cAttack, "Attack" }, { ids::cRelease, "Release" }, { ids::cMakeup, "Makeup" }, { ids::cMix, "Mix" } };
        for (int i = 0; i < 7; ++i) compKnobs[i] = addKnob (cp[i].first, cp[i].second);
        // --- Limitador ---
        limKnobs[0] = addKnob (ids::lGain,    "Gain");
        limKnobs[1] = addKnob (ids::lCeil,    "Ceiling");
        limKnobs[2] = addKnob (ids::lRelease, "Release");

        setSize (W, H);
        startTimerHz (30);
    }

    ~MainPanel() override { stopTimer(); setLookAndFeel (nullptr); }

    //--------------------------------------------------------------------------
    void resized() override
    {
        eqPanel = { 14.f, 70.f, 1072.f, 300.f };
        compPanel = { 14.f, 380.f, 531.f, 286.f };
        limPanel = { 555.f, 380.f, 531.f, 286.f };

        // ---- EQ ----
        eqBtn.setBounds (28, 74, 220, 26);
        eqDisplay.setBounds (26, 102, 1006, 150);
        inMeter.setBounds (1040, 116, 34, 136);

        const int kw = 74;
        const float gap = (1048.f - 13.f * (float) kw) / 4.f;
        float x = 26.f;
        hpfKnob->setBounds ((int) x, 268, kw, 90);
        groupRects[0] = { x, 255.f, (float) kw, 13.f };
        x += (float) kw + gap;
        for (int b = 0; b < 4; ++b)
        {
            groupRects[b + 1] = { x, 255.f, 3.f * (float) kw, 13.f };
            for (int k = 0; k < 3; ++k)
                bandKnobs[b][k]->setBounds ((int) x + k * kw, 268, kw, 90);
            x += 3.f * (float) kw + gap;
        }

        // ---- Compresor ----
        compBtn.setBounds (28, 384, 220, 26);
        compCurve.setBounds (26, 416, 132, 132);
        compHistory.setBounds (166, 416, 327, 132);
        compGrMeter.setBounds (503, 430, 30, 118);
        const float cw = 507.f / 7.f;
        for (int i = 0; i < 7; ++i)
            compKnobs[i]->setBounds ((int) (26.f + (float) i * cw), 554, (int) cw, 102);

        // ---- Limitador ----
        limBtn.setBounds (569, 384, 220, 26);
        limHistory.setBounds (567, 416, 421, 132);
        limGrMeter.setBounds (996, 430, 30, 118);
        outMeter.setBounds (1034, 430, 40, 118);
        for (int i = 0; i < 3; ++i)
            limKnobs[i]->setBounds (577 + i * 96, 554, 96, 102);

        // ---- Columna de sonoridad (derecha) ----
        sidePanel = { 1100.f, 70.f, 176.f, 596.f };
        const int bx = 1134, bw = 27, bstep = 33, by = 120, bh = 330;
        loudM.setBounds (bx, by, bw, bh);
        loudS.setBounds (bx + bstep, by, bw, bh);
        rmsLMeter.setBounds (bx + 2 * bstep + 6, by, bw, bh);
        rmsRMeter.setBounds (bx + 3 * bstep + 6, by, bw, bh);
        resetBtn.setBounds (1114, 634, 148, 24);
    }

    //--------------------------------------------------------------------------
    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff131316), 0, 0, col::bg, 0, (float) H, false));
        g.fillAll();
        g.setTiledImageFill (texture, 0, 0, 0.05f);
        g.fillAll();

        paintHeader (g);
        gfx::drawMetalPanel (g, eqPanel, texture);
        gfx::drawMetalPanel (g, compPanel, texture);
        gfx::drawMetalPanel (g, limPanel, texture);
        gfx::drawMetalPanel (g, sidePanel, texture);
        paintLoudness (g);

        // títulos de grupo del EQ
        const char* names[5] = { "HIGH-PASS", "LOW SHELF", "LOW-MID BELL", "HIGH-MID BELL", "HIGH SHELF" };
        g.setFont (gfx::uiFont (10.f));
        for (int i = 0; i < 5; ++i)
        {
            const auto r = groupRects[i];
            g.setColour (col::dim);
            g.drawText (names[i], r, Justification::centred, false);
            const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), names[i]) * 0.5f + 8.f;
            g.setColour (Colour (0x40ffffff));
            g.drawHorizontalLine ((int) r.getCentreY(), r.getX() + 2.f, r.getCentreX() - tw);
            g.drawHorizontalLine ((int) r.getCentreY(), r.getCentreX() + tw, r.getRight() - 2.f);
        }

        // etiquetas de medidores
        g.setFont (gfx::uiFont (10.f));
        g.setColour (col::steelHi);
        g.drawText ("IN",  1040, 102, 34, 12, Justification::centred, false);
        g.drawText ("GR",  503, 416, 30, 12, Justification::centred, false);
        g.drawText ("GR",  996, 416, 30, 12, Justification::centred, false);
        g.drawText ("OUT", 1034, 416, 40, 12, Justification::centred, false);

        // texto informativo del limitador
        g.setColour (col::dim);
        g.setFont (gfx::uiFont (11.f));
        const auto info = Rectangle<float> (872.f, 560.f, 200.f, 90.f);
        g.drawFittedText ("BRICKWALL\nLOOKAHEAD  5 ms\nLATENCY  " + String (proc.getLatencySamples()) + " smp\nSTEREO LINKED",
                          info.toNearestInt(), Justification::topRight, 5);
    }

private:
    void paintLoudness (Graphics& g)
    {
        const int bx = 1134, bw = 27, bstep = 33, by = 120, bh = 330;
        g.setFont (gfx::titleFont (17.f));
        g.setColour (col::bone);
        g.drawText ("OUTPUT", 1118, 76, 150, 24, Justification::centredLeft, false);

        g.setFont (gfx::uiFont (10.f));
        g.setColour (col::steelHi);
        g.drawText ("LUFS", bx, 104, bw + bstep, 12, Justification::centred, false);
        g.drawText ("RMS",  bx + 2 * bstep + 6, 104, bw + bstep, 12, Justification::centred, false);
        g.setColour (col::dim);
        const char* letters[4] = { "M", "S", "L", "R" };
        for (int i = 0; i < 4; ++i)
            g.drawText (letters[i], bx + i * bstep + (i >= 2 ? 6 : 0), by + bh + 2, bw, 12, Justification::centred, false);

        g.setFont (gfx::uiFont (9.f, false));                       // escala en dB
        for (int db = 0; db >= -54; db -= 6)
        {
            const float y = (float) by + 3.f + (float) (bh - 6) * (float) (-db) / 54.f;
            g.drawText (String (db), 1103, (int) y - 6, 28, 12, Justification::centredRight, false);
        }

        auto fmt = [] (float v) { return v <= -100.f ? String ("-inf") : String (v, 1); };
        struct Row { const char* label; float value; int y; bool hi; };
        const Row rows[3] = { { "MOMENTARY", mLufs, 474, false }, { "SHORT-TERM", sLufs, 512, false }, { "INTEGRATED", iLufs, 550, true } };
        for (auto& r : rows)
        {
            g.setFont (gfx::uiFont (10.f));
            g.setColour (col::dim);
            g.drawText (r.label, 1114, r.y, 148, 12, Justification::centredLeft, false);
            g.setFont (gfx::titleFont (22.f));
            g.setColour (r.hi ? col::glow : col::bone);
            g.drawText (fmt (r.value), 1114, r.y + 12, 100, 24, Justification::centredRight, false);
            g.setFont (gfx::uiFont (10.f));
            g.setColour (col::dim);
            g.drawText ("LUFS", 1220, r.y + 18, 44, 14, Justification::centredLeft, false);
        }
        g.setFont (gfx::uiFont (11.f));
        g.setColour (col::text);
        g.drawText ("PEAK  " + fmt (peakDbv) + " dBFS", 1114, 594, 148, 14, Justification::centredLeft, false);
        g.drawText ("RMS  " + fmt (rmsLv) + " / " + fmt (rmsRv), 1114, 609, 148, 14, Justification::centredLeft, false);
    }

    void paintHeader (Graphics& g)
    {
        const Rectangle<float> r (0.f, 0.f, (float) W, 62.f);
        g.setGradientFill (ColourGradient (Colour (0xff222327), 0, 0, Colour (0xff0b0b0d), 0, r.getBottom(), false));
        g.fillRect (r);
        g.setTiledImageFill (texture, 0, 0, 0.10f);
        g.fillRect (r);

        // dientes de sierra / cuchilla en el borde inferior
        const float y0 = 52.f, step = 18.f;
        Path teeth;
        teeth.startNewSubPath (0.f, y0 - 3.f);
        for (float x = 0.f; x < (float) W; x += step)
        {
            teeth.lineTo (x + step * 0.5f, y0 + 10.f);
            teeth.lineTo (x + step, y0 - 3.f);
        }
        teeth.lineTo ((float) W, 0.f);
        teeth.lineTo (0.f, 0.f);
        teeth.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillPath (teeth, AffineTransform::translation (0.f, 2.f));
        g.setGradientFill (ColourGradient (Colour (0xff34353a), 0, 0, Colour (0xff0f0f11), 0, y0 + 10.f, false));
        g.fillPath (teeth);
        g.setColour (col::blood);
        g.fillRect (0.f, y0 - 4.f, (float) W, 1.5f);

        // logotipo
        const Rectangle<int> tb (28, 4, 420, 50);
        g.setFont (gfx::titleFont (44.f));
        g.setColour (col::blood.withAlpha (0.28f));
        for (auto o : { Point<int> (-2, 0), Point<int> (2, 0), Point<int> (0, -2), Point<int> (0, 2) })
            g.drawText ("IRONHITS", tb.translated (o.x, o.y), Justification::centredLeft, false);
        g.setColour (Colours::black);
        g.drawText ("IRONHITS", tb.translated (2, 3), Justification::centredLeft, false);
        g.setGradientFill (ColourGradient (Colour (0xfff4f5f7), 0, 8, Colour (0xff6e717a), 0, 52, false));
        g.drawText ("IRONHITS", tb, Justification::centredLeft, false);

        g.setFont (gfx::uiFont (12.f));
        g.setColour (col::dim);
        g.drawText ("PARAMETRIC EQ   //   COMPRESSOR   //   BRICKWALL LIMITER", Rectangle<int> (500, 14, 776, 30), Justification::centredRight, false);
    }

    Knob* addKnob (const char* id, const String& name, bool bipolar = false)
    {
        knobs.push_back (std::make_unique<Knob> (proc.apvts, id, name, bipolar));
        addAndMakeVisible (*knobs.back());
        return knobs.back().get();
    }

    void timerCallback() override
    {
        constexpr float dt = 1.f / 30.f;
        const float inL = proc.inPeak[0].exchange (0.f),  inR = proc.inPeak[1].exchange (0.f);
        const float oL  = proc.outPeak[0].exchange (0.f), oR  = proc.outPeak[1].exchange (0.f);
        const float cIn = proc.compInPeak.exchange (0.f);
        const float cGr = proc.compGrDb.exchange (0.f);
        const float lGr = proc.limGrDb.exchange (0.f);

        inMeter.update (inL, inR, dt);
        outMeter.update (oL, oR, dt);
        compGrMeter.update (cGr, 0.f, dt);
        limGrMeter.update (lGr, 0.f, dt);

        const float cInDb = Decibels::gainToDecibels (cIn, -100.f);
        compCurve.setInputDb (cInDb);
        compHistory.push (cInDb, cGr);
        limHistory.push (Decibels::gainToDecibels (jmax (oL, oR), -100.f), lGr);
        eqDisplay.tick();

        auto& ld = proc.loudness;
        mLufs = ld.momentary.load();  sLufs = ld.shortTerm.load();  iLufs = ld.integrated.load();
        peakDbv = ld.peakDb.load();  rmsLv = ld.rmsDb[0].load();  rmsRv = ld.rmsDb[1].load();
        loudM.updateDb (mLufs, 0.f, dt);      loudS.updateDb (sLufs, 0.f, dt);
        rmsLMeter.updateDb (rmsLv, 0.f, dt);  rmsRMeter.updateDb (rmsRv, 0.f, dt);
        repaint (1100, 470, 176, 160);
    }

    IronHitsProcessor& proc;
    MetalLookAndFeel laf;
    Image texture;

    ToggleButton eqBtn { "EQUALIZER" }, compBtn { "COMPRESSOR" }, limBtn { "LIMITER" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> eqAtt, compAtt, limAtt;

    EqDisplay eqDisplay;
    CompCurve compCurve;
    HistoryDisplay compHistory, limHistory;
    MeterBar inMeter, outMeter, compGrMeter, limGrMeter;
    MeterBar loudM, loudS, rmsLMeter, rmsRMeter;
    TextButton resetBtn;
    float mLufs = -120.f, sLufs = -120.f, iLufs = -120.f, peakDbv = -120.f, rmsLv = -120.f, rmsRv = -120.f;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* hpfKnob = nullptr;
    Knob* bandKnobs[4][3] {};
    Knob* compKnobs[7] {};
    Knob* limKnobs[3] {};

    Rectangle<float> eqPanel, compPanel, limPanel, sidePanel, groupRects[5];
};

//==============================================================================
IronHitsEditor::IronHitsEditor (IronHitsProcessor& p)
    : AudioProcessorEditor (&p), panel (std::make_unique<MainPanel> (p))
{
    addAndMakeVisible (*panel);
    setResizable (true, true);
    setResizeLimits (903, 476, 1935, 1020);
    getConstrainer()->setFixedAspectRatio ((double) MainPanel::W / (double) MainPanel::H);
    setSize (MainPanel::W, MainPanel::H);
}

IronHitsEditor::~IronHitsEditor() = default;

void IronHitsEditor::resized()
{
    panel->setTransform (AffineTransform::scale ((float) getWidth() / (float) MainPanel::W));
    panel->setBounds (0, 0, MainPanel::W, MainPanel::H);
}
