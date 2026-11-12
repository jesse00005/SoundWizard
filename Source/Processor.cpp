#include "Processor.h"
#include "Editor.h"
#include "BinaryData.h"
#include <cmath>

juce::AudioProcessorValueTreeState::ParameterLayout SoundWizardProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout parameters;
    auto seconds = [](float low, float high, float middle) {
        juce::NormalisableRange<float> range(low, high, 0.001f);
        range.setSkewForCentre(middle);
        return range;
    };
    parameters.add(std::make_unique<juce::AudioParameterInt>(juce::ParameterID{"instrument", 1}, "Instrument", 0, 65535, 0));
    parameters.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"customEnvelope", 1}, "Custom envelope", false));
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"attack", 1}, "Attack", seconds(0.001f, 5, 0.15f), 0.01f));
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"decay", 1}, "Decay", seconds(0.001f, 5, 0.3f), 0.5f));
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"sustain", 1}, "Sustain", juce::NormalisableRange<float>(0, 1, 0.001f), 0.7f));
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"release", 1}, "Release", seconds(0.005f, 10, 0.4f), 0.4f));
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"reverb", 1}, "Reverb", juce::NormalisableRange<float>(0, 1, 0.001f), 0.12f));
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"volume", 1}, "Volume", juce::NormalisableRange<float>(-60, 6, 0.1f), -9.0f));
    parameters.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"mono", 1}, "Mono", false));
    parameters.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"portamento", 1}, "Portamento", seconds(0, 2, 0.2f), 0.0f));
    return parameters;
}

SoundWizardProcessor::SoundWizardProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      Thread("Sound Wizard loader"), state(*this, nullptr, "SOUND_WIZARD", makeLayout()) {
    program = state.getRawParameterValue("instrument");
    customEnvelope = state.getRawParameterValue("customEnvelope");
    attack = state.getRawParameterValue("attack");
    decay = state.getRawParameterValue("decay");
    sustain = state.getRawParameterValue("sustain");
    release = state.getRawParameterValue("release");
    reverbAmount = state.getRawParameterValue("reverb");
    volumeDb = state.getRawParameterValue("volume");
    mono = state.getRawParameterValue("mono");
    portamento = state.getRawParameterValue("portamento");
    for (auto& point : scope) point.store(0);
    engine = SoundFontEngine::load(BinaryData::WizardStarter_sf2, BinaryData::WizardStarter_sf2Size);
    info.name = "Wizard Starter";
    info.message = "Ready";
    if (engine) info.instruments = engine->instrumentNames();
    else info.message = "Starter bank could not be loaded. Use Load SF2.";
    startThread();
}

SoundWizardProcessor::~SoundWizardProcessor() {
    signalThreadShouldExit();
    loadEvent.signal();
    stopThread(-1);
}

bool SoundWizardProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    auto output = layout.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void SoundWizardProcessor::prepareToPlay(double rate, int) {
    sampleRate.store(rate);
    const juce::ScopedLock lock(engineMutex);
    if (engine) { engine->stop(); engine->setSampleRate(rate); }
    reverb.setSampleRate(rate);
    reverb.reset();
    volume.reset(rate, 0.02);
    volume.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(volumeDb->load(), -60.0f));
    previewSamples = 0;
}

void SoundWizardProcessor::releaseResources() {
    const juce::ScopedLock lock(engineMutex);
    if (engine) engine->stop();
    reverb.reset();
    activeVoices.store(0);
}

void SoundWizardProcessor::selectInstrument(int index) {
    auto* parameter = state.getParameter("instrument");
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1((float)index));
    parameter->endChangeGesture();
}

int SoundWizardProcessor::selectedInstrument() const { return juce::roundToInt(program->load()); }
SoundWizardProcessor::LibraryInfo SoundWizardProcessor::getLibraryInfo() const {
    const juce::ScopedLock lock(infoMutex);
    return info;
}

void SoundWizardProcessor::loadSoundFont(const juce::File& file) { requestLoad(file.getFullPathName(), false); }
void SoundWizardProcessor::loadStarterBank() { requestLoad({}, false); }

