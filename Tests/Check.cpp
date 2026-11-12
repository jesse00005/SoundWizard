#include "Processor.h"
#include "Editor.h"
#include <iostream>
#include <stdexcept>
#include <cmath>

static void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static void set(SoundWizardProcessor& processor, const char* id, float value) {
    auto* parameter = processor.state.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static void waitForLoad(SoundWizardProcessor& processor) {
    const auto deadline = juce::Time::getMillisecondCounter() + 10000;
    while (processor.getLibraryInfo().loading && juce::Time::getMillisecondCounter() < deadline)
        juce::Thread::sleep(10);
    check(!processor.getLibraryInfo().loading, "SoundFont load timed out");
}
static void snapshot(juce::Component& editor, const juce::File& file) {
    auto stream = file.createOutputStream();
    check(stream != nullptr, "Cannot create preview file");
    stream->setPosition(0); stream->truncate();
    juce::PNGImageFormat png;
    check(png.writeImageToStream(editor.createComponentSnapshot(editor.getLocalBounds()), *stream), "Cannot render UI");
}
static double energy(const juce::AudioBuffer<float>& buffer) {
    double sum = 0;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const float value = buffer.getSample(channel, i);
            check(std::isfinite(value) && std::abs(value) < 4, "Invalid audio output");
            sum += value * value;
        }
    return sum;
}

static double frequency(const juce::AudioBuffer<float>& audio, double rate, double startSeconds, double duration, int channel = 0) {
    const int start = juce::roundToInt(startSeconds * rate);
    const int end = juce::jmin(audio.getNumSamples(), start + juce::roundToInt(duration * rate));
    double first = 0, last = 0;
    int crossings = 0;
    for (int i = start + 1; i < end; ++i) {
        const float previous = audio.getSample(channel, i - 1), current = audio.getSample(channel, i);
        if (previous <= 0 && current > 0) {
            const double position = i - 1 + (double)(-previous) / (current - previous);
            if (crossings++ == 0) first = position;
            last = position;
        }
    }
    check(crossings >= 3, "Insufficient waveform periods for a pitch check");
    return (crossings - 1) * rate / (last - first);
}

static std::unique_ptr<SoundFontEngine> monoEngine(const juce::File& factory, double rate) {
    juce::MemoryBlock bytes;
    check(factory.loadFileAsData(bytes), "Cannot read mono test SoundFont");
    auto engine = SoundFontEngine::load(bytes.getData(), (int)bytes.getSize());
    check(engine != nullptr, "Cannot load mono test SoundFont");
    engine->setSampleRate(rate);
    engine->setEnvelope(true, 0.001f, 0.001f, 1, 0.005f);
    engine->setPlaybackMode(true, 0.4f);
    return engine;
}

