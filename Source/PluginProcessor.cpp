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

    // Velocity is the primary physical input.  The knobs scale how strongly
    // velocity changes the sound across the entire kit.
    const float phy        = clamp01(param(apvts, "physicality"));
    const float trans      = clamp01(param(apvts, "transient"));
    const float pitch      = clamp01(param(apvts, "pitch"));
    const float variation  = clamp01(param(apvts, "variation"));
    const float timing     = clamp01(param(apvts, "timing"));
    const float brightness = clamp01(param(apvts, "brightness"));
    const float attack     = clamp01(param(apvts, "attack"));
    const float decay      = clamp01(param(apvts, "decay"));
    const float body       = juce::jlimit(0.0f, 1.5f, param(apvts, "body"));

    const float vel = clamp01(velocity);
    const float velCurve = std::pow(vel, 0.82f);
    const float physicalAmount = juce::jlimit(0.0f, 1.0f, 0.20f + 0.80f * phy);
    const float variationAmount = 0.15f + 0.85f * variation;

    auto signedRandom = [&]() { return voice->rng.nextFloat() * 2.0f - 1.0f; };

    // Small stochastic identity remains, but it is deliberately subordinate
    // to velocity.  The same velocity therefore produces the same overall
    // physical direction while individual hits still have fingerprints.
    const float randomIdentity = signedRandom() * variationAmount * physicalAmount;

    // HIGH velocity = longer, fuller, more sustained hit.
    // LOW velocity = shorter, softer, less sustained hit.
    // maxProgress gates the sample, while rate stretches/compresses it.
    const float sustainFromVelocity = 0.34f + 0.66f * std::pow(vel, 0.72f);
    const float sustainFromDecay = 0.55f + 0.55f * decay;
    const float sustainVariation = 1.0f + randomIdentity * 0.10f;
    voice->maxProgress = juce::jlimit(0.22f, 1.0f,
        sustainFromVelocity * sustainFromDecay * sustainVariation);

    // Higher velocities travel through more of the sample more slowly.
    // This is the main audible velocity -> duration relationship.
    const float velocityDurationRate = 1.18f - 0.46f * vel;
    const float pitchFromVelocity = (vel - 0.5f) * 110.0f * pitch * (0.45f + 0.90f * phy);
    const float randomPitch = signedRandom() * (18.0f + 42.0f * variation) * physicalAmount;
    const float cents = pitchFromVelocity + randomPitch;
    const double pitchRate = std::pow(2.0, cents / 1200.0);

    voice->active = true;
    voice->pad = padIndex;
    voice->pos = 0.0;
    voice->rate = velocityDurationRate * pitchRate * (pads[padIndex].sampleRate / currentSampleRate);
    voice->velocity = vel;
    voice->pitchCents = cents;

    // Velocity-driven accent: soft hits are genuinely quieter; hard hits hit
    // the transient/body much harder. Random gain is only a small identity layer.
    const float gainIdentityDb = randomIdentity * 2.8f;
    voice->gainJitter = dbToGain(gainIdentityDb);
    voice->gain = std::pow(std::max(0.001f, vel), 1.20f)
        * (0.45f + 1.15f * trans * (0.35f + 0.65f * phy))
        * voice->gainJitter
        * pads[padIndex].trim;

    // Velocity now controls the attack shape: hard hits arrive almost
    // immediately; soft hits have a softer/longer onset.
    const float attackSeconds = 0.00015f
        + (1.0f - vel) * (0.002f + 0.022f * attack * (0.45f + 0.85f * phy));
    voice->attack = attackSeconds * (float) currentSampleRate;

    // Velocity drives transient intensity directly.
    const float velocityTransient = 0.35f + 2.10f * std::pow(vel, 0.78f);
    const float transientIdentity = 1.0f + randomIdentity * 0.32f;
    voice->transientBoost = juce::jlimit(0.25f, 2.60f,
        velocityTransient * transientIdentity);

    // Velocity drives brightness.  Harder hits open the top end substantially.
    const float velocityBrightness = brightness * (0.16f + 0.84f * std::pow(vel, 0.70f));
    const float brightnessIdentity = randomIdentity * 0.10f;
    voice->brightnessAmount = juce::jlimit(0.02f, 1.0f,
        velocityBrightness + brightnessIdentity);

    const float cutoff = juce::jlimit(900.0f, 19500.0f,
        900.0f + voice->brightnessAmount * 18600.0f);
    voice->lowpassCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
        * cutoff / (float) currentSampleRate);
    voice->lowpassState = 0.0f;

    // Body is global: it affects every pad the same way, while velocity decides
    // how much body is actually exposed on each hit.
    voice->decayScale = juce::jlimit(0.30f, 1.70f,
        0.55f + 0.95f * vel + 0.28f * decay + randomIdentity * 0.10f);

    voice->age = 0;

    // Timing remains a global amount. The random offset is intentionally tiny
    // compared with the velocity-driven dynamics so timing never dominates.
    if (timing > 0.0f)
        voice->pos = -voice->rng.nextFloat() * timing * physicalAmount * 0.006 * currentSampleRate;
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

        const int idx = (int) v.pos;
        if (idx >= total) { v.active = false; break; }

        const int next = std::min(idx + 1, total - 1);
        const float frac = (float) (v.pos - idx);
        const float raw = data[idx] + (data[next] - data[idx]) * frac;
        const float progress = (float) idx / (float) std::max(1, total);

        // Low velocity can end the hit early; high velocity is allowed to use
        // essentially the whole sample. This is a true duration change rather
        // than a random volume change.
        if (progress >= v.maxProgress)
        {
            v.active = false;
            break;
        }

        const float normalizedLife = progress / std::max(0.001f, v.maxProgress);
        const float transientShape = std::exp(-normalizedLife * 105.0f);
        const float bodyShape = 0.50f + 0.50f * std::exp(-normalizedLife * 5.0f);

        const float attackEnv = v.age < v.attack
            ? (float) v.age / std::max(1.0f, v.attack)
            : 1.0f;

        // Decay knob is global; velocity chooses how much of that decay is
        // exposed. Hard hits stay alive longer and retain more body.
        const float decayExponent = juce::jlimit(0.18f, 5.5f,
            0.22f + (1.0f - decay) * 3.8f) / v.decayScale;
        const float tailShape = std::pow(
            std::max(0.0f, 1.0f - normalizedLife), decayExponent);

        // Harder hits produce a much larger transient and body contribution.
        const float velocityTransient = 0.45f
            + transient * v.transientBoost * transientShape
            * (0.35f + 1.75f * v.velocity)
            * (0.45f + 0.95f * phy);

        const float velocityBody = 0.35f
            + body * (0.25f + 1.35f * v.velocity)
            * (0.55f + 0.85f * phy * bodyShape);

        // Global brightness control + velocity-driven spectral opening.
        v.lowpassState += v.lowpassCoeff * (raw - v.lowpassState);
        const float spectral = v.lowpassState
            + v.brightnessAmount * (raw - v.lowpassState);

        const float env = attackEnv * tailShape;
        const float out = spectral * v.gain
            * velocityTransient * velocityBody * env * mix;

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
