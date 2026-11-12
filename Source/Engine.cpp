#include "Engine.h"
#define TSF_IMPLEMENTATION
#include "../ThirdParty/tsf.h"
#include <array>
#include <vector>

struct SoundFontEngine::Impl {
    struct HeldNote {
        unsigned int presses = 0;
        juce::uint64 order = 0;
        float velocity = 0;
        bool sustained = false;
    };
    struct VoiceGlide {
        unsigned int playIndex = 0;
        double offset = 0, step = 0;
        int samples = 0;
    };
    tsf* font = nullptr;
    int instrument = -1;
    bool mono = false;
    double sampleRate = 44100, portamento = 0;
    std::array<HeldNote, 16 * 128> heldNotes {};
    std::array<bool, 16> pedals {};
    juce::uint64 noteOrder = 0;
    int activeNote = -1, rootKey = -1, voiceChannel = -1;
    double pitch = 0, targetPitch = 0, pitchStep = 0;
    int glideSamples = 0;
    std::array<VoiceGlide, 128> polyGlides {};
    std::vector<std::vector<tsf_envelope>> originalEnvelopes;
    std::array<float, 512> scratch {};
    ~Impl() { if (font) tsf_close(font); }

    void silenceVoices() {
        for (int i = 0; i < font->voiceNum; ++i) font->voices[i].playingPreset = -1;
    }

    void applyPolyPitch(int index) {
        auto& voice = font->voices[index];
        const auto& channel = font->channels->channels[voice.playingChannel];
        const float bend = channel.pitchWheel == 8192 ? 0.0f
            : channel.pitchWheel / 16383.0f * channel.pitchRange * 2 - channel.pitchRange;
        tsf_voice_calcpitchratio(&voice, channel.tuning + bend + (float)polyGlides[(size_t)index].offset,
            font->outSampleRate);
    }

    void restorePolyPitch(int channel) {
        for (int i = 0; i < font->voiceNum; ++i) {
            const auto& voice = font->voices[i];
            const auto& glide = polyGlides[(size_t)i];
            if (voice.playingPreset >= 0 && voice.playingChannel == channel
                && glide.samples > 0 && glide.playIndex == voice.playIndex)
                applyPolyPitch(i);
        }
    }

    double heldPitch(int note) const {
        if (note < 0) return -1;
        unsigned int newest = 0;
        double result = -1;
        for (int i = 0; i < font->voiceNum; ++i) {
            const auto& voice = font->voices[i];
            if (voice.playingPreset < 0 || voice.playingChannel != note / 128
                || voice.playingKey != note % 128) continue;
            if (result < 0 || voice.playIndex >= newest) {
                const auto& glide = polyGlides[(size_t)i];
                result = voice.playingKey + (glide.playIndex == voice.playIndex ? glide.offset : 0);
                newest = voice.playIndex;
            }
        }
        return result;
    }

    void playPolyNote(int channel, int key, float velocity) {
        const double from = heldPitch(lastHeldNote());
        auto& held = heldNotes[(size_t)channel * 128 + (size_t)key];
        ++held.presses;
        held.sustained = false;
        held.velocity = velocity;
        held.order = ++noteOrder;
        const auto playIndex = font->voicePlayIndex;
        tsf_channel_note_on(font, channel, key, velocity);
        for (int i = 0; i < font->voiceNum; ++i) {
            const auto& voice = font->voices[i];
            if (voice.playingPreset < 0 || voice.playIndex != playIndex) continue;
            auto& glide = polyGlides[(size_t)i];
            glide = {};
            glide.playIndex = playIndex;
            if (from < 0 || portamento == 0 || from == key) continue;
            glide.samples = juce::jmax(1, juce::roundToInt(sampleRate * portamento));
            glide.offset = from - key;
            glide.step = -glide.offset / glide.samples;
            applyPolyPitch(i);
        }
    }

