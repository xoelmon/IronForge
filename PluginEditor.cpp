#include "PluginEditor.h"
#include "UI.h"

using namespace juce;

//==============================================================================
//  Panel principal: tamaño lógico fijo (1290 x 944) que se escala al redimensionar la ventana
//
//    [ EQ                              ]  [ OUTPUT ]
//    [ Compresor ]  [ Clipper          ]  [ LUFS / RMS ]
//    [ Limitador 1 ] [ Limitador 2     ]  [ hasta abajo ]
//==============================================================================
class MainPanel : public Component, private Timer
{
public:
    static constexpr int W = 1290, H = 944;
    static constexpr int dy = H - 680;                 // alto extra que gana la columna de sonoridad

    explicit MainPanel (LargartixaProcessor& p)
        : proc (p), eqDisplay (p), tabs (p.apvts), compCurve (p.apvts), clipCurve (p.apvts),
          compHistory (24.f), clipHistory (12.f), lim1History (12.f), lim2History (12.f),
          inMeter (2, false, 48.f), outMeter (2, false, 48.f),
          compGrMeter (1, true, 24.f), clipMeter (1, true, 12.f), lim1GrMeter (1, true, 12.f), lim2GrMeter (1, true, 12.f),
          loudM (1, false, 54.f), loudS (1, false, 54.f), rmsLMeter (1, false, 54.f), rmsRMeter (1, false, 54.f)
    {
        setLookAndFeel (&laf);
        texture = gfx::makeBrushedTexture();

        using BA = AudioProcessorValueTreeState::ButtonAttachment;
        auto addToggle = [&] (ToggleButton& b, const char* id, std::unique_ptr<BA>& att)
        {
            addAndMakeVisible (b);
            att = std::make_unique<BA> (p.apvts, id, b);
        };
        addToggle (eqBtn,   ids::eqOn,       eqAtt);
        addToggle (compBtn, ids::cOn,        compAtt);
        addToggle (clipBtn, ids::clipOn,     clipAtt);
        addToggle (lim1Btn, ids::lOn,        lim1Att);
        addToggle (lim2Btn, ids::l2On,       lim2Att);
        addToggle (tpBtn,   ids::l2TruePeak, tpAtt);

        for (auto* c : std::initializer_list<Component*> { &eqDisplay, &compCurve, &clipCurve,
                                                           &compHistory, &clipHistory, &lim1History, &lim2History,
                                                           &inMeter, &outMeter, &compGrMeter, &clipMeter, &lim1GrMeter, &lim2GrMeter,
                                                           &loudM, &loudS, &rmsLMeter, &rmsRMeter, &resetBtn })
            addAndMakeVisible (*c);

        for (auto* m : { &loudM, &loudS, &rmsLMeter, &rmsRMeter })
            m->setFallRate (22.f);
        resetBtn.setButtonText ("RESET");
        resetBtn.onClick = [this] { proc.loudness.requestReset(); };

        // --- EQ: HPF + controles de la banda seleccionada (frecuencia/ganancia/Q + sección dinámica) ---
        hpfKnob = addKnob (ids::hpf, "Freq");
        for (int b = 0; b < 4; ++b)
        {
            bandKnobs[b][0] = addKnob (ids::bandFreq[b], "Freq");
            bandKnobs[b][1] = addKnob (ids::bandGain[b], "Gain", true);
            bandKnobs[b][2] = addKnob (ids::bandQ[b],    "Q");
            dynBtn[b].setButtonText ("DYN");
            addChildComponent (dynBtn[b]);
            dynAtt[b] = std::make_unique<BA> (p.apvts, ids::bandDyn[b], dynBtn[b]);
            dynKnobs[b][0] = addKnob (ids::bandThr[b],   "Thresh");
            dynKnobs[b][1] = addKnob (ids::bandRange[b], "Range", true);
            dynKnobs[b][2] = addKnob (ids::bandAtk[b],   "Attack");
            dynKnobs[b][3] = addKnob (ids::bandRel[b],   "Release");
        }
        addAndMakeVisible (tabs);
        tabs.onSelect = [this] (int b) { selectBand (b); };
        eqDisplay.onSelect = [this] (int b) { selectBand (b); };
        // --- Compresor ---
        const std::pair<const char*, const char*> cp[7] = {
            { ids::cThresh, "Thresh" }, { ids::cRatio, "Ratio" }, { ids::cKnee, "Knee" },
            { ids::cAttack, "Attack" }, { ids::cRelease, "Release" }, { ids::cMakeup, "Makeup" }, { ids::cMix, "Mix" } };
        for (int i = 0; i < 7; ++i) compKnobs[i] = addKnob (cp[i].first, cp[i].second);
        scKnobs[0] = addKnob (ids::cScHp, "HPF", false, 56);       // filtros del sidechain del compresor
        scKnobs[1] = addKnob (ids::cScLp, "LPF", false, 56);
        // --- Clipper ---
        clipKnobs[0] = addKnob (ids::clipDrive, "Drive");
        clipKnobs[1] = addKnob (ids::clipCeil,  "Ceiling");
        clipKnobs[2] = addKnob (ids::clipSoft,  "Soft");
        clipKnobs[3] = addKnob (ids::clipMix,   "Mix");
        // --- Limitadores ---
        lim1Knobs[0] = addKnob (ids::lGain,    "Gain");
        lim1Knobs[1] = addKnob (ids::lCeil,    "Ceiling");
        lim1Knobs[2] = addKnob (ids::lRelease, "Release");
        lim2Knobs[0] = addKnob (ids::l2Gain,    "Gain");
        lim2Knobs[1] = addKnob (ids::l2Ceil,    "Ceiling");
        lim2Knobs[2] = addKnob (ids::l2Release, "Release");

        selectBand (1);
        setSize (W, H);
        startTimerHz (30);
    }

