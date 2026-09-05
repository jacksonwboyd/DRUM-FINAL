#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }
float param(const juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue(id)->load(); }
}

PhysicalDrumEngineAudioProcessor::PhysicalDrumEngineAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameters())
{
    const char* names[numPads] = {"Kick", "Snare", "Closed Hat", "Open Hat", "Tom 1", "Tom 2", "Tom 3", "Crash", "Ride", "Clap", "Perc 1", "Perc 2"};
    const int notes[numPads] = {36, 38, 42, 46, 45, 43, 41, 49, 51, 39, 37, 40};
    for (int i = 0; i < numPads; ++i) { pads[i].name = names[i]; pads[i].midiNote = notes[i]; }
    formatManager.registerBasicFormats();
    loadFactorySnare();
}

juce::AudioProcessorValueTreeState::ParameterLayout PhysicalDrumEngineAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&](const char* id, const char* name, float min, float max, float def)
    {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(min, max, 0.001f), def));
    };
    add("physicality", "Physicality", 0.0f, 1.0f, 0.75f);
    add("transient", "Transient", 0.0f, 1.0f, 0.70f);
    add("attack", "Attack", 0.0f, 1.0f, 0.35f);
    add("brightness", "Brightness", 0.0f, 1.0f, 0.55f);
    add("pitch", "Pitch Response", 0.0f, 1.0f, 0.35f);
    add("body", "Body", 0.0f, 1.5f, 1.0f);
    add("decay", "Decay", 0.0f, 1.0f, 0.55f);
    add("timing", "Timing", 0.0f, 1.0f, 0.20f);
    add("variation", "Hit Variation", 0.0f, 1.0f, 0.30f);
    add("output", "Output dB", -18.0f, 6.0f, 0.0f);
    add("mix", "Dry/Wet", 0.0f, 1.0f, 1.0f);
    return {p.begin(), p.end()};
}

bool PhysicalDrumEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PhysicalDrumEngineAudioProcessor::prepareToPlay(double sr, int)
{
    currentSampleRate = sr;
    clearVoices();
}

void PhysicalDrumEngineAudioProcessor::clearVoices()
{
    for (auto& v : voices) v.active = false;
}

int PhysicalDrumEngineAudioProcessor::noteToPad(int note) const
{
    for (int i = 0; i < numPads; ++i) if (pads[i].midiNote == note) return i;
    return -1;
}

void PhysicalDrumEngineAudioProcessor::triggerPad(int padIndex, float velocity)
{
    if (padIndex < 0 || padIndex >= numPads || !pads[padIndex].sample) return;
    Voice* voice = nullptr;
    for (auto& candidate : voices) if (!candidate.active) { voice = &candidate; break; }
    if (!voice) voice = &voices[0];

    const float phy = clamp01(param(apvts, "physicality"));
    const float trans = clamp01(param(apvts, "transient"));
    const float pitch = clamp01(param(apvts, "pitch"));
    const float variation = clamp01(param(apvts, "variation"));
    const float timing = clamp01(param(apvts, "timing"));

    const float r = voice->rng.nextFloat() - 0.5f;
    const float velocityPitch = (velocity - 0.5f) * 30.0f * pitch * phy;
    const float randomPitch = r * 7.0f * variation * phy;
    const double rate = std::pow(2.0, (velocityPitch + randomPitch) / 1200.0);

    voice->active = true;
    voice->pad = padIndex;
    voice->pos = 0.0;
    voice->rate = rate * (pads[padIndex].sampleRate / currentSampleRate);
    voice->velocity = velocity;
    voice->gain = std::pow(std::max(0.001f, velocity), 0.62f) * (0.80f + 0.20f * trans * phy) * pads[padIndex].trim;
    voice->attack = (0.00025f + (1.0f - velocity) * 0.012f * clamp01(param(apvts, "attack")) * phy) * (float)currentSampleRate;
    voice->age = 0;

    // Timing is represented as a tiny pre-roll delay by starting the voice slightly late.
    if (timing > 0.0f)
        voice->pos = -voice->rng.nextFloat() * timing * phy * 0.0025 * currentSampleRate;
}

