#pragma once
#include "Processor.h"

class CobaltLook final : public juce::LookAndFeel_V4 {
public:
    CobaltLook();
    static void bevel(juce::Graphics&, juce::Rectangle<int>, juce::Colour, bool inset = false);
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float,
        juce::Slider::SliderStyle, juce::Slider&) override;
    bool areScrollbarButtonsVisible() override { return true; }
    int getDefaultScrollbarWidth() override { return 20; }
    int getMinimumScrollbarThumbSize(juce::ScrollBar&) override { return 20; }
    int getScrollbarButtonSize(juce::ScrollBar&) override { return 20; }
    void drawScrollbarButton(juce::Graphics&, juce::ScrollBar&, int, int, int, bool, bool, bool) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int, int, int, int, bool, int, int, bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void fillTextEditorBackground(juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
};

class WizardKeyboard final : public juce::MidiKeyboardComponent {
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;
    void drawBlackNote(int, juce::Graphics&, juce::Rectangle<float>, bool, bool, juce::Colour) override;
};

class SoundFontsLibraryBox final : public juce::ComboBox {
public:
    explicit SoundFontsLibraryBox(juce::File folder = juce::File::getSpecialLocation(
        juce::File::userMusicDirectory).getChildFile("soundfonts"));
    void refreshFiles();
    void selectFile(const juce::File&);
    void showPopup() override;
    std::function<void(const juce::File&)> onFileChosen;
private:
    const juce::File libraryFolder;
    juce::File currentFile;
    juce::Array<juce::File> files;
};

class SoundWizardEditor final : public juce::AudioProcessorEditor,
    private juce::Timer, private juce::ListBoxModel, public juce::FileDragAndDropTarget {
public:
    explicit SoundWizardEditor(SoundWizardProcessor&);
    ~SoundWizardEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
private:
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override;
    void selectedRowsChanged(int) override;
    void timerCallback() override;
    void openFile();
    void refreshLibrary();
    void filterLibrary();
    SoundWizardProcessor& processor;
    CobaltLook look;
    juce::TextButton load {"Load SF2..."}, starter {"Starter bank"}, preview {"Preview C4"}, stop {"Panic"};
    juce::TextEditor search;
    SoundFontsLibraryBox soundFonts;
    juce::ListBox library {"Instruments", this};
    juce::ToggleButton custom {"Use custom ADSR"};
    std::array<juce::Slider, 6> sliders;
    std::array<juce::Label, 6> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 6> attachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> customAttachment;
    WizardKeyboard keys;
    juce::TooltipWindow tooltips {this, 600};
    std::unique_ptr<juce::FileChooser> chooser;
    SoundWizardProcessor::LibraryInfo info;
    std::vector<int> filtered;
    std::array<float, 512> waveform {};
    int lastRevision = -1, lastSelected = -1;
    bool updatingSelection = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoundWizardEditor)
};