    int polyRenderCount(int length) {
        int count = juce::jmin(length, 256);
        for (int i = 0; i < font->voiceNum; ++i) {
            auto& glide = polyGlides[(size_t)i];
            if (glide.samples <= 0) continue;
            const auto& voice = font->voices[i];
            if (voice.playingPreset < 0 || voice.playIndex != glide.playIndex) {
                glide = {};
                continue;
            }
            // Limit tuning steps to one cent and at most 16 audio samples.
            const int step = (int)juce::jlimit(1.0, 16.0, std::floor(0.01 / std::abs(glide.step)));
            count = juce::jmin(count, juce::jmin(step, glide.samples));
        }
        return count;
    }

    void advancePolyGlides(int count) {
        for (int i = 0; i < font->voiceNum; ++i) {
            auto& glide = polyGlides[(size_t)i];
            if (glide.samples <= 0) continue;
            if (font->voices[i].playingPreset < 0) { glide = {}; continue; }
            glide.samples -= count;
            glide.offset = glide.samples > 0 ? glide.offset + glide.step * count : 0;
            applyPolyPitch(i);
        }
    }

    void applyPitch() {
        if (voiceChannel >= 0) tsf_channel_set_tuning(font, voiceChannel, (float)(pitch - rootKey));
    }

    void startGlide(int key, bool legato) {
        targetPitch = key;
        glideSamples = legato ? juce::roundToInt(sampleRate * portamento) : 0;
        if (glideSamples > 0) pitchStep = (targetPitch - pitch) / glideSamples;
        else pitch = targetPitch;
        applyPitch();
    }

    int lastHeldNote() const {
        int latest = -1;
        juce::uint64 latestOrder = 0;
        for (size_t i = 0; i < heldNotes.size(); ++i) {
            const auto& note = heldNotes[i];
            if ((note.presses > 0 || note.sustained) && note.order > latestOrder) {
                latest = (int)i;
                latestOrder = note.order;
            }
        }
        return latest;
    }

    void playMonoNote(int note) {
        const bool legato = activeNote >= 0 && tsf_active_voice_count(font) > 0;
        if (note == activeNote && legato) return;
        activeNote = note;
        if (!legato) {
            silenceVoices();
            if (voiceChannel >= 0) tsf_channel_set_tuning(font, voiceChannel, 0);
            voiceChannel = note / 128;
            rootKey = note % 128;
            startGlide(rootKey, false);
            tsf_channel_note_on(font, voiceChannel, rootKey, heldNotes[(size_t)note].velocity);
        } else {
            // Legato reuses the running sample layers and envelope for a continuous slide.
            startGlide(note % 128, true);
        }
    }

    void updateHeldNote() {
        const int next = lastHeldNote();
        if (next >= 0) {
            if (next != activeNote) playMonoNote(next);
        }
        else if (activeNote >= 0) {
            tsf_channel_note_off(font, voiceChannel, rootKey);
            activeNote = -1;
        }
    }

    void releasePedal(int channel) {
        pedals[(size_t)channel] = false;
        for (int key = 0; key < 128; ++key)
            heldNotes[(size_t)channel * 128 + (size_t)key].sustained = false;
        if (mono) updateHeldNote();
    }
};

SoundFontEngine::SoundFontEngine(std::unique_ptr<Impl> data) : impl(std::move(data)) {}
SoundFontEngine::~SoundFontEngine() = default;

std::unique_ptr<SoundFontEngine> SoundFontEngine::load(const void* data, int bytes) {
    if (bytes < 12 || std::memcmp(data, "RIFF", 4) != 0
        || std::memcmp(static_cast<const char*>(data) + 8, "sfbk", 4) != 0)
        return {};
    auto result = std::make_unique<Impl>();
    result->font = tsf_load_memory(data, bytes);
    if (!result->font || tsf_get_presetcount(result->font) == 0
        || !tsf_set_max_voices(result->font, 128))
        return {};
    auto* font = result->font;
    for (int i = 0; i < font->presetNum; ++i) {
        std::vector<tsf_envelope> envelopes;
        for (int j = 0; j < font->presets[i].regionNum; ++j)
            envelopes.push_back(font->presets[i].regions[j].ampenv);
        result->originalEnvelopes.push_back(std::move(envelopes));
    }
    for (int channel = 0; channel < 16; ++channel)
        tsf_channel_set_presetindex(font, channel, 0);
    auto engine = std::unique_ptr<SoundFontEngine>(new SoundFontEngine(std::move(result)));
    engine->setSampleRate(44100);
    engine->setInstrument(0);
    return engine;
}

