#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

namespace ifg
{
//==============================================================================
//  EQ: filtros biquad (fórmulas RBJ "Audio EQ Cookbook"), fase mínima, sin latencia
//==============================================================================
struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

enum class FilterType { LowShelf, Peak, HighShelf, HighPass };

inline BiquadCoeffs makeBiquad (FilterType type, double sr, double freq, double gainDb, double q)
{
    freq = juce::jlimit (10.0, sr * 0.49, freq);
    q    = juce::jmax (0.05, q);

    const double w0 = juce::MathConstants<double>::twoPi * freq / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double A  = std::pow (10.0, gainDb / 40.0);
    const double alpha = sw / (2.0 * q);
    const double beta  = 2.0 * std::sqrt (A) * alpha;

    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (type)
    {
        case FilterType::Peak:
            b0 = 1 + alpha * A;  b1 = -2 * cw;  b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;  a1 = -2 * cw;  a2 = 1 - alpha / A;
            break;

        case FilterType::LowShelf:
            b0 =      A * ((A + 1) - (A - 1) * cw + beta);
            b1 =  2 * A * ((A - 1) - (A + 1) * cw);
            b2 =      A * ((A + 1) - (A - 1) * cw - beta);
            a0 =           (A + 1) + (A - 1) * cw + beta;
            a1 =     -2 * ((A - 1) + (A + 1) * cw);
            a2 =           (A + 1) + (A - 1) * cw - beta;
            break;

        case FilterType::HighShelf:
            b0 =      A * ((A + 1) + (A - 1) * cw + beta);
            b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            b2 =      A * ((A + 1) + (A - 1) * cw - beta);
            a0 =           (A + 1) - (A - 1) * cw + beta;
            a1 =      2 * ((A - 1) - (A + 1) * cw);
            a2 =           (A + 1) - (A - 1) * cw - beta;
            break;

        case FilterType::HighPass:
            b0 = (1 + cw) * 0.5;  b1 = -(1 + cw);  b2 = (1 + cw) * 0.5;
            a0 = 1 + alpha;       a1 = -2 * cw;    a2 = 1 - alpha;
            break;
    }

    BiquadCoeffs c;
    c.b0 = b0 / a0;  c.b1 = b1 / a0;  c.b2 = b2 / a0;
    c.a1 = a1 / a0;  c.a2 = a2 / a0;
    return c;
}

// Respuesta en magnitud (dB) de un biquad: la usa la interfaz para dibujar la curva del EQ
inline double biquadMagnitudeDb (const BiquadCoeffs& c, double freq, double sr)
{
    const double w = juce::MathConstants<double>::twoPi * freq / sr;
    const auto z1 = std::polar (1.0, -w);
    const auto z2 = std::polar (1.0, -2.0 * w);
    const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return 20.0 * std::log10 (juce::jmax (1.0e-9, std::abs (num) / std::abs (den)));
}

struct Biquad
{
    BiquadCoeffs c;
    double z1[2] { 0, 0 }, z2[2] { 0, 0 };

    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }

    inline float process (int ch, float x) noexcept
    {
        const double in = x;
        const double y  = c.b0 * in + z1[ch];
        z1[ch] = c.b1 * in - c.a1 * y + z2[ch];
        z2[ch] = c.b2 * in - c.a2 * y;
        return (float) y;
    }
};

struct EqBandParams { float freq = 1000.f, gainDb = 0.f, q = 1.f; };

struct EqParams
{
    bool  on = true;
    float hpfFreq = 20.f;                 // <= 20.5 Hz = filtro apagado
    std::array<EqBandParams, 4> band {};
};