void PhysicalDrumEngineAudioProcessor::renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!v.active || v.pad < 0 || !pads[v.pad].sample) return;
    auto& pad = pads[v.pad];
    const auto* data = pad.sample->getReadPointer(0);
    const int total = pad.sample->getNumSamples();
    const float phy = clamp01(param(apvts, "physicality"));
    const float transient = clamp01(param(apvts, "transient"));
    const float brightness = clamp01(param(apvts, "brightness"));
    const float body = juce::jlimit(0.0f, 1.5f, param(apvts, "body"));
    const float decay = clamp01(param(apvts, "decay"));
    const float mix = clamp01(param(apvts, "mix"));

    for (int i = 0; i < numSamples; ++i)
    {
        if (v.pos < 0.0) { v.pos += 1.0; ++v.age; continue; }
        const int idx = (int)v.pos;
        if (idx >= total) { v.active = false; break; }
        const int next = std::min(idx + 1, total - 1);
        const float frac = (float)(v.pos - idx);
        float s = data[idx] + (data[next] - data[idx]) * frac;

        const float progress = (float)idx / (float)std::max(1, total);
        const float transientShape = std::exp(-progress * 90.0f);
        const float bodyShape = 0.55f + 0.45f * std::exp(-progress * 7.0f);
        const float attackEnv = v.age < v.attack ? (float)v.age / std::max(1.0f, v.attack) : 1.0f;
        const float tailShape = std::pow(1.0f - progress, 0.5f + (1.0f - decay) * 2.5f);
        const float velocityTransient = 1.0f + phy * transient * transientShape * ((v.velocity - 0.5f) * 1.9f);
        const float velocityBody = body * (0.75f + 0.5f * v.velocity) * (1.0f + phy * 0.18f * bodyShape);
        const float brightnessShape = 1.0f + (brightness - 0.5f) * phy * transientShape * 1.5f;
        const float env = attackEnv * tailShape;

        float out = s * v.gain * velocityTransient * velocityBody * brightnessShape * env * mix;
        buffer.addSample(0, startSample + i, out);
        if (buffer.getNumChannels() > 1) buffer.addSample(1, startSample + i, out);
        v.pos += v.rate;
        ++v.age;
    }
}

void PhysicalDrumEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    const int totalSamples = buffer.getNumSamples();
    for (const auto metadata : midi)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn() && msg.getVelocity() > 0.0f)
            triggerPad(noteToPad(msg.getNoteNumber()), msg.getFloatVelocity());
    }

    for (auto& v : voices) renderVoice(v, buffer, 0, totalSamples);
    buffer.applyGain(dbToGain(param(apvts, "output")));
}

bool PhysicalDrumEngineAudioProcessor::loadSampleForPad(int padIndex, const juce::File& file)
{
    if (padIndex < 0 || padIndex >= numPads || !file.existsAsFile()) return false;
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) return false;
    auto audio = std::make_unique<juce::AudioBuffer<float>>((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(audio.get(), 0, (int)reader->lengthInSamples, 0, true, true);
    pads[padIndex].sample = std::move(audio);
    pads[padIndex].sampleRate = reader->sampleRate;
    pads[padIndex].sampleFile = file;
    return true;
}

void PhysicalDrumEngineAudioProcessor::loadSampleForPadFromChooser(int padIndex)
{
    juce::FileChooser chooser("Choose a WAV sample", {}, "*.wav");
    if (chooser.browseForFileToOpen()) loadSampleForPad(padIndex, chooser.getResult());
}

void PhysicalDrumEngineAudioProcessor::loadFactorySnare()
{
    // Embedded by JUCE so the AU/VST3 bundle does not depend on an external WAV path.
    if (auto* format = formatManager.findFormatForFileExtension("wav"))
    {
        std::unique_ptr<juce::InputStream> stream(new juce::MemoryInputStream(BinaryData::snare_wav, BinaryData::snare_wavSize, false));
        std::unique_ptr<juce::AudioFormatReader> reader(format->createReaderFor(stream.release(), true));
        if (reader)
        {
            auto audio = std::make_unique<juce::AudioBuffer<float>>((int)reader->numChannels, (int)reader->lengthInSamples);
            reader->read(audio.get(), 0, (int)reader->lengthInSamples, 0, true, true);
            pads[1].sample = std::move(audio);
            pads[1].sampleRate = reader->sampleRate;
        }
    }
}

void PhysicalDrumEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void PhysicalDrumEngineAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* PhysicalDrumEngineAudioProcessor::createEditor()
{
    return new PhysicalDrumEngineAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhysicalDrumEngineAudioProcessor();
}