static void checkMono(const juce::File& factory, double rate) {
    auto engine = monoEngine(factory, rate);
    juce::AudioBuffer<float> warmup(2, juce::roundToInt(rate * 0.1));
    engine->handleMidi(juce::MidiMessage::noteOn(1, 48, 0.8f));
    engine->render(warmup, 0, warmup.getNumSamples());
    engine->handleMidi(juce::MidiMessage::noteOn(2, 60, 0.8f));
    check(engine->voices() == 1, "Mono overlap started a second note");
    juce::AudioBuffer<float> glide(2, juce::roundToInt(rate * 0.5));
    engine->render(glide, 0, glide.getNumSamples());
    const double early = frequency(glide, rate, 0, 0.04);
    const double middle = frequency(glide, rate, 0.18, 0.04);
    const double late = frequency(glide, rate, 0.44, 0.04);
    check(early > 128 && early < 146, "Portamento jumped to the new pitch");
    check(middle > 177 && middle < 193, "Portamento midpoint timing is wrong");
    check(std::abs(late - 261.625) < 2, "Portamento did not reach the target pitch");
    engine->handleMidi(juce::MidiMessage::pitchWheel(2, 16383));
    engine->render(glide, 0, glide.getNumSamples());
    check(std::abs(frequency(glide, rate, 0.4, 0.05) - 293.66) < 2, "Pitch bend did not combine with glide tuning");
    engine->handleMidi(juce::MidiMessage::pitchWheel(2, 8192));
    engine->handleMidi(juce::MidiMessage::noteOff(2, 60));
    engine->render(glide, 0, glide.getNumSamples());
    check(std::abs(frequency(glide, rate, 0.44, 0.04) - 130.8125) < 2, "Last-note priority did not return to the held note");
    engine->handleMidi(juce::MidiMessage::noteOff(1, 48));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(warmup.getMagnitude(warmup.getNumSamples() / 2, warmup.getNumSamples() / 2) < 1e-6f, "Mono note-off left a stuck note");

    engine->handleMidi(juce::MidiMessage::noteOn(1, 48, 0.8f));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(std::abs(frequency(warmup, rate, 0.04, 0.04) - 130.8125) < 2, "A detached note should start at its own pitch");
    engine->handleMidi(juce::MidiMessage::noteOn(1, 60, 0.8f));
    engine->setPlaybackMode(true, 0);
    engine->render(warmup, 0, warmup.getNumSamples());
    check(std::abs(frequency(warmup, rate, 0.04, 0.04) - 261.625) < 2, "Zero portamento did not snap to pitch");
    engine->handleMidi(juce::MidiMessage::controllerEvent(1, 64, 127));
    engine->handleMidi(juce::MidiMessage::noteOff(1, 60));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(engine->voices() == 1, "Mono sustain pedal released the note too soon");
    engine->handleMidi(juce::MidiMessage::controllerEvent(1, 64, 0));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(std::abs(frequency(warmup, rate, 0.04, 0.04) - 130.8125) < 2, "Pedal release did not return to a physically held key");
    engine->handleMidi(juce::MidiMessage::noteOff(1, 48));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(engine->voices() == 0, "Mono sustain left a stuck voice");

    engine->handleMidi(juce::MidiMessage::noteOn(1, 60, 0.8f));
    engine->handleMidi(juce::MidiMessage::noteOn(1, 60, 0.8f));
    engine->handleMidi(juce::MidiMessage::noteOff(1, 60));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(engine->voices() == 1, "Repeated mono keys were released prematurely");
    engine->handleMidi(juce::MidiMessage::noteOff(1, 60));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(engine->voices() == 0, "Repeated mono keys left a stuck voice");
    engine->setPlaybackMode(false, 0.4f);
    engine->handleMidi(juce::MidiMessage::noteOn(1, 60, 0.8f));
    engine->handleMidi(juce::MidiMessage::noteOn(1, 64, 0.8f));
    check(engine->voices() == 2, "Switching back to poly did not restore chords");
    engine->setPlaybackMode(true, 0.4f);
    check(engine->voices() == 0, "Switching modes did not reset existing voices");
    engine->handleMidi(juce::MidiMessage::noteOn(1, 48, 0.8f));
    engine->handleMidi(juce::MidiMessage::noteOn(1, 72, 0.8f));
    engine->stop();
    engine->render(warmup, 0, warmup.getNumSamples());
    check(energy(warmup) == 0, "Panic did not cancel a glide");
}