class Equalizer
{
public:
    static constexpr FilterType bandType[4] = { FilterType::LowShelf, FilterType::Peak,
                                                FilterType::Peak,     FilterType::HighShelf };
    // El HPF son 2 biquads en cascada (Butterworth de 4º orden = 24 dB/oct)
    static constexpr double hpfQ1 = 0.5412, hpfQ2 = 1.3065;

    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        dry.setSize (2, maxBlock);
        wetSmooth.reset (sr, 0.015);
        wetSmooth.setCurrentAndTargetValue (1.f);
        for (auto& b : bands) b.reset();
        hpf[0].reset(); hpf[1].reset();
    }

    void update (const EqParams& p)
    {
        for (int i = 0; i < 4; ++i)
            bands[(size_t) i].c = makeBiquad (bandType[i], sampleRate, p.band[(size_t) i].freq,
                                              p.band[(size_t) i].gainDb, p.band[(size_t) i].q);

        hpfActive = p.hpfFreq > 20.5f;
        hpf[0].c = makeBiquad (FilterType::HighPass, sampleRate, p.hpfFreq, 0.0, hpfQ1);
        hpf[1].c = makeBiquad (FilterType::HighPass, sampleRate, p.hpfFreq, 0.0, hpfQ2);
        wetSmooth.setTargetValue (p.on ? 1.f : 0.f);
    }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int n = buf.getNumSamples();
        if (! wetSmooth.isSmoothing() && wetSmooth.getTargetValue() <= 0.f)
            return;                                            // bypass total

        if (n > dry.getNumSamples())                           // bloque inusualmente grande (render offline)
            dry.setSize (2, n, false, false, true);

        for (int ch = 0; ch < 2; ++ch)
            dry.copyFrom (ch, 0, buf, ch, 0, n);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                if (hpfActive) { x = hpf[0].process (ch, x); x = hpf[1].process (ch, x); }
                for (auto& b : bands) x = b.process (ch, x);
                d[i] = x;
            }
        }

        // crossfade wet/dry para que el botón ON/OFF no haga "clic"
        auto* l = buf.getWritePointer (0);
        auto* r = buf.getWritePointer (1);
        const auto* dl = dry.getReadPointer (0);
        const auto* dr = dry.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float w = wetSmooth.getNextValue();
            l[i] = dl[i] + (l[i] - dl[i]) * w;
            r[i] = dr[i] + (r[i] - dr[i]) * w;
        }
    }

private:
    double sampleRate = 44100.0;
    std::array<Biquad, 4> bands;
    std::array<Biquad, 2> hpf;
    bool hpfActive = false;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> wetSmooth;
};

//==============================================================================
//  COMPRESOR: feed-forward, detector de picos stereo-linked, rodilla suave,
//  suavizado en el dominio de la ganancia (dB), mezcla paralela (dry/wet)
//==============================================================================
struct CompParams
{
    bool  on = true;
    float thresholdDb = -18.f, ratio = 4.f, attackMs = 10.f, releaseMs = 120.f;
    float kneeDb = 6.f, makeupDb = 0.f, mix = 1.f;
};

class Compressor
{
public:
    // Curva estática (la misma que dibuja la interfaz): dB entrada -> dB salida
    static float staticCurveDb (float inDb, float thr, float ratio, float knee)
    {
        const float over = inDb - thr;
        if (knee < 0.1f)
            return over <= 0.f ? inDb : thr + over / ratio;

        if (2.f * over < -knee)         return inDb;
        if (2.f * std::abs (over) <= knee)
        {
            const float x = over + knee * 0.5f;
            return inDb + (1.f / ratio - 1.f) * x * x / (2.f * knee);
        }
        return thr + over / ratio;
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        grState = 0.f;
        makeupSmooth.reset (sr, 0.03);  makeupSmooth.setCurrentAndTargetValue (1.f);
        wetSmooth.reset (sr, 0.03);     wetSmooth.setCurrentAndTargetValue (1.f);
    }

