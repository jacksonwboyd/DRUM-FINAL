#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhysicalDrumEngineAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                     public juce::DragAndDropTarget
{
public:
    explicit PhysicalDrumEngineAudioProcessorEditor(PhysicalDrumEngineAudioProcessor&);
    ~PhysicalDrumEngineAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void itemDragEnter(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
    void itemDragMove(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
    void itemDragExit(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    PhysicalDrumEngineAudioProcessor& processor;

    juce::Label title;
    juce::Label subtitle;
    std::array<juce::TextButton, PhysicalDrumEngineAudioProcessor::numPads> padButtons;
    std::array<juce::Label, PhysicalDrumEngineAudioProcessor::numPads> padLabels;
    std::array<juce::Slider, 11> knobs;
    std::array<juce::Label, 11> knobLabels;
    std::array<juce::Rectangle<int>, PhysicalDrumEngineAudioProcessor::numPads> padDropBounds{};
    int dragPad = -1;

    static constexpr std::array<const char*, 11> knobIds = {
        "physicality", "transient", "attack", "brightness", "pitch", "body",
        "decay", "timing", "variation", "output", "mix"
    };

    static constexpr std::array<const char*, 11> knobNames = {
        "PHYSICALITY", "TRANSIENT", "ATTACK", "BRIGHTNESS", "PITCH", "BODY",
        "DECAY", "TIMING", "VARIATION", "OUTPUT", "MIX"
    };

    int padAtPoint(juce::Point<int> point) const;
    void setDragPad(int newPad);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalDrumEngineAudioProcessorEditor)
};
