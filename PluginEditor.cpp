#include "PluginEditor.h"

PhysicalDrumEngineAudioProcessorEditor::PhysicalDrumEngineAudioProcessorEditor(PhysicalDrumEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(980, 650);
    title.setText("PHYSICAL DRUM ENGINE", juce::dontSendNotification);
    title.setFont(juce::Font(26.0f, juce::Font::bold));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);
    subtitle.setText("V1.1  •  MIDI DRUM RESYNTHESIS", juce::dontSendNotification);
    subtitle.setFont(juce::Font(12.0f));
    addAndMakeVisible(subtitle);

    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
    {
        padButtons[i] = std::make_unique<juce::TextButton>("LOAD");
        addAndMakeVisible(*padButtons[i]);
        padLabels[i] = std::make_unique<juce::Label>();
        padLabels[i]->setText(processor.pads[i].name + "  " + juce::String(processor.pads[i].midiNote), juce::dontSendNotification);
        padLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*padLabels[i]);
        padButtons[i]->onClick = [this, i] { processor.loadSampleForPadFromChooser(i); };
    }

    for (int i = 0; i < 11; ++i)
    {
        knobs[i] = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        knobs[i]->setRange(0.0, 1.0, 0.001);
        knobs[i]->setTextValueSuffix("");
        addAndMakeVisible(*knobs[i]);
        knobLabels[i] = std::make_unique<juce::Label>();
        knobLabels[i]->setText(knobNames[i], juce::dontSendNotification);
        knobLabels[i]->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*knobLabels[i]);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, knobIds[i], *knobs[i]);
    }
}

void PhysicalDrumEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111214));
    g.setColour(juce::Colour(0xff202226));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 12.0f);
    g.setColour(juce::Colour(0xff3a3d42));
    g.drawRoundedRectangle(12.0f, 12.0f, (float)getWidth()-24.0f, (float)getHeight()-24.0f, 12.0f, 1.0f);
}

void PhysicalDrumEngineAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    title.setBounds(area.removeFromTop(34));
    subtitle.setBounds(area.removeFromTop(24));
    area.removeFromTop(10);

    auto padsArea = area.removeFromTop(250);
    const int cellW = padsArea.getWidth() / 4;
    const int cellH = padsArea.getHeight() / 3;
    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
    {
        int row = i / 4, col = i % 4;
        auto cell = juce::Rectangle<int>(padsArea.getX()+col*cellW, padsArea.getY()+row*cellH, cellW-8, cellH-8).reduced(4);
        padLabels[i]->setBounds(cell.removeFromTop(28));
        padButtons[i]->setBounds(cell.reduced(18, 12));
    }

    area.removeFromTop(16);
    const int knobW = area.getWidth() / 11;
    for (int i = 0; i < 11; ++i)
    {
        auto x = area.getX() + i * knobW;
        knobLabels[i]->setBounds(x, area.getY(), knobW-4, 22);
        knobs[i]->setBounds(x, area.getY()+22, knobW-4, 100);
    }
}