    void update (const CompParams& p)
    {
        params = p;
        atkCoef = std::exp (-1.f / (juce::jmax (0.01f, p.attackMs)  * 0.001f * (float) sampleRate));
        relCoef = std::exp (-1.f / (juce::jmax (0.01f, p.releaseMs) * 0.001f * (float) sampleRate));
        makeupSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.makeupDb));
        wetSmooth.setTargetValue (p.on ? p.mix : 0.f);
    }

    // Devuelve la máxima reducción de ganancia del bloque, en dB (valor positivo)
    float process (juce::AudioBuffer<float>& buf)
    {
        const int n = buf.getNumSamples();
        auto* l = buf.getWritePointer (0);
        auto* r = buf.getWritePointer (1);
        float maxGr = 0.f;

        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i], inR = r[i];
            const float level   = std::max (std::abs (inL), std::abs (inR));
            const float levelDb = juce::Decibels::gainToDecibels (level, -100.f);
            const float target  = staticCurveDb (levelDb, params.thresholdDb, params.ratio, params.kneeDb) - levelDb;

            const float coef = target < grState ? atkCoef : relCoef;
            grState = target + coef * (grState - target);

            const float makeup = makeupSmooth.getNextValue();
            const float wet    = wetSmooth.getNextValue();
            const float gain   = juce::Decibels::decibelsToGain (grState) * makeup;

            l[i] = inL + (inL * gain - inL) * wet;
            r[i] = inR + (inR * gain - inR) * wet;

            maxGr = std::max (maxGr, -grState);
        }
        return params.on ? maxGr : 0.f;
    }

private:
    double sampleRate = 44100.0;
    CompParams params;
    float atkCoef = 0.f, relCoef = 0.f, grState = 0.f;
    juce::SmoothedValue<float> makeupSmooth, wetSmooth;
};

//==============================================================================
//  CLIPPER con oversampling x4 (evita el aliasing que genera recortar la señal).
//  Forma ajustable: duro (recorte plano) <-> suave (rodilla tipo tanh). Nunca supera el techo (ceiling).
//  El wet y el dry salen alineados en el tiempo, así que la mezcla (mix) es limpia.
//==============================================================================
struct ClipParams
{
    bool  on = true;
    float driveDb = 0.f, ceilingDb = -2.f, soft = 0.5f, mix = 1.f;   // soft 0 = duro, 1 = suave
};

class Clipper
{
public:
    // Función de transferencia estática. knee = 1 -> recorte duro; knee = 0 -> tanh completo.
    static float shape (float x, float ceiling, float knee)
    {
        const float u = std::abs (x) / ceiling;
        float y;
        if (u <= knee)              y = u;
        else if (knee >= 0.9999f)   y = 1.f;
        else                        y = knee + (1.f - knee) * std::tanh ((u - knee) / (1.f - knee));
        return std::copysign (y * ceiling, x);
    }

    void prepare (double sr, int maxBlock)
    {
        maxBlockSize = std::max (1, maxBlock);
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        os->initProcessing ((size_t) maxBlockSize);
        latency = (int) std::lround (os->getLatencyInSamples());

        dryDelay.prepare ({ sr, (juce::uint32) maxBlockSize, 2 });
        dryDelay.setDelay ((float) latency);
        dryDelay.reset();
        dryScratch.setSize (2, maxBlockSize);

        driveSmooth.reset (sr * 4.0, 0.02);  driveSmooth.setCurrentAndTargetValue (1.f);
        wetSmooth.reset (sr, 0.02);          wetSmooth.setCurrentAndTargetValue (1.f);
    }

    int getLatencySamples() const { return latency; }

