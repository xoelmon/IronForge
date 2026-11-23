#pragma once

#include "PluginProcessor.h"

class MainPanel;

class IronHitsEditor : public juce::AudioProcessorEditor
{
public:
    explicit IronHitsEditor (IronHitsProcessor&);
    ~IronHitsEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    std::unique_ptr<MainPanel> panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IronHitsEditor)
};
