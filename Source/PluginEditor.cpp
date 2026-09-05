#include "PluginEditor.h"

PhysicalDrumEngineAudioProcessorEditor::PhysicalDrumEngineAudioProcessorEditor(PhysicalDrumEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // V1.2 deliberately keeps editor construction simple and host-safe.
    // No file I/O, timers, or parameter attachments are created here.
    setSize(960, 620);
    setResizable(false, false);

    title.setText("PHYSICAL DRUM ENGINE", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions{}.withHeight(26.0f).withStyle("Bold")));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    subtitle.setText("V1.2  •  SAFE EDITOR BUILD", juce::dontSendNotification);
    subtitle.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
    subtitle.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitle);

    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
    {
        padLabels[(size_t) i].setText(
            processor.pads[(size_t) i].name + "  •  " + juce::String(processor.pads[(size_t) i].midiNote),
            juce::dontSendNotification);
        padLabels[(size_t) i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(padLabels[(size_t) i]);

        padButtons[(size_t) i].setButtonText("LOAD WAV");
        padButtons[(size_t) i].onClick = [this, i]
        {
            processor.loadSampleForPadFromChooser(i);
        };
        addAndMakeVisible(padButtons[(size_t) i]);
    }

    for (int i = 0; i < 11; ++i)
    {
        auto& slider = knobs[(size_t) i];
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);

        if (auto* parameter = processor.apvts.getParameter(knobIds[(size_t) i]))
        {
            auto range = parameter->getNormalisableRange();
            slider.setRange(range.start, range.end, range.interval);
            slider.setValue(range.convertFrom0to1(parameter->getValue()), juce::dontSendNotification);

            slider.onValueChange = [this, i, parameter]
            {
                if (parameter != nullptr)
                {
                    auto range = parameter->getNormalisableRange();
                    parameter->setValueNotifyingHost(range.convertTo0to1((float) knobs[(size_t) i].getValue()));
                }
            };
        }

        knobLabels[(size_t) i].setText(knobNames[(size_t) i], juce::dontSendNotification);
        knobLabels[(size_t) i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knobLabels[(size_t) i]);
        addAndMakeVisible(slider);
    }
}

void PhysicalDrumEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111214));

    auto panel = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(juce::Colour(0xff202226));
    g.fillRoundedRectangle(panel, 12.0f);
    g.setColour(juce::Colour(0xff3a3d42));
    g.drawRoundedRectangle(panel, 12.0f, 1.0f);
}

void PhysicalDrumEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);

    title.setBounds(area.removeFromTop(34));
    subtitle.setBounds(area.removeFromTop(24));
    area.removeFromTop(12);

    auto padsArea = area.removeFromTop(270);
    const int cellW = padsArea.getWidth() / 4;
    const int cellH = padsArea.getHeight() / 3;

    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
    {
        const int row = i / 4;
        const int col = i % 4;
        auto cell = juce::Rectangle<int>(
            padsArea.getX() + col * cellW,
            padsArea.getY() + row * cellH,
            cellW - 8,
            cellH - 8).reduced(4);

        padLabels[(size_t) i].setBounds(cell.removeFromTop(26));
        padButtons[(size_t) i].setBounds(cell.reduced(18, 10));
    }

    area.removeFromTop(14);
    const int knobW = juce::jmax(1, area.getWidth() / 11);

    for (int i = 0; i < 11; ++i)
    {
        const int x = area.getX() + i * knobW;
        knobLabels[(size_t) i].setBounds(x, area.getY(), knobW - 4, 22);
        knobs[(size_t) i].setBounds(x, area.getY() + 22, knobW - 4, 112);
    }
}