    void update (const ClipParams& p)
    {
        active = p.on;
        ceilingLin = juce::Decibels::decibelsToGain (p.ceilingDb);
        knee = 1.f - juce::jlimit (0.f, 1.f, p.soft);
        driveSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.driveDb));
        wetSmooth.setTargetValue (p.on ? p.mix : 0.f);
    }

    // Devuelve cuánto ha recortado el pico del bloque, en dB (positivo)
    float process (juce::AudioBuffer<float>& buf)
    {
        const int total = buf.getNumSamples();
        float maxIn = 0.f, maxOut = 0.f;

        for (int start = 0; start < total; start += maxBlockSize)
        {
            const int len = std::min (maxBlockSize, total - start);

            // 1) copia del dry retrasada lo mismo que el oversampler
            for (int c = 0; c < 2; ++c)
            {
                const float* src = buf.getReadPointer (c) + start;
                float* dst = dryScratch.getWritePointer (c);
                for (int i = 0; i < len; ++i)
                {
                    dryDelay.pushSample (c, src[i]);
                    dst[i] = dryDelay.popSample (c);
                }
            }

            const bool needWet = wetSmooth.isSmoothing() || wetSmooth.getTargetValue() > 0.f;
            auto sub = juce::dsp::AudioBlock<float> (buf).getSubBlock ((size_t) start, (size_t) len);

            if (needWet)
            {
                auto up = os->processSamplesUp (sub);
                float* l = up.getChannelPointer (0);
                float* r = up.getChannelPointer (1);
                const size_t m = up.getNumSamples();
                for (size_t i = 0; i < m; ++i)
                {
                    const float dg = driveSmooth.getNextValue();
                    const float a = l[i] * dg, b = r[i] * dg;
                    maxIn = std::max ({ maxIn, std::abs (a), std::abs (b) });
                    l[i] = shape (a, ceilingLin, knee);
                    r[i] = shape (b, ceilingLin, knee);
                    maxOut = std::max ({ maxOut, std::abs (l[i]), std::abs (r[i]) });
                }
                os->processSamplesDown (sub);
            }

            // 2) mezcla wet/dry alineada (el crossfade del ON/OFF también pasa por aquí)
            float* ol = buf.getWritePointer (0) + start;
            float* orr = buf.getWritePointer (1) + start;
            const float* dl = dryScratch.getReadPointer (0);
            const float* dr = dryScratch.getReadPointer (1);
            for (int i = 0; i < len; ++i)
            {
                const float w = wetSmooth.getNextValue();
                if (needWet) { ol[i] = dl[i] + (ol[i] - dl[i]) * w;  orr[i] = dr[i] + (orr[i] - dr[i]) * w; }
                else         { ol[i] = dl[i];                         orr[i] = dr[i]; }
            }
        }
        return (active && maxIn > 1.0e-6f) ? std::max (0.f, 20.f * std::log10 (maxIn / std::max (maxOut, 1.0e-9f))) : 0.f;
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 2048 };
    juce::AudioBuffer<float> dryScratch;
    juce::SmoothedValue<float> driveSmooth, wetSmooth;
    int maxBlockSize = 512, latency = 0;
    float ceilingLin = 0.8f, knee = 0.5f;
    bool active = true;
};

//==============================================================================
//  LIMITADOR brickwall con look-ahead de 5 ms.
//  Mínimo deslizante + media móvil sobre la ganancia => ataque suave y garantía
//  matemática de que la salida nunca supera el techo (ceiling).
//  Opcional (si se prepara con truePeakCapable): detector de picos entre muestras
//  (interpolación x4, estilo ITU-R BS.1770) que añade ~12 muestras de latencia fija.
//  La latencia es constante aunque se active o desactive, para que el DAW la compense siempre igual.
//==============================================================================
struct LimiterParams
{
    bool  on = true;
    float driveDb = 0.f, ceilingDb = -0.3f, releaseMs = 100.f;
    bool  truePeak = false;
};