juce::StringArray SoundFontEngine::instrumentNames() const {
    juce::StringArray names;
    for (int i = 0; i < impl->font->presetNum; ++i) {
        const auto& preset = impl->font->presets[i];
        names.add(juce::String::fromUTF8(preset.presetName, 20).trim()
            + "|" + juce::String(preset.bank) + ":" + juce::String(preset.preset));
    }
    return names;
}

void SoundFontEngine::setSampleRate(double rate) {
    impl->sampleRate = rate;
    tsf_set_output(impl->font, TSF_STEREO_INTERLEAVED, juce::roundToInt(rate), -6.0f);
}

void SoundFontEngine::stop() {
    auto* font = impl->font;
    // Keep preallocated channels and voices, so Panic never allocates on the audio thread.
    impl->silenceVoices();
    impl->heldNotes.fill({});
    impl->pedals.fill(false);
    impl->activeNote = impl->rootKey = impl->voiceChannel = -1;
    impl->glideSamples = 0;
    impl->polyGlides.fill({});
    impl->noteOrder = 0;
    for (int channel = 0; channel < 16; ++channel) {
        tsf_channel_set_sustain(font, channel, 0);
        tsf_channel_set_pitchwheel(font, channel, 8192);
        tsf_channel_set_tuning(font, channel, 0);
    }
}

void SoundFontEngine::setPlaybackMode(bool mono, float portamentoSeconds) {
    if (mono != impl->mono) {
        stop();
        impl->mono = mono;
    }
    const double seconds = juce::jlimit(0.0, 2.0, (double)portamentoSeconds);
    if (seconds != impl->portamento) {
        impl->portamento = seconds;
        if (impl->glideSamples > 0) impl->startGlide(juce::roundToInt(impl->targetPitch), true);
        for (int i = 0; i < impl->font->voiceNum; ++i) {
            auto& glide = impl->polyGlides[(size_t)i];
            const auto& voice = impl->font->voices[i];
            if (glide.samples <= 0) continue;
            if (voice.playingPreset < 0 || glide.playIndex != voice.playIndex) { glide = {}; continue; }
            glide.samples = juce::roundToInt(impl->sampleRate * seconds);
            if (glide.samples > 0) glide.step = -glide.offset / glide.samples;
            else { glide.offset = 0; impl->applyPolyPitch(i); }
        }
    }
}

void SoundFontEngine::setInstrument(int index) {
    index = juce::jlimit(0, impl->font->presetNum - 1, index);
    if (impl->instrument == index) return;
    stop();
    impl->instrument = index;
    for (int channel = 0; channel < 16; ++channel)
        tsf_channel_set_presetindex(impl->font, channel, index);
}

void SoundFontEngine::setEnvelope(bool custom, float attack, float decay, float sustain, float release) {
    auto& preset = impl->font->presets[impl->instrument];
    for (int i = 0; i < preset.regionNum; ++i) {
        auto& envelope = preset.regions[i].ampenv;
        envelope = impl->originalEnvelopes[(size_t)impl->instrument][(size_t)i];
        if (custom) {
            envelope.delay = envelope.hold = 0;
            envelope.keynumToHold = envelope.keynumToDecay = 0;
            envelope.attack = attack;
            envelope.decay = decay;
            envelope.sustain = sustain;
            envelope.release = release;
        }
    }
}