static void checkPolyGlide(const juce::File& factory, double rate) {
    auto engine = monoEngine(factory, rate);
    engine->setPlaybackMode(false, 0.4f);
    engine->handleMidi(juce::MidiMessage::controllerEvent(1, 10, 0));
    engine->handleMidi(juce::MidiMessage::controllerEvent(2, 10, 127));
    engine->handleMidi(juce::MidiMessage::noteOn(1, 48, 0.8f));
    juce::AudioBuffer<float> warmup(2, juce::roundToInt(rate * 0.1));
    engine->render(warmup, 0, warmup.getNumSamples());
    engine->handleMidi(juce::MidiMessage::noteOn(2, 60, 0.8f));
    check(engine->voices() == 2, "Poly portamento removed the earlier note");
    juce::AudioBuffer<float> audio(2, juce::roundToInt(rate * 0.5));
    engine->render(audio, 0, audio.getNumSamples());
    check(frequency(audio, rate, 0, 0.04, 1) < 146, "Poly note did not glide from the held pitch");
    check(std::abs(frequency(audio, rate, 0.18, 0.04, 1) - 185) < 8, "Poly glide timing is wrong");
    check(std::abs(frequency(audio, rate, 0.44, 0.04, 1) - 261.625) < 2, "Poly glide missed its target");
    check(std::abs(frequency(audio, rate, 0.18, 0.04, 0) - 130.8125) < 2, "A new poly glide changed an earlier note's pitch");
    engine->handleMidi(juce::MidiMessage::pitchWheel(2, 16383));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(std::abs(frequency(warmup, rate, 0.04, 0.04, 1) - 293.66) < 2, "Poly pitch bend is incorrect");
    check(std::abs(frequency(warmup, rate, 0.04, 0.04, 0) - 130.8125) < 2, "Poly pitch bend affected another MIDI channel");
    engine->handleMidi(juce::MidiMessage::noteOff(2, 60));
    engine->handleMidi(juce::MidiMessage::noteOff(1, 48));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(engine->voices() == 0, "Poly glide left stuck notes");
    engine->handleMidi(juce::MidiMessage::pitchWheel(2, 8192));
    engine->handleMidi(juce::MidiMessage::noteOn(2, 72, 0.8f));
    engine->render(warmup, 0, warmup.getNumSamples());
    check(std::abs(frequency(warmup, rate, 0.04, 0.04, 1) - 523.25) < 2, "Detached poly note inherited an old glide");
    engine->stop();
    engine->handleMidi(juce::MidiMessage::noteOn(1, 48, 0.8f));
    engine->handleMidi(juce::MidiMessage::noteOn(2, 60, 0.8f));
    engine->render(warmup, 0, warmup.getNumSamples());
    engine->handleMidi(juce::MidiMessage::pitchWheel(2, 16383));
    engine->render(warmup, 0, warmup.getNumSamples());
    const double bentMidpoint = frequency(warmup, rate, 0.03, 0.04, 1);
    check(bentMidpoint > 180 && bentMidpoint < 211, "Pitch bend interrupted an active poly glide");
    engine->setPlaybackMode(false, 0);
    engine->render(warmup, 0, warmup.getNumSamples());
    check(std::abs(frequency(warmup, rate, 0.04, 0.04, 1) - 293.66) < 2, "Disabling Porta did not finish an active poly glide");
    engine->stop();
    engine->render(warmup, 0, warmup.getNumSamples());
    check(energy(warmup) == 0, "Panic did not silence poly portamento");
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialise;
    try {
        const auto directory = argc > 1 ? juce::File(argv[1]) : juce::File::getCurrentWorkingDirectory();
        directory.createDirectory();
        const auto factory = juce::File(__FILE__).getParentDirectory().getSiblingFile("Assets").getChildFile("WizardStarter.sf2");
        check(factory.existsAsFile(), "Factory bank missing");
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            checkMono(factory, rate);
            checkPolyGlide(factory, rate);
        }
        {
            SoundWizardProcessor monoProcessor;
            set(monoProcessor, "mono", 1); set(monoProcessor, "portamento", 0.4f);
            set(monoProcessor, "customEnvelope", 1); set(monoProcessor, "attack", 0.001f);
            set(monoProcessor, "decay", 0.001f); set(monoProcessor, "sustain", 1);
            set(monoProcessor, "reverb", 0);
            monoProcessor.prepareToPlay(48000, 128);
            juce::AudioBuffer<float> audio(2, 33600); juce::MidiBuffer events;
            events.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8f), 0);
            events.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 4800);
            monoProcessor.processBlock(audio, events);
            check(monoProcessor.activeVoices.load() == 1, "Processor Mono control did not limit overlapping notes");
            check(frequency(audio, 48000, 0.11, 0.04) < 150, "Processor Porta control did not start a timed glide");
            check(std::abs(frequency(audio, 48000, 0.6, 0.04) - 261.625) < 2, "Processor glide missed the target pitch");
        }
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            for (int channels : {1, 2}) {
                SoundWizardProcessor processor;
                check(processor.getLibraryInfo().instruments.size() == 4, "Starter presets not loaded");
                auto layout = processor.getBusesLayout();
                layout.outputBuses.set(0, channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
                check(processor.setBusesLayout(layout), "Output layout rejected");
                set(processor, "reverb", 0);
                processor.prepareToPlay(rate, 128);
                for (int preset = 0; preset < 4; ++preset) {
                    processor.selectInstrument(preset);
                    juce::AudioBuffer<float> output(channels, 2048);
                    juce::MidiBuffer midi;
                    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 128);
                    midi.addEvent(juce::MidiMessage::noteOn(2, 64, 0.65f), 256);
                    midi.addEvent(juce::MidiMessage::pitchWheel(2, 10000), 512);
                    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 768);
                    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 1024);
                    processor.processBlock(output, midi);
                    check(output.getMagnitude(0, 128) == 0, "MIDI sample offsets were ignored");
                    check(energy(output) > 0.001, "SoundFont preset produced no audio");
                    check(processor.activeVoices.load() > 0, "Voice count missing");
                    processor.panic(); processor.processBlock(output, midi);
                    check(energy(output) == 0 && processor.activeVoices.load() == 0, "Panic left stuck notes or a reverb tail");
                }
            }
        }

        SoundWizardProcessor processor;
        for (auto* parameter : processor.getParameters()) check(parameter->isAutomatable(), "Parameter cannot be automated");
        processor.loadSoundFont(factory); waitForLoad(processor);
        check(processor.getLibraryInfo().path == factory.getFullPathName(), "External SF2 did not load");
        check(processor.getLibraryInfo().recentPaths.size() == 1, "Recent library entry missing");
        processor.selectInstrument(2);
        set(processor, "customEnvelope", 1); set(processor, "attack", 0.03f);
        set(processor, "decay", 0.25f); set(processor, "sustain", 0.45f);
        set(processor, "release", 0.2f); set(processor, "reverb", 0.3f); set(processor, "volume", -12);
        set(processor, "mono", 1); set(processor, "portamento", 0.375f);
        juce::MemoryBlock saved;
        processor.getStateInformation(saved);
        SoundWizardProcessor restored;
        restored.setStateInformation(saved.getData(), (int)saved.getSize()); waitForLoad(restored);
        check(restored.getLibraryInfo().path == factory.getFullPathName(), "SoundFont path recall failed");
        for (int i = 0; i < processor.getParameters().size(); ++i)
            check(std::abs(processor.getParameters()[i]->getValue() - restored.getParameters()[i]->getValue()) < 1e-6f, "Parameter recall failed");
        const char malformed[] = "bad state";
        restored.setStateInformation(malformed, sizeof(malformed));
        check(restored.selectedInstrument() == 2, "Malformed state changed the instrument");
        auto legacy = processor.state.copyState();
        legacy.removeChild(legacy.getChildWithProperty("id", "mono"), nullptr);
        legacy.removeChild(legacy.getChildWithProperty("id", "portamento"), nullptr);
        auto legacyXml = legacy.createXml(); juce::MemoryBlock legacyBytes;
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyBytes);
        restored.setStateInformation(legacyBytes.getData(), (int)legacyBytes.getSize()); waitForLoad(restored);
        check(restored.state.getRawParameterValue("mono")->load() == 0
            && restored.state.getRawParameterValue("portamento")->load() == 0, "Old projects did not restore poly mode with no glide");

        const auto invalid = directory.getChildFile("invalid-test.sf2");
        invalid.replaceWithText("invalid SoundFont");
        restored.loadSoundFont(invalid); waitForLoad(restored);
        check(restored.getLibraryInfo().instruments.size() == 4, "Failed manual load removed the working bank");
        check(restored.selectedInstrument() == 2, "Failed manual load changed the working instrument");
        check(restored.getLibraryInfo().message.contains("Could not"), "Invalid SF2 error is missing");
        auto missingState = processor.state.copyState();
        missingState.setProperty("soundFontPath", directory.getChildFile("missing-bank.sf2").getFullPathName(), nullptr);
        auto xml = missingState.createXml(); juce::MemoryBlock missing;
        juce::AudioProcessor::copyXmlToBinary(*xml, missing);
        restored.setStateInformation(missing.getData(), (int)missing.getSize()); waitForLoad(restored);
        check(restored.getLibraryInfo().instruments.isEmpty(), "Missing restored file played the wrong bank");
        check(restored.getLibraryInfo().message.contains("missing"), "Missing file feedback is absent");
        invalid.deleteFile();

        // Consecutive requests must publish only the last requested bank.
        restored.loadSoundFont(factory); restored.loadStarterBank(); waitForLoad(restored);
        check(restored.getLibraryInfo().path.isEmpty() && restored.getLibraryInfo().instruments.size() == 4, "Load request ordering failed");

        // The custom per-voice envelope must release each note independently.
        SoundWizardProcessor envelope;
        set(envelope, "customEnvelope", 1); set(envelope, "attack", 0.001f);
        set(envelope, "decay", 0.001f); set(envelope, "sustain", 1);
        set(envelope, "release", 0.005f); set(envelope, "reverb", 0);
        envelope.prepareToPlay(48000, 256);
        juce::AudioBuffer<float> block(2, 4800); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 67, 0.8f), 0);
        envelope.processBlock(block, midi); check(energy(block) > 0.1, "Custom envelope produced no notes");
        midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        envelope.processBlock(block, midi); check(energy(block) > 0.1, "One note-off stopped the whole chord");
        midi.addEvent(juce::MidiMessage::noteOff(1, 67), 0);
        envelope.processBlock(block, midi);
        check(block.getMagnitude(4000, 800) < 1e-6f, "Custom release did not end the last note");

        SoundWizardProcessor visual;
        visual.prepareToPlay(44100, 256);
        visual.keyboard.noteOn(1, 60, 0.8f);
        juce::AudioBuffer<float> audio(2, 256);
        for (int i = 0; i < 30; ++i) visual.processBlock(audio, midi);
        std::array<float, 512> scope;
        visual.readScope(scope);
        float peak = 0; for (float sample : scope) peak = juce::jmax(peak, std::abs(sample));
        check(peak > 0.001f, "Waveform display received no audio");
        auto editor = std::unique_ptr<juce::AudioProcessorEditor>(visual.createEditor());
        snapshot(*editor, directory.getChildFile("Sound-Wizard-preview.png"));
        auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*>(editor.get());
        check(dropTarget && dropTarget->isInterestedInFileDrag({factory.getFullPathName()}), "SF2 drag and drop is disconnected");
        bool volumeFound = false;
        for (auto* child : editor->getChildren()) {
            if (auto* slider = dynamic_cast<juce::Slider*>(child)) if (slider->getName() == "VOLUME") {
                slider->setValue(-17, juce::sendNotificationSync); volumeFound = true;
                check(std::abs(visual.state.getRawParameterValue("volume")->load() + 17) < 0.01f, "Volume slider attachment failed");
            }
        }
        check(volumeFound, "Volume control missing");

        set(visual, "customEnvelope", 1);
        set(visual, "attack", 0.15f);
        set(visual, "release", 1.2f);
        set(visual, "mono", 1);
        set(visual, "portamento", 0.25f);
        auto envelopeEditor = std::unique_ptr<juce::AudioProcessorEditor>(visual.createEditor());
        snapshot(*envelopeEditor, directory.getChildFile("Sound-Wizard-envelope.png"));
        bool monoFound = false, portaFound = false;
        for (auto* child : envelopeEditor->getChildren()) {
            if (auto* toggle = dynamic_cast<juce::ToggleButton*>(child)) if (toggle->getName() == "MONO") {
                toggle->setToggleState(false, juce::sendNotificationSync); monoFound = true;
                check(visual.state.getRawParameterValue("mono")->load() == 0, "Mono toggle attachment failed");
            }
            if (auto* slider = dynamic_cast<juce::Slider*>(child)) if (slider->getName() == "PORTA") {
                check(slider->isEnabled(), "Porta must remain enabled with Mono off");
                slider->setValue(0.45, juce::sendNotificationSync); portaFound = true;
                check(std::abs(visual.state.getRawParameterValue("portamento")->load() - 0.45f) < 0.001f, "Porta slider attachment failed");
            }
        }
        check(monoFound && portaFound, "Mono or Porta control missing");

        // Exercise folder discovery and the same dropdown callback used by the editor.
        const auto libraryFolder = directory.getNonexistentChildFile("soundfonts-library-test", "", false);
        check(libraryFolder.createDirectory().wasOk(), "Cannot create library test folder");
        const auto nestedFolder = libraryFolder.getChildFile("Subfolder");
        check(nestedFolder.createDirectory().wasOk(), "Cannot create library subfolder");
        const auto firstFont = libraryFolder.getChildFile("A bank.SF2");
        const auto secondFont = nestedFolder.getChildFile("B bank.sf2");
        const auto ignored = libraryFolder.getChildFile("notes.txt");
        check(factory.copyFileTo(firstFont) && factory.copyFileTo(secondFont), "Cannot create library fixtures");
        ignored.replaceWithText("This file must not appear in the dropdown.");
        SoundFontsLibraryBox libraryBox(libraryFolder);
        check(libraryBox.getNumItems() == 2, "Library missed nested or uppercase SF2 files");
        check(libraryBox.getItemText(0) == "A bank.SF2", "Library ordering is wrong");
        libraryBox.onFileChosen = [&processor](const juce::File& file) { processor.loadSoundFont(file); };
        libraryBox.setSelectedId(1, juce::sendNotificationSync);
        waitForLoad(processor);
        check(processor.getLibraryInfo().path == firstFont.getFullPathName(), "Dropdown did not load its chosen SoundFont");
        check(secondFont.deleteFile(), "Cannot remove library fixture");
        libraryBox.refreshFiles();
        check(libraryBox.getNumItems() == 1 && libraryBox.getSelectedId() == 1, "Library refresh lost its valid selection");
        check(firstFont.deleteFile(), "Cannot remove library fixture");
        libraryBox.refreshFiles();
        check(libraryBox.getSelectedId() == 0 && !libraryBox.isItemEnabled(1), "Empty library should offer disabled guidance");
        check(ignored.deleteFile() && nestedFolder.deleteFile() && libraryFolder.deleteFile(), "Cannot clean library fixtures");
        SoundFontsLibraryBox defaultLibrary;
        const auto expectedFolder = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("soundfonts");
        check(defaultLibrary.getTooltip().contains(expectedFolder.getFullPathName()), "Default library path is not resolved for the current Windows user");

        juce::AudioBuffer<float> audition(2, 44100 * 8);
        SoundWizardProcessor demo;
        for (int preset = 0; preset < 4; ++preset) {
            demo.selectInstrument(preset); demo.prepareToPlay(44100, 256);
            for (int offset = 0; offset < 88200; offset += 256) {
                const int count = juce::jmin(256, 88200 - offset);
                juce::AudioBuffer<float> output(2, count);
                juce::MidiBuffer events;
                if (offset == 0) for (int note : {60, 64, 67}) events.addEvent(juce::MidiMessage::noteOn(1, note, 0.7f), 0);
                if (offset == 44032) for (int note : {60, 64, 67}) events.addEvent(juce::MidiMessage::noteOff(1, note), 0);
                demo.processBlock(output, events);
                for (int channel = 0; channel < 2; ++channel) audition.copyFrom(channel, preset * 88200 + offset, output, channel, 0, count);
            }
        }
        check(audition.getMagnitude(0, audition.getNumSamples()) < 1, "Audition would clip on export");
        auto stream = directory.getChildFile("Sound-Wizard-audition.wav").createOutputStream();
        check(stream != nullptr, "Cannot write audition"); stream->setPosition(0); stream->truncate();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 44100, 2, 24, {}, 0));
        check(writer && writer->writeFromAudioSampleBuffer(audition, 0, audition.getNumSamples()), "Audition export failed");
        std::cout << "PASS: all four SF2 presets, mono/stereo at three rates, sample-accurate MIDI, pitch/sustain, panic, custom polyphonic ADSR, automation, state and external file recall, invalid/missing SF2, load ordering, real waveform, UI attachment, mono last-note priority, independent polyphonic glide, timed glide, pitch bend during glide, sustain and repeated mono keys, legacy project defaults, per-user folder library, nested SF2 discovery, dropdown loading, refresh, empty library, preview and audition.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