class Limiter
{
public:
    void prepare (double sr, bool truePeakCapable)
    {
        sampleRate = sr;
        L = std::max (2, (int) std::lround (sr * 0.005));
        H = truePeakCapable ? kTpHalfTaps : 0;
        S = L + H;
        for (auto& d : delay) d.assign ((size_t) S, 0.f);
        qIdx.assign ((size_t) L + 2, 0);
        qVal.assign ((size_t) L + 2, 1.f);
        avg.assign ((size_t) L, 1.f);
        drive.reset (sr, 0.02);
        drive.setCurrentAndTargetValue (1.f);
        if (H > 0) makeTruePeakFilters();
        reset();
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.f);
        std::fill (avg.begin(), avg.end(), 1.f);
        qHead = qCount = 0;  counter = 0;  avgPos = 0;  wPos = 0;
        sum = (double) L;  env = 1.f;  jPrev = 0.f;
    }

    int getLatencySamples() const { return S - 1; }

    void update (const LimiterParams& p)
    {
        active     = p.on;
        tpOn       = p.truePeak && H > 0;
        ceilingLin = p.on ? juce::Decibels::decibelsToGain (p.ceilingDb) : 1000.f;
        drive.setTargetValue (p.on ? juce::Decibels::decibelsToGain (p.driveDb) : 1.f);
        relCoef = std::exp (-1.f / (juce::jmax (1.f, p.releaseMs) * 0.001f * (float) sampleRate));
    }

    // Devuelve la reducción máxima del bloque en dB (positiva)
    float process (juce::AudioBuffer<float>& buf)
    {
        const int n = buf.getNumSamples();
        float* ch[2] = { buf.getWritePointer (0), buf.getWritePointer (1) };
        const int cap = L + 2;
        float minEnv = 1.f;

        for (int i = 0; i < n; ++i)
        {
            const float d  = drive.getNextValue();
            const float xl = ch[0][i] * d, xr = ch[1][i] * d;

            // 0) la muestra nueva entra en la línea de retardo (que también es el historial del detector)
            delay[0][(size_t) wPos] = xl;
            delay[1][(size_t) wPos] = xr;

            // 1) pico a limitar: de la muestra, o el true-peak de su entorno (desfasado H muestras)
            const float peak = H > 0 ? detectNeighbourhoodPeak() : std::max (std::abs (xl), std::abs (xr));
            const float g = peak > ceilingLin ? ceilingLin / peak : 1.f;

            // 2) mínimo deslizante de la ganancia sobre L muestras (cola monótona)
            while (qCount > 0 && qVal[(size_t) ((qHead + qCount - 1) % cap)] >= g)
                --qCount;
            const int tail = (qHead + qCount) % cap;
            qIdx[(size_t) tail] = counter;
            qVal[(size_t) tail] = g;
            ++qCount;
            while (qIdx[(size_t) qHead] <= counter - L)
            {
                qHead = (qHead + 1) % cap;
                --qCount;
            }
            const float m = qVal[(size_t) qHead];
            ++counter;

            // 3) media móvil de L muestras => rampa de ataque suave
            sum += (double) m - (double) avg[(size_t) avgPos];
            avg[(size_t) avgPos] = m;
            avgPos = (avgPos + 1) % L;
            const float b = (float) (sum / (double) L);

            // 4) release: la ganancia baja al instante y sube con la constante de release
            env = (b < env) ? b : b + relCoef * (env - b);
            minEnv = std::min (minEnv, env);

            // 5) salida: la muestra más antigua (retardo S-1) con la ganancia aplicada
            const int rPos = (wPos + 1) % S;
            for (int c = 0; c < 2; ++c)
                ch[c][i] = juce::jlimit (-ceilingLin, ceilingLin, delay[(size_t) c][(size_t) rPos] * env);   // seguro final
            wPos = rPos;
        }
        return active ? -juce::Decibels::gainToDecibels (minEnv, -100.f) : 0.f;
    }

private:
    static constexpr int kTpHalfTaps = 12;           // 24 coeficientes por fase, 3 fases (1/4, 1/2, 3/4)

    void makeTruePeakFilters()
    {
        const double pi = juce::MathConstants<double>::pi;
        for (int ph = 0; ph < 3; ++ph)
        {
            const double frac = 0.25 * (double) (ph + 1);
            std::array<double, 2 * kTpHalfTaps> c {};
            double total = 0.0;
            for (int t = 0; t < 2 * H; ++t)
            {
                const double u = (double) (-H + 1 + t) - frac;                  // distancia al punto interpolado
                const double sinc = std::abs (u) < 1.0e-9 ? 1.0 : std::sin (pi * u) / (pi * u);
                const double x = u / (double) H;
                const double win = 0.42 + 0.5 * std::cos (pi * x) + 0.08 * std::cos (2.0 * pi * x);   // Blackman
                c[(size_t) t] = sinc * win;
                total += c[(size_t) t];
            }
            for (int t = 0; t < 2 * H; ++t)
                tpCoef[(size_t) ph][(size_t) t] = (float) (c[(size_t) t] / total);
        }
    }

    // Pico (incluyendo puntos interpolados entre muestras) en el entorno de la muestra x[n-H]
    float detectNeighbourhoodPeak()
    {
        float j = 0.f;
        float hist[2 * kTpHalfTaps];
        for (int c = 0; c < 2; ++c)
        {
            const float* r = delay[(size_t) c].data();
            j = std::max (j, std::abs (r[(size_t) ((wPos - (H - 1) + S) % S)]));        // x[i+1]
            if (tpOn)
            {
                for (int t = 0; t < 2 * H; ++t)
                    hist[t] = r[(size_t) ((wPos - (2 * H - 1 - t) + S) % S)];          // x[i-H+1 .. i+H]
                for (size_t ph = 0; ph < 3; ++ph)
                {
                    float acc = 0.f;
                    const float* h = tpCoef[ph].data();
                    for (int t = 0; t < 2 * H; ++t) acc += hist[t] * h[t];
                    j = std::max (j, std::abs (acc));
                }
            }
        }
        const float tp = std::max (jPrev, j);                                          // entorno (i-1, i+1]
        jPrev = j;
        return tp;
    }

    double sampleRate = 44100.0;
    int L = 240, H = 0, S = 240;
    bool active = true, tpOn = false;
    float ceilingLin = 1.f, relCoef = 0.f, env = 1.f, jPrev = 0.f;
    std::array<std::vector<float>, 2> delay;
    std::array<std::array<float, 2 * kTpHalfTaps>, 3> tpCoef {};
    std::vector<int64_t> qIdx;
    std::vector<float>   qVal, avg;
    int qHead = 0, qCount = 0, avgPos = 0, wPos = 0;
    int64_t counter = 0;
    double sum = 0.0;
    juce::SmoothedValue<float> drive;
};

