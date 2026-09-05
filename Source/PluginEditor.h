#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhysicalDrumEngineAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PhysicalDrumEngineAudioProcessorEditor(PhysicalDrumEngineAudioProcessor&);
    ~PhysicalDrumEngineAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PhysicalDrumEngineAudioProcessor& processor;

    juce::Label title;
    juce::Label subtitle;
    std::array<juce::TextButton, PhysicalDrumEngineAudioProcessor::numPads> padButtons;
    std::array<juce::Label, PhysicalDrumEngineAudioProcessor::numPads> padLabels;
    std::array<juce::Slider, 11> knobs;
    std::array<juce::Label, 11> knobLabels;

    static constexpr std::array<const char*, 11> knobIds = {
        "physicality", "transient", "attack", "brightness", "pitch", "body",
        "decay", "timing", "variation", "output", "mix"
    };

    static constexpr std::array<const char*, 11> knobNames = {
        "PHYSICALITY", "TRANSIENT", "ATTACK", "BRIGHTNESS", "PITCH", "BODY",
        "DECAY", "TIMING", "VARIATION", "OUTPUT", "MIX"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalDrumEngineAudioProcessorEditor)
};
