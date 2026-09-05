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
    juce::Label title, subtitle;
    std::array<std::unique_ptr<juce::TextButton>, PhysicalDrumEngineAudioProcessor::numPads> padButtons;
    std::array<std::unique_ptr<juce::Label>, PhysicalDrumEngineAudioProcessor::numPads> padLabels;
    std::array<std::unique_ptr<juce::Slider>, 11> knobs;
    std::array<std::unique_ptr<juce::Label>, 11> knobLabels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 11> attachments;
    const char* knobIds[11] = {"physicality","transient","attack","brightness","pitch","body","decay","timing","variation","output","mix"};
    const char* knobNames[11] = {"PHYSICALITY","TRANSIENT","ATTACK","BRIGHTNESS","PITCH","BODY","DECAY","TIMING","VARIATION","OUTPUT","MIX"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalDrumEngineAudioProcessorEditor)
};