//==============================================================================
//  MEDIDOR DE SONORIDAD ITU-R BS.1770-4 / EBU R128 (LUFS) + RMS
//    Momentary (400 ms), Short-term (3 s), Integrated (con puertas -70 LUFS y -10 LU)
//==============================================================================
class LoudnessMeter
{
public:
    std::atomic<float> momentary { -120.f }, shortTerm { -120.f }, integrated { -120.f }, peakDb { -120.f };
    std::atomic<float> rmsDb[2] { -120.f, -120.f };

    void requestReset() { resetFlag.store (true); }      // lo llama la interfaz; lo ejecuta el hilo de audio

    void prepare (double sr)
    {
        len400 = std::max (1, (int) std::lround (0.4 * sr));
        len3s  = std::max (len400 + 1, (int) std::lround (3.0 * sr));
        len300 = std::max (1, (int) std::lround (0.3 * sr));
        hop    = std::max (1, (int) std::lround (0.1 * sr));
        ring.assign ((size_t) len3s, 0.f);
        for (auto& r : rmsRing) r.assign ((size_t) len300, 0.f);

        // Filtro K, etapa 1: shelf agudo (~+4 dB). Coeficientes válidos para cualquier frecuencia de muestreo.
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.c.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.c.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.c.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.c.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.c.a2 = (1.0 - K / Q + K * K) / a0;
        }
        // Etapa 2: paso-alto "RLB" (~38 Hz)
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (juce::MathConstants<double>::pi * f0 / sr);
            const double a0 = 1.0 + K / Q + K * K;
            highpass.c.b0 = 1.0;  highpass.c.b1 = -2.0;  highpass.c.b2 = 1.0;
            highpass.c.a1 = 2.0 * (K * K - 1.0) / a0;
            highpass.c.a2 = (1.0 - K / Q + K * K) / a0;
        }
        shelf.reset();  highpass.reset();

        for (int i = 0; i < kBins; ++i)
            binEnergy[(size_t) i] = std::pow (10.0, (binLoudness (i) + 0.691) / 10.0);

        ringPos = rmsPos = hopCounter = 0;
        samplesSeen = 0;
        sum400 = sum3 = rmsSum[0] = rmsSum[1] = 0.0;
        resetIntegrated();
    }

    void process (const juce::AudioBuffer<float>& buf)
    {
        if (resetFlag.exchange (false))
            resetIntegrated();

        const int n = buf.getNumSamples();
        const float* in[2] = { buf.getReadPointer (0), buf.getReadPointer (1) };

        for (int i = 0; i < n; ++i)
        {
            const float xl = in[0][i], xr = in[1][i];
            peakAbs = std::max ({ peakAbs, std::abs (xl), std::abs (xr) });

            // RMS sin ponderar, ventana de 300 ms
            const float x2[2] = { xl * xl, xr * xr };
            for (int c = 0; c < 2; ++c)
            {
                auto& r = rmsRing[(size_t) c];
                rmsSum[c] += (double) x2[c] - (double) r[(size_t) rmsPos];
                r[(size_t) rmsPos] = x2[c];
            }
            rmsPos = (rmsPos + 1) % len300;

            // LUFS: ponderación K y energía de ambos canales
            const double kl = highpass.process (0, shelf.process (0, xl));
            const double kr = highpass.process (1, shelf.process (1, xr));
            const float z = (float) (kl * kl + kr * kr);
            const int old400 = (ringPos - len400 + len3s) % len3s;
            sum400 += (double) z - (double) ring[(size_t) old400];
            sum3   += (double) z - (double) ring[(size_t) ringPos];
            ring[(size_t) ringPos] = z;
            ringPos = (ringPos + 1) % len3s;

            ++samplesSeen;
            if (++hopCounter >= hop)                        // un bloque de 400 ms cada 100 ms (75 % de solape)
            {
                hopCounter = 0;
                if (samplesSeen >= len400)
                    addBlock (sum400 / (double) len400);
            }
        }

        auto toLufs = [] (double ms) { return (float) std::max (-120.0, -0.691 + 10.0 * std::log10 (std::max (ms, 1.0e-12))); };
        momentary.store (toLufs (sum400 / (double) len400));
        shortTerm.store (toLufs (sum3 / (double) len3s));
        for (int c = 0; c < 2; ++c)
            rmsDb[c].store ((float) std::max (-120.0, 10.0 * std::log10 (std::max (rmsSum[c] / (double) len300, 1.0e-12))));
        peakDb.store (juce::Decibels::gainToDecibels (peakAbs, -120.f));
    }