    ~MainPanel() override { stopTimer(); setLookAndFeel (nullptr); }

    //--------------------------------------------------------------------------
    void resized() override
    {
        eqPanel   = { 14.f,  70.f,  1072.f, 300.f };
        compPanel = { 14.f,  380.f, 531.f,  286.f };
        clipPanel = { 555.f, 380.f, 531.f,  286.f };
        lim1Panel = { 14.f,  676.f, 531.f,  254.f };
        lim2Panel = { 555.f, 676.f, 531.f,  254.f };
        sidePanel = { 1100.f, 70.f, 176.f, (float) (H - 84) };

        // ---- EQ ----
        eqBtn.setBounds (28, 74, 220, 26);
        eqDisplay.setBounds (26, 102, 1006, 150);
        inMeter.setBounds (1040, 116, 34, 136);

        const int kw = 74;
        const float gap = (1048.f - (float) (kw + 148 + 3 * kw + 80 + 4 * kw)) / 3.f;
        float x = 26.f;
        hpfKnob->setBounds ((int) x, 268, kw, 90);
        groupRects[0] = { x, 255.f, (float) kw, 13.f };
        x += (float) kw + gap;
        tabs.setBounds ((int) x, 292, 148, 40);
        groupRects[1] = { x, 255.f, 148.f, 13.f };
        x += 148.f + gap;
        groupRects[2] = { x, 255.f, 3.f * (float) kw, 13.f };
        for (int b = 0; b < 4; ++b)
            for (int k = 0; k < 3; ++k)
                bandKnobs[b][k]->setBounds ((int) x + k * kw, 268, kw, 90);
        x += 3.f * (float) kw + gap;
        groupRects[3] = { x, 255.f, (float) (80 + 4 * kw), 13.f };
        for (int b = 0; b < 4; ++b)
        {
            dynBtn[b].setBounds ((int) x, 300, 76, 26);
            for (int k = 0; k < 4; ++k)
                dynKnobs[b][k]->setBounds ((int) x + 80 + k * kw, 268, kw, 90);
        }

        // ---- Compresor ----
        compBtn.setBounds (28, 384, 220, 26);
        compCurve.setBounds (26, 416, 132, 132);
        compHistory.setBounds (166, 416, 207, 132);
        scKnobs[0]->setBounds (381, 436, 56, 84);
        scKnobs[1]->setBounds (437, 436, 56, 84);
        compGrMeter.setBounds (503, 430, 30, 118);
        const float cw = 507.f / 7.f;
        for (int i = 0; i < 7; ++i)
            compKnobs[i]->setBounds ((int) (26.f + (float) i * cw), 554, (int) cw, 102);

        // ---- Clipper ----
        clipBtn.setBounds (569, 384, 220, 26);
        clipCurve.setBounds (567, 416, 132, 132);
        clipHistory.setBounds (707, 416, 327, 132);
        clipMeter.setBounds (1044, 430, 30, 118);
        for (int i = 0; i < 4; ++i)
            clipKnobs[i]->setBounds (580 + i * 120, 554, 120, 102);

        // ---- Limitador 1 ----
        lim1Btn.setBounds (28, 680, 220, 26);
        lim1History.setBounds (26, 712, 469, 98);
        lim1GrMeter.setBounds (503, 726, 30, 84);
        for (int i = 0; i < 3; ++i)
            lim1Knobs[i]->setBounds (36 + i * 96, 816, 96, 102);

        // ---- Limitador 2 (true-peak) ----
        lim2Btn.setBounds (569, 680, 220, 26);
        lim2History.setBounds (567, 712, 421, 98);
        lim2GrMeter.setBounds (996, 726, 30, 84);
        outMeter.setBounds (1034, 726, 40, 84);
        for (int i = 0; i < 3; ++i)
            lim2Knobs[i]->setBounds (577 + i * 96, 816, 96, 102);
        tpBtn.setBounds (876, 826, 190, 26);

        // ---- Columna de sonoridad (derecha, hasta abajo) ----
        const int bx = 1134, bw = 27, bstep = 33, by = 120, bh = 330 + dy;
        loudM.setBounds (bx, by, bw, bh);
        loudS.setBounds (bx + bstep, by, bw, bh);
        rmsLMeter.setBounds (bx + 2 * bstep + 6, by, bw, bh);
        rmsRMeter.setBounds (bx + 3 * bstep + 6, by, bw, bh);
        resetBtn.setBounds (1114, 634 + dy, 148, 24);
    }