void SoundWizardProcessor::requestLoad(juce::String path, bool restore) {
    {
        const juce::ScopedLock lock(infoMutex);
        pendingPath = std::move(path);
        pendingRestore = restore;
        pending = true;
        ++requestVersion;
        info.loading = true;
        info.message = "Loading SoundFont...";
        ++info.revision;
    }
    loadEvent.signal();
}

void SoundWizardProcessor::run() {
    while (!threadShouldExit()) {
        loadEvent.wait(200);
        juce::String path;
        bool restore = false;
        int version = 0;
        {
            const juce::ScopedLock lock(infoMutex);
            if (!pending) continue;
            path = pendingPath;
            restore = pendingRestore;
            version = requestVersion;
            pending = false;
        }
        if (threadShouldExit()) break;
        std::unique_ptr<SoundFontEngine> loaded;
        juce::String error;
        if (path.isEmpty()) {
            loaded = SoundFontEngine::load(BinaryData::WizardStarter_sf2, BinaryData::WizardStarter_sf2Size);
        } else {
            juce::File file(path);
            if (!file.existsAsFile()) error = "SoundFont missing. Load SF2 to locate it.";
            else if (file.getSize() > 256LL * 1024 * 1024) error = "File exceeds the 256 MB limit.";
            else {
                juce::MemoryBlock bytes;
                if (file.loadFileAsData(bytes))
                    loaded = SoundFontEngine::load(bytes.getData(), (int)bytes.getSize());
            }
        }
        if (!loaded && error.isEmpty()) error = "Could not read this SF2 file. Try another SoundFont.";
        if (threadShouldExit()) break;
        juce::StringArray names;
        if (loaded) names = loaded->instrumentNames();
        std::unique_ptr<SoundFontEngine> previous;
        {
            const juce::ScopedLock infoLock(infoMutex);
            if (version != requestVersion) continue;
            if (loaded || restore) {
                const juce::ScopedLock engineLock(engineMutex);
                if (loaded) loaded->setSampleRate(sampleRate.load());
                previous = std::move(engine);
                engine = std::move(loaded);
                panicRequested.store(true);
                if (!restore && engine) selectInstrument(0);
                info.path = path;
                info.name = path.isEmpty() ? "Wizard Starter" : juce::File(path).getFileNameWithoutExtension();
                info.instruments = std::move(names);
                if (engine && path.isNotEmpty()) {
                    info.recentPaths.removeString(path);
                    info.recentPaths.insert(0, path);
                    while (info.recentPaths.size() > 8) info.recentPaths.remove(8);
                }
            }
            info.loading = false;
            info.message = error.isEmpty() ? "Ready" : error;
            ++info.revision;
        }
        // Large sample banks are freed here, away from the audio callback.
    }
}

void SoundWizardProcessor::panic() {
    keyboard.reset();
    panicRequested.store(true);
}

void SoundWizardProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboard.processNextMidiBuffer(midi, 0, buffer.getNumSamples(), true);
    const juce::ScopedTryLock lock(engineMutex);
    if (!lock.isLocked()) { midi.clear(); return; }
    if (panicRequested.exchange(false)) {
        if (engine) engine->stop();
        reverb.reset();
        previewSamples = 0;
    }
    if (engine) {
        engine->setInstrument(selectedInstrument());
        engine->setPlaybackMode(mono->load() > 0.5f, portamento->load());
        engine->setEnvelope(customEnvelope->load() > 0.5f, attack->load(), decay->load(), sustain->load(), release->load());
        if (previewRequested.exchange(false)) {
            engine->handleMidi(juce::MidiMessage::noteOff(16, 60));
            engine->handleMidi(juce::MidiMessage::noteOn(16, 60, 0.8f));
            previewSamples = juce::roundToInt(sampleRate.load());
        }
        auto render = [&](int start, int count) {
            if (previewSamples > 0 && previewSamples <= count) {
                engine->render(buffer, start, previewSamples);
                engine->handleMidi(juce::MidiMessage::noteOff(16, 60));
                engine->render(buffer, start + previewSamples, count - previewSamples);
                previewSamples = 0;
            } else {
                engine->render(buffer, start, count);
                previewSamples = juce::jmax(0, previewSamples - count);
            }
        };
        int position = 0;
        for (const auto metadata : midi) {
            const int eventPosition = juce::jlimit(position, buffer.getNumSamples(), metadata.samplePosition);
            render(position, eventPosition - position);
            engine->handleMidi(metadata.getMessage());
            position = eventPosition;
        }
        render(position, buffer.getNumSamples() - position);
        activeVoices.store(engine->voices());
    } else {
        previewRequested.store(false);
        activeVoices.store(0);
    }
    midi.clear();
    juce::Reverb::Parameters parameters;
    parameters.roomSize = 0.65f;
    parameters.damping = 0.55f;
    parameters.wetLevel = reverbAmount->load() * 0.32f;
    parameters.dryLevel = 1.0f - reverbAmount->load() * 0.2f;
    parameters.width = 1;
    reverb.setParameters(parameters);
    if (buffer.getNumChannels() == 2)
        reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    else if (buffer.getNumChannels() == 1)
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
    volume.setTargetValue(juce::Decibels::decibelsToGain(volumeDb->load(), -60.0f));
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        const float gain = volume.getNextValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            float value = buffer.getSample(channel, i) * gain;
            buffer.setSample(channel, i, std::isfinite(value) ? value : 0.0f);
        }
    }
    updateScope(buffer);
}

void SoundWizardProcessor::updateScope(const juce::AudioBuffer<float>& buffer) {
    unsigned int position = scopePosition.load(std::memory_order_relaxed);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        if (++scopeDecimation < 4) continue;
        scopeDecimation = 0;
        float sample = buffer.getSample(0, i);
        if (buffer.getNumChannels() > 1) sample = (sample + buffer.getSample(1, i)) * 0.5f;
        scope[position++ % scope.size()].store(sample, std::memory_order_relaxed);
    }
    scopePosition.store(position, std::memory_order_release);
}

void SoundWizardProcessor::readScope(std::array<float, 512>& destination) const {
    const unsigned int position = scopePosition.load(std::memory_order_acquire);
    for (size_t i = 0; i < destination.size(); ++i)
        destination[i] = scope[(position + (unsigned int)i) % scope.size()].load(std::memory_order_relaxed);
}

void SoundWizardProcessor::getStateInformation(juce::MemoryBlock& data) {
    juce::ValueTree saved;
    {
        const juce::ScopedLock lock(infoMutex);
        saved = state.copyState();
        saved.setProperty("soundFontPath", info.loading ? pendingPath : info.path, nullptr);
        saved.setProperty("recentPaths", info.recentPaths.joinIntoString("\n"), nullptr);
        if (info.loading && !pendingRestore) {
            for (auto parameter : saved)
                if (parameter.getProperty("id").toString() == "instrument")
                    parameter.setProperty("value", 0, nullptr);
        }
    }
    auto xml = saved.createXml();
    copyXmlToBinary(*xml, data);
}

void SoundWizardProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(state.state.getType())) return;
    auto saved = juce::ValueTree::fromXml(*xml);
    // Projects saved before these controls existed retain polyphonic, zero-glide playback.
    for (const auto* id : {"mono", "portamento"}) {
        if (!saved.getChildWithProperty("id", id).isValid()) {
            juce::ValueTree parameter("PARAM");
            parameter.setProperty("id", id, nullptr);
            parameter.setProperty("value", 0.0f, nullptr);
            saved.addChild(parameter, -1, nullptr);
        }
    }
    state.replaceState(saved);
    {
        const juce::ScopedLock lock(infoMutex);
        info.recentPaths = juce::StringArray::fromLines(saved.getProperty("recentPaths").toString());
        info.recentPaths.removeEmptyStrings();
        while (info.recentPaths.size() > 8) info.recentPaths.remove(8);
    }
    requestLoad(saved.getProperty("soundFontPath").toString(), true);
}

juce::AudioProcessorEditor* SoundWizardProcessor::createEditor() { return new SoundWizardEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SoundWizardProcessor(); }
