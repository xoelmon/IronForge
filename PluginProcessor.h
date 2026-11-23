#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"
#include "Params.h"

class LargartixaProcessor : public juce::AudioProcessor
{
public:
    LargartixaProcessor();
    ~LargartixaProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LARGARTIXA SOUNDLAB"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // --- Datos para los medidores (el audio los escribe, la interfaz los lee y los pone a 0) ---
    std::atomic<float> inPeak[2]  { 0.f, 0.f };   // lineal
    std::atomic<float> outPeak[2] { 0.f, 0.f };   // lineal
    std::atomic<float> compInPeak { 0.f };        // lineal
    std::atomic<float> compGrDb   { 0.f };        // dB de reducción (positivo)
    std::atomic<float> limGrDb    { 0.f };        // dB de reducción (positivo)

    // --- Sonoridad de la salida (LUFS / RMS) ---
    ifg::LoudnessMeter loudness;

    // --- Cola de audio para el analizador de espectro del EQ ---
    juce::AbstractFifo scopeFifo { 16384 };
    std::array<float, 16384> scopeData {};

private:
    ifg::Equalizer  eq;
    ifg::Compressor comp;
    ifg::Limiter    limiter;

    static void storeMax (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v)) {}
    }

    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on  (const char* id) const { return get (id) > 0.5f; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LargartixaProcessor)
};
