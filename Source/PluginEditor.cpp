#include "PluginEditor.h"

PhysicalDrumEngineAudioProcessorEditor::PhysicalDrumEngineAudioProcessorEditor(PhysicalDrumEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(960, 620);
    setResizable(false, false);

    title.setText("PHYSICAL DRUM ENGINE", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions{}.withHeight(26.0f).withStyle("Bold")));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    subtitle.setText("V1.5  •  VELOCITY ENGINE  •  DRAG & DROP SAMPLES", juce::dontSendNotification);
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

        padButtons[(size_t) i].setButtonText("DROP / LOAD WAV");
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

    setWantsKeyboardFocus(false);
}

bool PhysicalDrumEngineAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        juce::File f(path);
        if (f.existsAsFile() && (f.hasFileExtension("wav;aif;aiff;flac;ogg")))
            return true;
    }
    return false;
}

void PhysicalDrumEngineAudioProcessorEditor::itemDragEnter(const juce::DragAndDropTarget::SourceDetails& details)
{
    setDragPad(padAtPoint(details.localPosition.roundToInt()));
    repaint();
}

void PhysicalDrumEngineAudioProcessorEditor::itemDragMove(const juce::DragAndDropTarget::SourceDetails& details)
{
    setDragPad(padAtPoint(details.localPosition.roundToInt()));
    repaint();
}

void PhysicalDrumEngineAudioProcessorEditor::itemDragExit(const juce::DragAndDropTarget::SourceDetails&)
{
    setDragPad(-1);
    repaint();
}

void PhysicalDrumEngineAudioProcessorEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    const int targetPad = padAtPoint({ x, y });
    setDragPad(-1);

    if (targetPad < 0 || files.isEmpty())
    {
        repaint();
        return;
    }

    juce::File file(files[0]);
    if (!file.existsAsFile() || !file.hasFileExtension("wav;aif;aiff;flac;ogg"))
    {
        repaint();
        return;
    }

    if (processor.loadSampleForPad(targetPad, file))
    {
        padLabels[(size_t) targetPad].setText(
            processor.pads[(size_t) targetPad].name + "  •  " + file.getFileNameWithoutExtension(),
            juce::dontSendNotification);
        padButtons[(size_t) targetPad].setButtonText("LOADED  •  DROP TO REPLACE");
    }

    repaint();
}

int PhysicalDrumEngineAudioProcessorEditor::padAtPoint(juce::Point<int> point) const
{
    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
        if (padDropBounds[(size_t) i].contains(point))
            return i;
    return -1;
}

void PhysicalDrumEngineAudioProcessorEditor::setDragPad(int newPad)
{
    if (dragPad == newPad) return;
    dragPad = newPad;
    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
        padButtons[(size_t) i].setButtonText(i == dragPad ? "DROP SAMPLE HERE" : "DROP / LOAD WAV");
}

void PhysicalDrumEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111214));

    auto panel = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(juce::Colour(0xff202226));
    g.fillRoundedRectangle(panel, 12.0f);
    g.setColour(juce::Colour(0xff3a3d42));
    g.drawRoundedRectangle(panel, 12.0f, 1.0f);

    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
    {
        auto r = padDropBounds[(size_t) i].toFloat().reduced(1.0f);
        g.setColour(i == dragPad ? juce::Colour(0xffd8d8d3) : juce::Colour(0xff3a3d42));
        g.drawRoundedRectangle(r, 8.0f, i == dragPad ? 2.0f : 1.0f);
    }

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    g.setColour(juce::Colour(0xff777b80));
    g.drawText("DRAG WAV / AIFF / FLAC / OGG ONTO ANY PAD", 28, 584, 904, 18, juce::Justification::centred);
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

        padDropBounds[(size_t) i] = cell;
        auto labelArea = cell;
        padLabels[(size_t) i].setBounds(labelArea.removeFromTop(26));
        padButtons[(size_t) i].setBounds(labelArea.reduced(18, 10));
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