    //--------------------------------------------------------------------------
    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff131316), 0, 0, col::bg, 0, (float) H, false));
        g.fillAll();
        g.setTiledImageFill (texture, 0, 0, 0.05f);
        g.fillAll();

        paintHeader (g);
        for (auto* r : { &eqPanel, &compPanel, &clipPanel, &lim1Panel, &lim2Panel, &sidePanel })
            gfx::drawMetalPanel (g, *r, texture);
        paintLoudness (g);

        // títulos de grupo del EQ
        const char* bandNames[4] = { "LOW SHELF", "LOW-MID BELL", "HIGH-MID BELL", "HIGH SHELF" };
        const String names[4] = { "HIGH-PASS", "BAND SELECT", "BAND " + String (selBand + 1) + "  -  " + bandNames[selBand], "DYNAMIC EQ" };
        g.setFont (gfx::uiFont (10.f));
        for (int i = 0; i < 4; ++i)
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
        g.drawText ("IN",   1040, 102, 34, 12, Justification::centred, false);
        g.drawText ("GR",    503, 416, 30, 12, Justification::centred, false);
        g.drawText ("CLIP", 1044, 416, 30, 12, Justification::centred, false);
        g.drawText ("GR",    503, 712, 30, 12, Justification::centred, false);
        g.drawText ("GR",    996, 712, 30, 12, Justification::centred, false);
        g.drawText ("OUT",  1034, 712, 40, 12, Justification::centred, false);

        // textos informativos
        g.setColour (col::dim);
        g.setFont (gfx::uiFont (11.f));
        g.drawText ("4x OVERSAMPLING", 800, 384, 264, 26, Justification::centredRight, false);
        g.setColour (col::steelHi);
        g.setFont (gfx::uiFont (10.f));
        g.drawText ("SIDECHAIN", 381, 420, 112, 12, Justification::centred, false);
        g.drawFittedText ("BRICKWALL\nLOOKAHEAD  5 ms\nSTEREO LINKED", Rectangle<int> (331, 826, 202, 60), Justification::topRight, 4);

        const int lat = proc.getLatencySamples();
        const double sr = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
        g.drawFittedText ("LOOKAHEAD  5 ms\nTOTAL LATENCY  " + String (lat) + " smp\n= " + String ((double) lat * 1000.0 / sr, 1) + " ms",
                          Rectangle<int> (863, 862, 207, 60), Justification::topRight, 4);
    }

