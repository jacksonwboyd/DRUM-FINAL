#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

class PhysicalDrumEngineAudioProcessor final : public juce::AudioProcessor
{
public:
    PhysicalDrumEngineAudioProcessor();
    ~PhysicalDrumEngineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static constexpr int numPads = 12;
    struct Pad {
        juce::String name;
        int midiNote = 36;
        std::unique_ptr<juce::AudioBuffer<float>> sample;
        double sampleRate = 44100.0;
        juce::File sampleFile;
        float trim = 1.0f;
    };

    std::array<Pad, numPads> pads;
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

    bool loadSampleForPad(int padIndex, const juce::File& file);
    void loadSampleForPadFromChooser(int padIndex);
    void loadFactorySnare();
    int noteToPad(int note) const;

private:
    struct Voice {
        bool active = false;
        int pad = -1;
        double pos = 0.0;
        double rate = 1.0;
        float velocity = 1.0f;
        float gain = 1.0f;
        float attack = 0.0f;
        int age = 0;
        juce::Random rng;
    };

    static constexpr int maxVoices = 64;
    std::array<Voice, maxVoices> voices;
    double currentSampleRate = 44100.0;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void triggerPad(int padIndex, float velocity);
    void renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void clearVoices();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalDrumEngineAudioProcessor)
};
