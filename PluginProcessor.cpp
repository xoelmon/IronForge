#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
//  Parámetros
//==============================================================================
static String fmtDb (float v, int)    { return String (v, 1) + " dB"; }
static String fmtMs (float v, int)    { return v < 10.f ? String (v, 2) + " ms" : v < 100.f ? String (v, 1) + " ms" : String ((int) std::round (v)) + " ms"; }
static String fmtRatio (float v, int) { return (v >= 19.9f ? String ("INF") : String (v, 1)) + ":1"; }
static String fmtQ (float v, int)     { return String (v, 2); }
static String fmtPct (float v, int)   { return String ((int) std::round (v)) + " %"; }
static String fmtHz (float v, int)    { return v >= 1000.f ? String (v / 1000.f, v >= 10000.f ? 1 : 2) + " kHz" : String (v, v < 100.f ? 1 : 0) + " Hz"; }
static String fmtHpf (float v, int)   { return v <= 20.5f ? String ("OFF") : fmtHz (v, 0); }
static String fmtLpf (float v, int)   { return v >= 19900.f ? String ("OFF") : fmtHz (v, 0); }

static float parseNumber (const String& t)
{
    return t.retainCharacters ("0123456789.-").getFloatValue();
}
static float parseHz (const String& t)
{
    const float v = parseNumber (t);
    return t.containsIgnoreCase ("k") ? v * 1000.f : v;
}
static float parseRatio (const String& t)
{
    return t.containsIgnoreCase ("inf") ? 20.f : parseNumber (t);
}

