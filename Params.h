#pragma once

// IDs de todos los parámetros (se usan en el procesador, la interfaz y la automatización del DAW)
namespace ids
{
    inline constexpr const char* eqOn   = "eq_on";
    inline constexpr const char* hpf    = "hpf_freq";

    inline constexpr const char* bandFreq[4] = { "b1_freq", "b2_freq", "b3_freq", "b4_freq" };
    inline constexpr const char* bandGain[4] = { "b1_gain", "b2_gain", "b3_gain", "b4_gain" };
    inline constexpr const char* bandQ[4]    = { "b1_q",    "b2_q",    "b3_q",    "b4_q"    };

    inline constexpr const char* cOn      = "c_on";
    inline constexpr const char* cThresh  = "c_thresh";
    inline constexpr const char* cRatio   = "c_ratio";
    inline constexpr const char* cAttack  = "c_attack";
    inline constexpr const char* cRelease = "c_release";
    inline constexpr const char* cKnee    = "c_knee";
    inline constexpr const char* cMakeup  = "c_makeup";
    inline constexpr const char* cMix     = "c_mix";

    inline constexpr const char* lOn      = "l_on";
    inline constexpr const char* lGain    = "l_gain";
    inline constexpr const char* lCeil    = "l_ceiling";
    inline constexpr const char* lRelease = "l_release";
}
