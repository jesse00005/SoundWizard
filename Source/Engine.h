#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

class SoundFontEngine {
public:
    static std::unique_ptr<SoundFontEngine> load(const void* data, int bytes);
    ~SoundFontEngine();
    juce::StringArray instrumentNames() const;
    void setSampleRate(double rate);
    void setInstrument(int index);
    void setEnvelope(bool custom, float attack, float decay, float sustain, float release);
    void handleMidi(const juce::MidiMessage& message);
    void render(juce::AudioBuffer<float>& output, int start, int length);
    void stop();
    int voices() const;
private:
    struct Impl;
    explicit SoundFontEngine(std::unique_ptr<Impl>);
    std::unique_ptr<Impl> impl;
};
