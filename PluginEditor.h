#pragma once

#include "PluginProcessor.h"

class MainPanel;

class IronForgeEditor : public juce::AudioProcessorEditor
{
public:
    explicit IronForgeEditor (IronForgeProcessor&);
    ~IronForgeEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    std::unique_ptr<MainPanel> panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IronForgeEditor)
};
