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
    for (auto& candidate : voices)
        if (!candidate.active) { voice = &candidate; break; }
    if (!voice) voice = &voices[0];

    const float phy = clamp01(param(apvts, "physicality"));
    const float trans = clamp01(param(apvts, "transient"));
    const float pitch = clamp01(param(apvts, "pitch"));
    const float variation = clamp01(param(apvts, "variation"));
    const float timing = clamp01(param(apvts, "timing"));
    const float brightness = clamp01(param(apvts, "brightness"));
    const float attack = clamp01(param(apvts, "attack"));

    // Deliberately exaggerated per-hit physical variation.
    // Each hit gets its own gain, pitch, decay, transient and brightness identity.
    auto signedRandom = [&]() { return voice->rng.nextFloat() * 2.0f - 1.0f; };
    const float intensity = juce::jlimit(0.0f, 1.0f, 0.15f + 0.85f * phy);
    const float variationAmount = juce::jlimit(0.0f, 1.0f, 0.10f + 0.90f * variation);

    const float velocityPitch = (velocity - 0.5f) * 70.0f * pitch * phy;
    const float randomPitch = signedRandom() * (55.0f * intensity * variationAmount);
    const float cents = velocityPitch + randomPitch;
    const double rate = std::pow(2.0, cents / 1200.0);

    const float gainJitterDb = signedRandom() * (5.0f * intensity * variationAmount);
    const float randomGain = dbToGain(gainJitterDb);
    const float transientJitter = 1.0f + signedRandom() * (0.75f * intensity * variationAmount);
    const float decayJitter = 1.0f + signedRandom() * (0.45f * intensity * variationAmount);
    const float brightnessJitter = signedRandom() * (0.35f * intensity * variationAmount);

    voice->active = true;
    voice->pad = padIndex;
    voice->pos = 0.0;
    voice->rate = rate * (pads[padIndex].sampleRate / currentSampleRate);
    voice->velocity = velocity;
    voice->gainJitter = randomGain;
    voice->pitchCents = cents;
    voice->decayScale = juce::jlimit(0.45f, 1.55f, decayJitter);
    voice->transientBoost = juce::jlimit(0.25f, 1.90f, transientJitter);
    voice->brightnessAmount = juce::jlimit(0.05f, 1.0f, brightness + brightnessJitter);
    voice->lowpassState = 0.0f;

    // Brightness becomes a real spectral change: lower values strongly soften the hit.
    const float cutoff = juce::jlimit(1800.0f, 19000.0f,
        1800.0f + voice->brightnessAmount * 17200.0f);
    voice->lowpassCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float)currentSampleRate);

    voice->gain = std::pow(std::max(0.001f, velocity), 0.62f)
        * (0.70f + 0.60f * trans * phy)
        * randomGain
        * pads[padIndex].trim;

    voice->attack = (0.00015f
        + (1.0f - velocity) * 0.018f * attack * (0.5f + phy * 1.5f))
        * (float)currentSampleRate;

    voice->age = 0;

    // Physical timing variation: up to 8 ms at maximum, plus a random per-hit offset.
    if (timing > 0.0f)
        voice->pos = -voice->rng.nextFloat() * timing * phy * 0.008 * currentSampleRate;
}

void PhysicalDrumEngineAudioProcessor::renderVoice(Voice& v, juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!v.active || v.pad < 0 || !pads[v.pad].sample) return;

    auto& pad = pads[v.pad];
    const auto* data = pad.sample->getReadPointer(0);
    const int total = pad.sample->getNumSamples();

    const float phy = clamp01(param(apvts, "physicality"));
    const float transient = clamp01(param(apvts, "transient"));
    const float body = juce::jlimit(0.0f, 1.5f, param(apvts, "body"));
    const float decay = clamp01(param(apvts, "decay"));
    const float mix = clamp01(param(apvts, "mix"));

    for (int i = 0; i < numSamples; ++i)
    {
        if (v.pos < 0.0)
        {
            v.pos += 1.0;
            ++v.age;
            continue;
        }

        const int idx = (int)v.pos;
        if (idx >= total) { v.active = false; break; }

        const int next = std::min(idx + 1, total - 1);
        const float frac = (float)(v.pos - idx);
        const float raw = data[idx] + (data[next] - data[idx]) * frac;
        const float progress = (float)idx / (float)std::max(1, total);

        const float transientShape = std::exp(-progress * 105.0f);
        const float bodyShape = 0.55f + 0.45f * std::exp(-progress * 6.0f);
        const float attackEnv = v.age < v.attack
            ? (float)v.age / std::max(1.0f, v.attack)
            : 1.0f;

        // Decay now has a much more audible effect because it controls a per-hit envelope.
        const float decayExponent = juce::jlimit(0.20f, 5.0f,
            0.35f + (1.0f - decay) * 3.2f) / v.decayScale;
        const float tailShape = std::pow(std::max(0.0f, 1.0f - progress), decayExponent);

        // Velocity and physicality strongly reshape the transient/body relationship.
        const float velocityTransient = 1.0f
            + transient * transientShape * (0.65f + 1.75f * v.velocity)
            * v.transientBoost * (0.55f + 0.90f * phy);

        const float velocityBody = body
            * (0.55f + 0.95f * v.velocity)
            * (0.70f + 0.55f * phy * bodyShape);

        // Real spectral movement instead of a volume-only "brightness" control.
        v.lowpassState += v.lowpassCoeff * (raw - v.lowpassState);
        const float spectral = v.lowpassState + v.brightnessAmount * (raw - v.lowpassState);

        const float env = attackEnv * tailShape;
        const float out = spectral * v.gain * velocityTransient * velocityBody * env * mix;

        buffer.addSample(0, startSample + i, out);
        if (buffer.getNumChannels() > 1)
            buffer.addSample(1, startSample + i, out);

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
    if (padIndex < 0 || padIndex >= numPads) return;

    sampleChooser = std::make_unique<juce::FileChooser>(
        "Choose a WAV sample", juce::File{}, "*.wav");

    sampleChooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, padIndex](const juce::FileChooser& chooser)
        {
            if (chooser.getURLResult().isLocalFile())
                loadSampleForPad(padIndex, chooser.getResult());
            sampleChooser.reset();
        });
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
