#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>
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
//  LIMITADOR brickwall con look-ahead de 5 ms.
//  Mínimo deslizante + media móvil sobre la ganancia => ataque suave y garantía
//  matemática de que la salida nunca supera el techo (ceiling).
//  Introduce una latencia fija de (lookahead - 1) muestras que el DAW compensa.
//==============================================================================
struct LimiterParams
{
    bool  on = true;
    float driveDb = 0.f, ceilingDb = -0.3f, releaseMs = 100.f;
};

class Limiter
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        L = std::max (2, (int) std::lround (sr * 0.005));
        for (auto& d : delay) d.assign ((size_t) L, 0.f);
        qIdx.assign ((size_t) L + 2, 0);
        qVal.assign ((size_t) L + 2, 1.f);
        avg.assign ((size_t) L, 1.f);
        drive.reset (sr, 0.02);
        drive.setCurrentAndTargetValue (1.f);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.f);
        std::fill (avg.begin(), avg.end(), 1.f);
        qHead = qCount = 0;  counter = 0;  avgPos = 0;  wPos = 0;
        sum = (double) L;  env = 1.f;
    }

    int getLatencySamples() const { return L - 1; }

    void update (const LimiterParams& p)
    {
        active     = p.on;
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
            const float peak = std::max (std::abs (xl), std::abs (xr));
            const float g = peak > ceilingLin ? ceilingLin / peak : 1.f;

            // 1) mínimo deslizante de la ganancia sobre L muestras (cola monótona)
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

            // 2) media móvil de L muestras => rampa de ataque suave
            sum += (double) m - (double) avg[(size_t) avgPos];
            avg[(size_t) avgPos] = m;
            avgPos = (avgPos + 1) % L;
            const float b = (float) (sum / (double) L);

            // 3) release: la ganancia baja al instante y sube con la constante de release
            env = (b < env) ? b : b + relCoef * (env - b);
            minEnv = std::min (minEnv, env);

            // 4) retraso de (L-1) muestras + aplicar ganancia
            const float in[2] = { xl, xr };
            const int rPos = (wPos + 1) % L;
            for (int c = 0; c < 2; ++c)
            {
                delay[(size_t) c][(size_t) wPos] = in[c];
                const float delayed = delay[(size_t) c][(size_t) rPos];
                ch[c][i] = juce::jlimit (-ceilingLin, ceilingLin, delayed * env);   // seguro final
            }
            wPos = rPos;
        }
        return active ? -juce::Decibels::gainToDecibels (minEnv, -100.f) : 0.f;
    }

private:
    double sampleRate = 44100.0;
    int L = 240;
    bool active = true;
    float ceilingLin = 1.f, relCoef = 0.f, env = 1.f;
    std::array<std::vector<float>, 2> delay;
    std::vector<int64_t> qIdx;
    std::vector<float>   qVal, avg;
    int qHead = 0, qCount = 0, avgPos = 0, wPos = 0;
    int64_t counter = 0;
    double sum = 0.0;
    juce::SmoothedValue<float> drive;
};

} // namespace ifg