void SoundFontEngine::handleMidi(const juce::MidiMessage& message) {
    const int channel = juce::jlimit(0, 15, message.getChannel() - 1);
    if (impl->mono) {
        if (message.isNoteOn()) {
            const int note = channel * 128 + message.getNoteNumber();
            auto& held = impl->heldNotes[(size_t)note];
            ++held.presses;
            held.sustained = false;
            held.velocity = message.getFloatVelocity();
            held.order = ++impl->noteOrder;
            impl->playMonoNote(note);
            return;
        }
        if (message.isNoteOff()) {
            auto& held = impl->heldNotes[(size_t)channel * 128 + (size_t)message.getNoteNumber()];
            if (held.presses > 0) {
                --held.presses;
                held.sustained = held.presses == 0 && impl->pedals[(size_t)channel];
                impl->updateHeldNote();
            }
            return;
        }
        if (message.isAllSoundOff()) { stop(); return; }
        if (message.isAllNotesOff()) {
            for (int key = 0; key < 128; ++key) impl->heldNotes[(size_t)channel * 128 + (size_t)key] = {};
            impl->updateHeldNote();
            return;
        }
        if (message.isController() && message.getControllerNumber() == 64) {
            if (message.getControllerValue() >= 64) impl->pedals[(size_t)channel] = true;
            else impl->releasePedal(channel);
            return;
        }
        if (message.isController() && message.getControllerNumber() == 121) impl->releasePedal(channel);
    }
    const int playbackChannel = impl->mono && impl->voiceChannel >= 0 ? impl->voiceChannel : channel;
    if (message.isNoteOn())
        impl->playPolyNote(channel, message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff()) {
        auto& held = impl->heldNotes[(size_t)channel * 128 + (size_t)message.getNoteNumber()];
        if (held.presses > 0) {
            --held.presses;
            held.sustained = held.presses == 0 && impl->pedals[(size_t)channel];
        }
        tsf_channel_note_off(impl->font, channel, message.getNoteNumber());
    }
    else if (message.isPitchWheel()) {
        tsf_channel_set_pitchwheel(impl->font, playbackChannel, message.getPitchWheelValue());
        if (!impl->mono) impl->restorePolyPitch(channel);
    }
    else if (message.isAllSoundOff() || message.isAllNotesOff()) {
        for (int key = 0; key < 128; ++key) impl->heldNotes[(size_t)channel * 128 + (size_t)key] = {};
        if (message.isAllSoundOff()) tsf_channel_sounds_off_all(impl->font, channel);
        else tsf_channel_note_off_all(impl->font, channel);
    }
    else if (message.isController()) {
        const int control = message.getControllerNumber();
        if (!impl->mono) {
            if (control == 64 && message.getControllerValue() >= 64) impl->pedals[(size_t)channel] = true;
            else if (control == 64 || control == 121) impl->releasePedal(channel);
        }
        // Instrument selection belongs to the library. Ignore MIDI bank changes.
        if (control != 0 && control != 32)
            tsf_channel_midi_control(impl->font, playbackChannel, control, message.getControllerValue());
        if (impl->mono) impl->applyPitch();
        else impl->restorePolyPitch(channel);
    }
}

void SoundFontEngine::render(juce::AudioBuffer<float>& output, int start, int length) {
    while (length > 0) {
        // Update tuning every sample during a glide, independent of host block size.
        const int count = impl->mono ? (impl->glideSamples > 0 ? 1 : juce::jmin(length, 256))
                                     : impl->polyRenderCount(length);
        tsf_render_float(impl->font, impl->scratch.data(), count, 0);
        for (int i = 0; i < count; ++i) {
            const float left = impl->scratch[(size_t)i * 2];
            const float right = impl->scratch[(size_t)i * 2 + 1];
            if (output.getNumChannels() == 1)
                output.setSample(0, start + i, (left + right) * 0.5f);
            else {
                output.setSample(0, start + i, left);
                output.setSample(1, start + i, right);
            }
        }
        start += count;
        length -= count;
        if (impl->glideSamples > 0) {
            impl->pitch += impl->pitchStep;
            if (--impl->glideSamples == 0) impl->pitch = impl->targetPitch;
            impl->applyPitch();
        }
        if (!impl->mono) impl->advancePolyGlides(count);
    }
}

int SoundFontEngine::voices() const { return tsf_active_voice_count(impl->font); }