static NormalisableRange<float> rangeWithCentre (float lo, float hi, float centre, float step = 0.f)
{
    NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

AudioProcessorValueTreeState::ParameterLayout LargartixaProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const char* id, const String& name, NormalisableRange<float> range, float def,
                         std::function<String (float, int)> toText, std::function<float (const String&)> fromText)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText))
                                                        .withValueFromStringFunction (std::move (fromText))));
    };
    auto addBool = [&] (const char* id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // ---------------- EQ ----------------
    addBool (ids::eqOn, "EQ On", true);
    addFloat (ids::hpf, "HPF", rangeWithCentre (20.f, 500.f, 90.f), 20.f, fmtHpf, parseHz);

    const char* bandNames[4] = { "Low Shelf", "Low-Mid", "High-Mid", "High Shelf" };
    const float fLo[4]  = { 20.f,   40.f,   200.f,  1000.f };
    const float fHi[4]  = { 1000.f, 5000.f, 16000.f, 20000.f };
    const float fCen[4] = { 150.f,  500.f,  2500.f, 6000.f };
    const float fDef[4] = { 100.f,  400.f,  2500.f, 8000.f };
    const float qLo[4]  = { 0.3f,   0.2f,   0.2f,   0.3f };
    const float qHi[4]  = { 2.0f,  10.0f,  10.0f,   2.0f };
    const float qDef[4] = { 0.71f,  1.0f,   1.0f,   0.71f };

    for (int i = 0; i < 4; ++i)
    {
        const String n (bandNames[i]);
        addFloat (ids::bandFreq[i], n + " Freq", rangeWithCentre (fLo[i], fHi[i], fCen[i]), fDef[i], fmtHz, parseHz);
        addFloat (ids::bandGain[i], n + " Gain", NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.f, fmtDb, parseNumber);
        addFloat (ids::bandQ[i],    n + " Q",    rangeWithCentre (qLo[i], qHi[i], 1.0f), qDef[i], fmtQ, parseNumber);

        // Sección dinámica: Range negativo = reduce al superar el threshold (de-esser, anti-resonancia), positivo = realza
        addBool (ids::bandDyn[i], n + " Dynamic", false);
        addFloat (ids::bandThr[i],   n + " Dyn Threshold", NormalisableRange<float> (-60.f, 0.f, 0.1f), -30.f, fmtDb, parseNumber);
        addFloat (ids::bandRange[i], n + " Dyn Range",     NormalisableRange<float> (-18.f, 18.f, 0.1f), -6.f, fmtDb, parseNumber);
        addFloat (ids::bandAtk[i],   n + " Dyn Attack",    rangeWithCentre (0.1f, 200.f, 10.f), 10.f, fmtMs, parseNumber);
        addFloat (ids::bandRel[i],   n + " Dyn Release",   rangeWithCentre (10.f, 1000.f, 120.f), 120.f, fmtMs, parseNumber);
    }

    // ---------------- Compresor ----------------
    addBool (ids::cOn, "Comp On", true);
    addFloat (ids::cThresh,  "Comp Threshold", NormalisableRange<float> (-60.f, 0.f, 0.1f), -18.f, fmtDb, parseNumber);
    addFloat (ids::cRatio,   "Comp Ratio",     rangeWithCentre (1.f, 20.f, 4.f, 0.1f), 4.f, fmtRatio, parseRatio);
    addFloat (ids::cAttack,  "Comp Attack",    rangeWithCentre (0.1f, 100.f, 10.f), 10.f, fmtMs, parseNumber);
    addFloat (ids::cRelease, "Comp Release",   rangeWithCentre (10.f, 1000.f, 120.f), 120.f, fmtMs, parseNumber);
    addFloat (ids::cKnee,    "Comp Knee",      NormalisableRange<float> (0.f, 24.f, 0.1f), 6.f, fmtDb, parseNumber);
    addFloat (ids::cMakeup,  "Comp Makeup",    NormalisableRange<float> (0.f, 24.f, 0.1f), 0.f, fmtDb, parseNumber);
    addFloat (ids::cMix,     "Comp Mix",       NormalisableRange<float> (0.f, 100.f, 1.f), 100.f, fmtPct, parseNumber);
    addFloat (ids::cScHp, "Comp Sidechain HPF", rangeWithCentre (20.f, 1000.f, 120.f), 20.f, fmtHpf, parseHz);
    addFloat (ids::cScLp, "Comp Sidechain LPF", rangeWithCentre (1000.f, 20000.f, 6000.f), 20000.f, fmtLpf, parseHz);

    // ---------------- Clipper ----------------
    addBool (ids::clipOn, "Clipper On", true);
    addFloat (ids::clipDrive, "Clipper Drive",   NormalisableRange<float> (0.f, 24.f, 0.1f), 0.f, fmtDb, parseNumber);
    addFloat (ids::clipCeil,  "Clipper Ceiling", NormalisableRange<float> (-12.f, 0.f, 0.1f), -2.f, fmtDb, parseNumber);
    addFloat (ids::clipSoft,  "Clipper Soft",    NormalisableRange<float> (0.f, 100.f, 1.f), 50.f, fmtPct, parseNumber);
    addFloat (ids::clipMix,   "Clipper Mix",     NormalisableRange<float> (0.f, 100.f, 1.f), 100.f, fmtPct, parseNumber);

    // ---------------- Limitador 1 ----------------
    addBool (ids::lOn, "Limiter 1 On", true);
    addFloat (ids::lGain,    "Limiter 1 Gain",    NormalisableRange<float> (0.f, 24.f, 0.1f), 0.f, fmtDb, parseNumber);
    addFloat (ids::lCeil,    "Limiter 1 Ceiling", NormalisableRange<float> (-12.f, 0.f, 0.1f), -1.0f, fmtDb, parseNumber);
    addFloat (ids::lRelease, "Limiter 1 Release", rangeWithCentre (10.f, 1000.f, 100.f), 100.f, fmtMs, parseNumber);

    // ---------------- Limitador 2 (true-peak) ----------------
    addBool (ids::l2On, "Limiter 2 On", true);
    addFloat (ids::l2Gain,    "Limiter 2 Gain",    NormalisableRange<float> (0.f, 24.f, 0.1f), 0.f, fmtDb, parseNumber);
    addFloat (ids::l2Ceil,    "Limiter 2 Ceiling", NormalisableRange<float> (-12.f, 0.f, 0.1f), -1.0f, fmtDb, parseNumber);
    addFloat (ids::l2Release, "Limiter 2 Release", rangeWithCentre (10.f, 1000.f, 50.f), 50.f, fmtMs, parseNumber);
    addBool (ids::l2TruePeak, "Limiter 2 True Peak", true);

    return layout;
}

//==============================================================================
LargartixaProcessor::LargartixaProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LARGARTIXA", createParameterLayout())
{
}

bool LargartixaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void LargartixaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    eq.prepare (sampleRate, samplesPerBlock);
    comp.prepare (sampleRate);
    clipper.prepare (sampleRate, jmax (1, samplesPerBlock));
    limiter1.prepare (sampleRate, false);
    limiter2.prepare (sampleRate, true);
    loudness.prepare (sampleRate);
    // El DAW compensa la latencia total: oversampling del clipper + look-ahead de los dos limitadores
    setLatencySamples (clipper.getLatencySamples() + limiter1.getLatencySamples() + limiter2.getLatencySamples());
    scopeFifo.reset();
}

void LargartixaProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    storeMax (inPeak[0], buffer.getMagnitude (0, 0, n));
    storeMax (inPeak[1], buffer.getMagnitude (1, 0, n));

    // ---- 1) EQ ----
    ifg::EqParams ep;
    ep.on = on (ids::eqOn);
    ep.hpfFreq = get (ids::hpf);
    for (size_t i = 0; i < 4; ++i)
        ep.band[i] = { get (ids::bandFreq[i]), get (ids::bandGain[i]), get (ids::bandQ[i]),
                       on (ids::bandDyn[i]), get (ids::bandThr[i]), get (ids::bandRange[i]), get (ids::bandAtk[i]), get (ids::bandRel[i]) };
    eq.update (ep);
    eq.process (buffer);
    for (int i = 0; i < 4; ++i)
        eqDynOffset[i].store (eq.getDynOffsetDb (i));

    storeMax (compInPeak, jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)));

    // ---- 2) Compresor ----
    ifg::CompParams cp;
    cp.on = on (ids::cOn);
    cp.thresholdDb = get (ids::cThresh);   cp.ratio    = get (ids::cRatio);
    cp.attackMs    = get (ids::cAttack);   cp.releaseMs = get (ids::cRelease);
    cp.kneeDb      = get (ids::cKnee);     cp.makeupDb  = get (ids::cMakeup);
    cp.mix         = get (ids::cMix) * 0.01f;
    cp.scHpHz      = get (ids::cScHp);     cp.scLpHz    = get (ids::cScLp);
    comp.update (cp);
    storeMax (compGrDb, comp.process (buffer));

    // ---- 3) Clipper (oversampling x4) ----
    storeMax (clipInPeak, jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)));
    ifg::ClipParams kp;
    kp.on = on (ids::clipOn);
    kp.driveDb = get (ids::clipDrive);  kp.ceilingDb = get (ids::clipCeil);
    kp.soft = get (ids::clipSoft) * 0.01f;  kp.mix = get (ids::clipMix) * 0.01f;
    clipper.update (kp);
    storeMax (clipAmountDb, clipper.process (buffer));

    // ---- 4) Limitador 1 ----
    storeMax (lim1InPeak, jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)));
    ifg::LimiterParams l1;
    l1.on = on (ids::lOn);
    l1.driveDb = get (ids::lGain);  l1.ceilingDb = get (ids::lCeil);  l1.releaseMs = get (ids::lRelease);
    limiter1.update (l1);
    storeMax (lim1GrDb, limiter1.process (buffer));

    // ---- 5) Limitador 2 (con detección true-peak opcional) ----
    storeMax (lim2InPeak, jmax (buffer.getMagnitude (0, 0, n), buffer.getMagnitude (1, 0, n)));
    ifg::LimiterParams l2;
    l2.on = on (ids::l2On);
    l2.driveDb = get (ids::l2Gain);  l2.ceilingDb = get (ids::l2Ceil);  l2.releaseMs = get (ids::l2Release);
    l2.truePeak = on (ids::l2TruePeak);
    limiter2.update (l2);
    storeMax (lim2GrDb, limiter2.process (buffer));

    loudness.process (buffer);

    storeMax (outPeak[0], buffer.getMagnitude (0, 0, n));
    storeMax (outPeak[1], buffer.getMagnitude (1, 0, n));

    // ---- Datos para el analizador de espectro ----
    const int toWrite = jmin (n, scopeFifo.getFreeSpace());
    if (toWrite > 0)
    {
        int s1, z1, s2, z2;
        scopeFifo.prepareToWrite (toWrite, s1, z1, s2, z2);
        const auto* l = buffer.getReadPointer (0);
        const auto* r = buffer.getReadPointer (1);
        for (int i = 0; i < z1; ++i) scopeData[(size_t) (s1 + i)] = 0.5f * (l[i] + r[i]);
        for (int i = 0; i < z2; ++i) scopeData[(size_t) (s2 + i)] = 0.5f * (l[z1 + i] + r[z1 + i]);
        scopeFifo.finishedWrite (z1 + z2);
    }
}

//==============================================================================
AudioProcessorEditor* LargartixaProcessor::createEditor() { return new LargartixaEditor (*this); }

void LargartixaProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LargartixaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LargartixaProcessor(); }