private:
    void paintLoudness (Graphics& g)
    {
        const int bx = 1134, bw = 27, bstep = 33, by = 120, bh = 330 + dy;
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
        for (int db = 0; db >= -54; db -= 3)
        {
            const float y = (float) by + 3.f + (float) (bh - 6) * (float) (-db) / 54.f;
            if (db % 6 == 0)
                g.drawText (String (db), 1103, (int) y - 6, 28, 12, Justification::centredRight, false);
            else
                g.fillRect (1124.f, y, 5.f, 1.f);
        }

        auto fmt = [] (float v) { return v <= -100.f ? String ("-inf") : String (v, 1); };
        struct Row { const char* label; float value; int y; bool hi; };
        const Row rows[3] = { { "MOMENTARY", mLufs, 474 + dy, false }, { "SHORT-TERM", sLufs, 512 + dy, false }, { "INTEGRATED", iLufs, 550 + dy, true } };
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
        g.drawText ("PEAK  " + fmt (peakDbv) + " dBFS", 1114, 594 + dy, 148, 14, Justification::centredLeft, false);
        g.drawText ("RMS  " + fmt (rmsLv) + " / " + fmt (rmsRv), 1114, 609 + dy, 148, 14, Justification::centredLeft, false);
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
        const Rectangle<int> tb (28, 4, 760, 50);
        g.setFont (gfx::titleFont (40.f));
        g.setColour (col::blood.withAlpha (0.28f));
        for (auto o : { Point<int> (-2, 0), Point<int> (2, 0), Point<int> (0, -2), Point<int> (0, 2) })
            g.drawText ("LARGARTIXA SOUNDLAB", tb.translated (o.x, o.y), Justification::centredLeft, false);
        g.setColour (Colours::black);
        g.drawText ("LARGARTIXA SOUNDLAB", tb.translated (2, 3), Justification::centredLeft, false);
        g.setGradientFill (ColourGradient (Colour (0xfff4f5f7), 0, 8, Colour (0xff6e717a), 0, 52, false));
        g.drawText ("LARGARTIXA SOUNDLAB", tb, Justification::centredLeft, false);

        g.setFont (gfx::uiFont (12.f));
        g.setColour (col::dim);
        g.drawText ("EQ   //   COMPRESSOR   //   CLIPPER   //   DUAL LIMITER", Rectangle<int> (600, 14, 676, 30), Justification::centredRight, false);
    }

    void selectBand (int b)
    {
        selBand = jlimit (0, 3, b);
        for (int i = 0; i < 4; ++i)
        {
            const bool v = (i == selBand);
            for (auto* k : bandKnobs[i]) k->setVisible (v);
            for (auto* k : dynKnobs[i])  k->setVisible (v);
            dynBtn[i].setVisible (v);
        }
        tabs.setSelected (selBand);
        eqDisplay.setSelected (selBand);
        repaint (14, 250, 1072, 20);
    }

    Knob* addKnob (const char* id, const String& name, bool bipolar = false, int textW = 64)
    {
        knobs.push_back (std::make_unique<Knob> (proc.apvts, id, name, bipolar, textW));
        addAndMakeVisible (*knobs.back());
        return knobs.back().get();
    }

    void timerCallback() override
    {
        constexpr float dt = 1.f / 30.f;
        const float inL = proc.inPeak[0].exchange (0.f),  inR = proc.inPeak[1].exchange (0.f);
        const float oL  = proc.outPeak[0].exchange (0.f), oR  = proc.outPeak[1].exchange (0.f);
        const float cIn = proc.compInPeak.exchange (0.f),  cGr = proc.compGrDb.exchange (0.f);
        const float kIn = proc.clipInPeak.exchange (0.f),  kAmt = proc.clipAmountDb.exchange (0.f);
        const float l1In = proc.lim1InPeak.exchange (0.f), l1Gr = proc.lim1GrDb.exchange (0.f);
        const float l2In = proc.lim2InPeak.exchange (0.f), l2Gr = proc.lim2GrDb.exchange (0.f);

        inMeter.update (inL, inR, dt);
        outMeter.update (oL, oR, dt);
        compGrMeter.update (cGr, 0.f, dt);
        clipMeter.update (kAmt, 0.f, dt);
        lim1GrMeter.update (l1Gr, 0.f, dt);
        lim2GrMeter.update (l2Gr, 0.f, dt);

        auto db = [] (float lin) { return Decibels::gainToDecibels (lin, -100.f); };
        compCurve.setInputDb (db (cIn));
        clipCurve.setInputPeak (kIn);
        compHistory.push (db (cIn), cGr);
        clipHistory.push (db (kIn), kAmt);
        lim1History.push (db (l1In), l1Gr);
        lim2History.push (db (l2In), l2Gr);
        eqDisplay.tick();
        tabs.repaint();

        auto& ld = proc.loudness;
        mLufs = ld.momentary.load();  sLufs = ld.shortTerm.load();  iLufs = ld.integrated.load();
        peakDbv = ld.peakDb.load();  rmsLv = ld.rmsDb[0].load();  rmsRv = ld.rmsDb[1].load();
        loudM.updateDb (mLufs, 0.f, dt);      loudS.updateDb (sLufs, 0.f, dt);
        rmsLMeter.updateDb (rmsLv, 0.f, dt);  rmsRMeter.updateDb (rmsRv, 0.f, dt);
        repaint (1100, 470 + dy, 176, 160);
    }

    LargartixaProcessor& proc;
    MetalLookAndFeel laf;
    Image texture;

    ToggleButton eqBtn { "EQUALIZER" }, compBtn { "COMPRESSOR" }, clipBtn { "CLIPPER" },
                 lim1Btn { "LIMITER 1" }, lim2Btn { "LIMITER 2" }, tpBtn { "TRUE PEAK" };
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> eqAtt, compAtt, clipAtt, lim1Att, lim2Att, tpAtt;

    EqDisplay eqDisplay;
    BandTabs tabs;
    CompCurve compCurve;
    ClipCurve clipCurve;
    HistoryDisplay compHistory, clipHistory, lim1History, lim2History;
    MeterBar inMeter, outMeter, compGrMeter, clipMeter, lim1GrMeter, lim2GrMeter;
    MeterBar loudM, loudS, rmsLMeter, rmsRMeter;
    TextButton resetBtn;
    float mLufs = -120.f, sLufs = -120.f, iLufs = -120.f, peakDbv = -120.f, rmsLv = -120.f, rmsRv = -120.f;

    std::vector<std::unique_ptr<Knob>> knobs;
    Knob* hpfKnob = nullptr;
    Knob* bandKnobs[4][3] {};
    Knob* dynKnobs[4][4] {};
    Knob* scKnobs[2] {};
    ToggleButton dynBtn[4];
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> dynAtt[4];
    int selBand = 1;
    Knob* compKnobs[7] {};
    Knob* clipKnobs[4] {};
    Knob* lim1Knobs[3] {};
    Knob* lim2Knobs[3] {};

    Rectangle<float> eqPanel, compPanel, clipPanel, lim1Panel, lim2Panel, sidePanel, groupRects[4];
};

//==============================================================================
LargartixaEditor::LargartixaEditor (LargartixaProcessor& p)
    : AudioProcessorEditor (&p), panel (std::make_unique<MainPanel> (p))
{
    addAndMakeVisible (*panel);
    const double aspect = (double) MainPanel::H / (double) MainPanel::W;
    setResizable (true, true);
    setResizeLimits (903, (int) (903.0 * aspect), 1500, (int) (1500.0 * aspect));
    getConstrainer()->setFixedAspectRatio ((double) MainPanel::W / (double) MainPanel::H);
    setSize ((int) (MainPanel::W * 0.9), (int) (MainPanel::H * 0.9));      // tamaño inicial al 90 %
}

LargartixaEditor::~LargartixaEditor() = default;

void LargartixaEditor::resized()
{
    panel->setTransform (AffineTransform::scale ((float) getWidth() / (float) MainPanel::W));
    panel->setBounds (0, 0, MainPanel::W, MainPanel::H);
}
