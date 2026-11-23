#pragma once

#include "PluginProcessor.h"

class MainPanel;

class LargartixaEditor : public juce::AudioProcessorEditor
{
public:
    explicit LargartixaEditor (LargartixaProcessor&);
    ~LargartixaEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    std::unique_ptr<MainPanel> panel;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LargartixaEditor)
};
