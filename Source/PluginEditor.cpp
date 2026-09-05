#include "PluginEditor.h"

namespace
{
bool isSupportedSampleFile(const juce::String& path)
{
    const auto ext = juce::File(path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg";
}
}

PhysicalDrumEngineAudioProcessorEditor::PhysicalDrumEngineAudioProcessorEditor(
    PhysicalDrumEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(960, 620);
    setResizable(false, false);

    title.setText("PHYSICAL DRUM ENGINE", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions{}.withHeight(26.0f).withStyle("Bold")));
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    subtitle.setText("V1.5  •  VELOCITY ENGINE  •  DRAG & DROP", juce::dontSendNotification);
    subtitle.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
    subtitle.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(subtitle);

    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
    {
        const auto& pad = processor.pads[(size_t) i];

        padLabels[(size_t) i].setText(
            pad.name + "  •  " + juce::String(pad.midiNote),
            juce::dontSendNotification);
        padLabels[(size_t) i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(padLabels[(size_t) i]);

        padButtons[(size_t) i].setButtonText(
            pad.sampleFile.existsAsFile() ? pad.sampleFile.getFileNameWithoutExtension()
                                          : "LOAD SAMPLE");
        padButtons[(size_t) i].onClick = [this, i]
        {
            processor.loadSampleForPadFromChooser(i);

            if (processor.pads[(size_t) i].sampleFile.existsAsFile())
                padButtons[(size_t) i].setButtonText(
                    processor.pads[(size_t) i].sampleFile.getFileNameWithoutExtension());
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
            slider.setValue(
                range.convertFrom0to1(parameter->getValue()),
                juce::dontSendNotification);

            slider.onValueChange = [this, i, parameter]
            {
                if (parameter != nullptr)
                {
                    auto range = parameter->getNormalisableRange();
                    parameter->setValueNotifyingHost(
                        range.convertTo0to1((float) knobs[(size_t) i].getValue()));
                }
            };
        }

        knobLabels[(size_t) i].setText(
            knobNames[(size_t) i], juce::dontSendNotification);
        knobLabels[(size_t) i].setJustificationType(juce::Justification::centred);
        addAndMakeVisible(knobLabels[(size_t) i]);
        addAndMakeVisible(slider);
    }

    setWantsKeyboardFocus(false);
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
        if (i == dragTargetPad)
        {
            auto r = padAreas[(size_t) i].toFloat().reduced(2.0f);
            g.setColour(juce::Colour(0xff777a80));
            g.drawRoundedRectangle(r, 8.0f, 2.0f);
        }
    }

    if (dragTargetPad >= 0)
    {
        auto textArea = padAreas[(size_t) dragTargetPad].reduced(8, 8);
        g.setColour(juce::Colour(0xffd8d8d3));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        g.drawText(
            "DROP SAMPLE",
            textArea.removeFromTop(18),
            juce::Justification::centred,
            false);
    }
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

        padAreas[(size_t) i] = cell;

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

bool PhysicalDrumEngineAudioProcessorEditor::isInterestedInFileDrag(
    const juce::StringArray& files)
{
    for (const auto& file : files)
        if (isSupportedSampleFile(file))
            return true;

    return false;
}

int PhysicalDrumEngineAudioProcessorEditor::padAtPosition(int x, int y) const
{
    for (int i = 0; i < PhysicalDrumEngineAudioProcessor::numPads; ++i)
        if (padAreas[(size_t) i].contains(x, y))
            return i;

    return -1;
}

void PhysicalDrumEngineAudioProcessorEditor::updateDragTarget(int x, int y)
{
    const int target = padAtPosition(x, y);

    if (target != dragTargetPad)
    {
        dragTargetPad = target;
        repaint();
    }
}

void PhysicalDrumEngineAudioProcessorEditor::fileDragEnter(
    const juce::StringArray& files, int x, int y)
{
    if (isInterestedInFileDrag(files))
        updateDragTarget(x, y);
}

void PhysicalDrumEngineAudioProcessorEditor::fileDragMove(
    const juce::StringArray& files, int x, int y)
{
    if (isInterestedInFileDrag(files))
        updateDragTarget(x, y);
}

void PhysicalDrumEngineAudioProcessorEditor::fileDragExit(
    const juce::StringArray&)
{
    clearDragTarget();
}

void PhysicalDrumEngineAudioProcessorEditor::clearDragTarget()
{
    if (dragTargetPad != -1)
    {
        dragTargetPad = -1;
        repaint();
    }
}

void PhysicalDrumEngineAudioProcessorEditor::filesDropped(
    const juce::StringArray& files, int x, int y)
{
    const int targetPad = padAtPosition(x, y);

    if (targetPad < 0 || files.isEmpty())
    {
        clearDragTarget();
        return;
    }

    // Only the first dropped sample is assigned to the target pad.
    // This keeps one drop = one predictable kit assignment.
    const juce::File sampleFile(files[0]);

    if (isSupportedSampleFile(sampleFile.getFullPathName())
        && processor.loadSampleForPad(targetPad, sampleFile))
    {
        padButtons[(size_t) targetPad].setButtonText(
            sampleFile.getFileNameWithoutExtension());

        repaint();
    }

    clearDragTarget();
}
