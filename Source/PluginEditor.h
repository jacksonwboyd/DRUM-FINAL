#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhysicalDrumEngineAudioProcessorEditor final
    : public juce::AudioProcessorEditor,
      public juce::FileDragAndDropTarget
{
public:
    explicit PhysicalDrumEngineAudioProcessorEditor(PhysicalDrumEngineAudioProcessor&);
    ~PhysicalDrumEngineAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    PhysicalDrumEngineAudioProcessor& processor;

    juce::Label title;
    juce::Label subtitle;
    std::array<juce::TextButton, PhysicalDrumEngineAudioProcessor::numPads> padButtons;
    std::array<juce::Label, PhysicalDrumEngineAudioProcessor::numPads> padLabels;
    std::array<juce::Slider, 11> knobs;
    std::array<juce::Label, 11> knobLabels;
    std::array<juce::Rectangle<int>, PhysicalDrumEngineAudioProcessor::numPads> padAreas;
    int dragTargetPad = -1;

    static constexpr std::array<const char*, 11> knobIds = {
        "physicality", "transient", "attack", "brightness", "pitch", "body",
        "decay", "timing", "variation", "output", "mix"
    };

    static constexpr std::array<const char*, 11> knobNames = {
        "PHYSICALITY", "TRANSIENT", "ATTACK", "BRIGHTNESS", "PITCH", "BODY",
        "DECAY", "TIMING", "VARIATION", "OUTPUT", "MIX"
    };

    int padAtPosition(int x, int y) const;
    void updateDragTarget(int x, int y);
    void clearDragTarget();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalDrumEngineAudioProcessorEditor)
};