private:
    static constexpr int kBins = 760;                        // de -70 a +6 LUFS en pasos de 0.1 LU
    static double binLoudness (int i) { return -70.0 + ((double) i + 0.5) * 0.1; }

    void resetIntegrated()
    {
        hist.fill (0);
        samplesSeen = 0;                                     // el integrado solo cuenta bloques posteriores al reset
        integrated.store (-120.f);
        peakAbs = 0.f;
        peakDb.store (-120.f);
    }

    void addBlock (double meanSquare)
    {
        const double loud = -0.691 + 10.0 * std::log10 (std::max (meanSquare, 1.0e-12));
        if (loud < -70.0) return;                            // puerta absoluta
        ++hist[(size_t) juce::jlimit (0, kBins - 1, (int) ((loud + 70.0) * 10.0))];

        double sumAll = 0.0;  uint64_t cntAll = 0;
        for (int i = 0; i < kBins; ++i) { sumAll += (double) hist[(size_t) i] * binEnergy[(size_t) i]; cntAll += hist[(size_t) i]; }
        const double relGate = -0.691 + 10.0 * std::log10 (sumAll / (double) cntAll) - 10.0;   // puerta relativa

        double sumRel = 0.0;  uint64_t cntRel = 0;
        for (int i = 0; i < kBins; ++i)
            if (hist[(size_t) i] > 0 && binLoudness (i) >= relGate)
            {
                sumRel += (double) hist[(size_t) i] * binEnergy[(size_t) i];
                cntRel += hist[(size_t) i];
            }
        integrated.store (cntRel > 0 ? (float) (-0.691 + 10.0 * std::log10 (sumRel / (double) cntRel)) : -120.f);
    }

    Biquad shelf, highpass;
    std::vector<float> ring;
    std::array<std::vector<float>, 2> rmsRing;
    std::array<uint32_t, kBins> hist {};
    std::array<double, kBins> binEnergy {};
    int len400 = 19200, len3s = 144000, len300 = 14400, hop = 4800;
    int ringPos = 0, rmsPos = 0, hopCounter = 0;
    int64_t samplesSeen = 0;
    double sum400 = 0.0, sum3 = 0.0, rmsSum[2] { 0.0, 0.0 };
    float peakAbs = 0.f;
    std::atomic<bool> resetFlag { false };
};

} // namespace ifg
