#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"
#include <array>

class SoundWizardProcessor final : public juce::AudioProcessor, private juce::Thread {
public:
    struct LibraryInfo {
        juce::String name, path, message;
        juce::StringArray instruments, recentPaths;
        bool loading = false;
        int revision = 0;
    };
    SoundWizardProcessor();
    ~SoundWizardProcessor() override;
    const juce::String getName() const override { return "Sound Wizard"; }
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    double getTailLengthSeconds() const override { return 15.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void loadSoundFont(const juce::File& file);
    void loadStarterBank();
    LibraryInfo getLibraryInfo() const;
    void selectInstrument(int index);
    int selectedInstrument() const;
    void preview() { previewRequested.store(true); }
    void panic();
    void readScope(std::array<float, 512>& destination) const;
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::atomic<int> activeVoices {0};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void requestLoad(juce::String path, bool restore);
    void run() override;
    void updateScope(const juce::AudioBuffer<float>&);
    mutable juce::CriticalSection infoMutex;
    juce::CriticalSection engineMutex;
    LibraryInfo info;
    std::unique_ptr<SoundFontEngine> engine;
    juce::WaitableEvent loadEvent;
    juce::String pendingPath;
    bool pending = false, pendingRestore = false;
    int requestVersion = 0;
    std::atomic<double> sampleRate {44100};
    std::atomic<bool> previewRequested {false}, panicRequested {false};
    int previewSamples = 0;
    juce::Reverb reverb;
    juce::SmoothedValue<float> volume;
    std::array<std::atomic<float>, 512> scope;
    std::atomic<unsigned int> scopePosition {0};
    unsigned int scopeDecimation = 0;
    std::atomic<float>* program = nullptr;
    std::atomic<float>* customEnvelope = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* decay = nullptr;
    std::atomic<float>* sustain = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* reverbAmount = nullptr;
    std::atomic<float>* volumeDb = nullptr;
    std::atomic<float>* mono = nullptr;
    std::atomic<float>* portamento = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundWizardProcessor)
};
