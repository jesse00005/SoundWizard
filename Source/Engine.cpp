#include "Engine.h"
#define TSF_IMPLEMENTATION
#include "../ThirdParty/tsf.h"
#include <array>
#include <vector>

struct SoundFontEngine::Impl {
    tsf* font = nullptr;
    int instrument = -1;
    std::vector<std::vector<tsf_envelope>> originalEnvelopes;
    std::array<float, 512> scratch {};
    ~Impl() { if (font) tsf_close(font); }
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
    tsf_set_output(impl->font, TSF_STEREO_INTERLEAVED, juce::roundToInt(rate), -6.0f);
}

void SoundFontEngine::stop() {
    auto* font = impl->font;
    // Keep preallocated channels and voices, so Panic never allocates on the audio thread.
    for (int i = 0; i < font->voiceNum; ++i) font->voices[i].playingPreset = -1;
    for (int channel = 0; channel < 16; ++channel) {
        tsf_channel_set_sustain(font, channel, 0);
        tsf_channel_set_pitchwheel(font, channel, 8192);
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
    if (message.isNoteOn())
        tsf_channel_note_on(impl->font, channel, message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        tsf_channel_note_off(impl->font, channel, message.getNoteNumber());
    else if (message.isPitchWheel())
        tsf_channel_set_pitchwheel(impl->font, channel, message.getPitchWheelValue());
    else if (message.isAllSoundOff())
        tsf_channel_sounds_off_all(impl->font, channel);
    else if (message.isAllNotesOff())
        tsf_channel_note_off_all(impl->font, channel);
    else if (message.isController()) {
        const int control = message.getControllerNumber();
        // Instrument selection belongs to the library. Ignore MIDI bank changes.
        if (control != 0 && control != 32)
            tsf_channel_midi_control(impl->font, channel, control, message.getControllerValue());
    }
}

void SoundFontEngine::render(juce::AudioBuffer<float>& output, int start, int length) {
    while (length > 0) {
        const int count = juce::jmin(length, 256);
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
    }
}

int SoundFontEngine::voices() const { return tsf_active_voice_count(impl->font); }
