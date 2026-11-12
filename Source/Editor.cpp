#include "Editor.h"
#include <algorithm>
#include <cmath>

namespace {
const juce::Colour cobalt(0xff0b0bff), panel(0xff2626ff), navy(0xff090957);
const juce::Colour highlight(0xff9292ff), shadow(0xff07077d), ink(0xff05052e);
const juce::Colour pale(0xffededff), green(0xff78ff80), waveformRed(0xffff1726);
juce::Font uiFont(float size = 15.0f, bool bold = false) {
    return juce::Font(juce::FontOptions("Arial", size, bold ? juce::Font::bold : juce::Font::plain));
}
juce::Font monoFont(float size = 15.0f) {
    return juce::Font(juce::FontOptions("Courier New", size, juce::Font::plain));
}
void pixelArrow(juce::Graphics& g, juce::Rectangle<int> bounds, int direction) {
    const auto centre = bounds.getCentre();
    for (int row = 0; row < 4; ++row) {
        const int extent = row * 2 + 1;
        if (direction == 0) g.fillRect(centre.x - row, centre.y - 2 + row, extent, 1);
        if (direction == 2) g.fillRect(centre.x - row, centre.y + 2 - row, extent, 1);
        if (direction == 1) g.fillRect(centre.x + 2 - row, centre.y - row, 1, extent);
        if (direction == 3) g.fillRect(centre.x - 2 + row, centre.y - row, 1, extent);
    }
}

}

CobaltLook::CobaltLook() {
    setColour(juce::TextButton::textColourOffId, pale);
    setColour(juce::TextButton::textColourOnId, pale);
    setColour(juce::ComboBox::backgroundColourId, navy);
    setColour(juce::ComboBox::textColourId, pale);
    setColour(juce::ComboBox::outlineColourId, highlight);
    setColour(juce::ComboBox::arrowColourId, pale);
    setColour(juce::PopupMenu::backgroundColourId, navy);
    setColour(juce::PopupMenu::textColourId, pale);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, cobalt);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff17178a));
    setColour(juce::TextEditor::textColourId, pale);
    setColour(juce::TextEditor::outlineColourId, highlight);
    setColour(juce::TextEditor::focusedOutlineColourId, green);
    setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff101078));
    setColour(juce::ScrollBar::thumbColourId, highlight);
    setColour(juce::Slider::textBoxTextColourId, pale);
    setColour(juce::Slider::textBoxBackgroundColourId, navy);
    setColour(juce::Slider::textBoxOutlineColourId, shadow);
    setColour(juce::TooltipWindow::backgroundColourId, pale);
    setColour(juce::TooltipWindow::textColourId, ink);
}

void CobaltLook::bevel(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Colour colour, bool inset) {
    g.setColour(colour); g.fillRect(bounds);
    g.setColour(inset ? ink : pale);
    g.fillRect(bounds.getX(), bounds.getY(), bounds.getWidth(), 2);
    g.fillRect(bounds.getX(), bounds.getY(), 2, bounds.getHeight());
    g.setColour(inset ? pale : ink);
    g.fillRect(bounds.getX(), bounds.getBottom() - 2, bounds.getWidth(), 2);
    g.fillRect(bounds.getRight() - 2, bounds.getY(), 2, bounds.getHeight());
    g.setColour(inset ? shadow : highlight); g.drawRect(bounds.reduced(2), 1);
}

void CobaltLook::drawButtonBackground(juce::Graphics& g, juce::Button& button,
    const juce::Colour&, bool, bool down) {
    bevel(g, button.getLocalBounds(), panel, down);
}

void CobaltLook::drawScrollbarButton(juce::Graphics& g, juce::ScrollBar&, int w, int h,
    int direction, bool, bool, bool down) {
    bevel(g, {0, 0, w, h}, highlight, down);
    g.setColour(ink);
    pixelArrow(g, juce::Rectangle<int>(0, 0, w, h).translated(down ? 1 : 0, down ? 1 : 0), direction);
}

void CobaltLook::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h,
    bool vertical, int thumbStart, int thumbSize, bool, bool) {
    g.setColour(juce::Colour(0xff4a4aff)); g.fillRect(x, y, w, h);
    g.setColour(highlight);
    for (int row = y; row < y + h; ++row)
        for (int column = x + ((row - y) & 1); column < x + w; column += 2)
            g.fillRect(column, row, 1, 1);
    if (thumbSize > 0) {
        const auto thumb = vertical ? juce::Rectangle<int>(x, thumbStart, w, thumbSize)
                                    : juce::Rectangle<int>(thumbStart, y, thumbSize, h);
        bevel(g, thumb, highlight);
    }
}

void CobaltLook::drawComboBox(juce::Graphics& g, int w, int h, bool down,
    int, int, int, int, juce::ComboBox&) {
    bevel(g, {0, 0, w, h}, navy, true);
    const juce::Rectangle<int> button(w - 23, 3, 20, h - 6);
    bevel(g, button, highlight, down);
    g.setColour(ink); pixelArrow(g, button.translated(down ? 1 : 0, down ? 1 : 0), 2);
}

void CobaltLook::fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor& editor) {
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId)); g.fillRect(0, 0, w, h);
}
void CobaltLook::drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor&) {
    g.setColour(ink); g.drawRect(0, 0, w, h, 2);
    g.setColour(pale); g.fillRect(0, h - 1, w, 1); g.fillRect(w - 1, 0, 1, h);
}
void CobaltLook::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    bevel(g, {0, 0, w, h}, navy);
}
void CobaltLook::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool down) {
    g.setColour(pale); g.setFont(uiFont(15, true));
    g.drawText(button.getButtonText(), button.getLocalBounds().translated(down ? 1 : 0, down ? 1 : 0), juce::Justification::centred);
}
void CobaltLook::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool, bool) {
    bevel(g, {0, 6, 18, 18}, navy, true);
    if (button.getToggleState()) {
        g.setColour(green);
        g.drawLine(4, 14, 8, 19, 2); g.drawLine(8, 19, 15, 10, 2);
    }
    g.setFont(uiFont(14)); g.setColour(pale);
    g.drawText(button.getButtonText(), 25, 0, button.getWidth() - 25, button.getHeight(), juce::Justification::centredLeft);
}
void CobaltLook::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float position,
    float, float, juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style == juce::Slider::LinearHorizontal) {
        const int centre = y + h / 2;
        g.setOpacity(slider.isEnabled() ? 1.0f : 0.45f);
        bevel(g, {x, centre - 4, w, 8}, navy, true);
        bevel(g, {juce::roundToInt(position) - 7, centre - 10, 15, 20}, highlight);
        g.setColour(ink); g.fillRect(juce::roundToInt(position), centre - 6, 1, 12);
        return;
    }
    const int centre = x + w / 2;
    g.setOpacity(slider.isEnabled() ? 1.0f : 0.45f);
    bevel(g, {centre - 4, y, 8, h}, navy, true);
    g.setColour(highlight.withAlpha(0.6f));
    for (int i = 0; i < 5; ++i) {
        const int mark = y + i * h / 4;
        g.drawHorizontalLine(mark, (float)centre - 20, (float)centre - 11);
        g.drawHorizontalLine(mark, (float)centre + 11, (float)centre + 20);
    }
    bevel(g, {centre - 17, juce::roundToInt(position) - 7, 34, 15}, juce::Colour(0xff8585ff));
    g.setColour(ink); g.fillRect(centre - 11, juce::roundToInt(position), 22, 1);
}
juce::Font CobaltLook::getComboBoxFont(juce::ComboBox&) { return uiFont(14); }
juce::Font CobaltLook::getTextButtonFont(juce::TextButton&, int) { return uiFont(15, true); }

void WizardKeyboard::drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area,
    bool down, bool over, juce::Colour colour) {
    if (down) colour = colour.overlaidWith(findColour(keyDownOverlayColourId));
    if (over) colour = colour.overlaidWith(findColour(mouseOverKeyOverlayColourId));
    g.setColour(colour);
    g.fillRect(area);
    g.setColour(findColour(keySeparatorLineColourId));
    g.drawRect(area, 1.0f);
}

SoundFontsLibraryBox::SoundFontsLibraryBox(juce::File folder) : libraryFolder(std::move(folder)) {
    setName("sf2 library");
    setTextWhenNothingSelected("sf2 library");
    setTooltip("sf2 library: " + libraryFolder.getFullPathName()
        + "\nPut .sf2 files here. Subfolders are included. Reopen this dropdown to refresh.");
    onChange = [this] {
        const int index = getSelectedId() - 1;
        if (juce::isPositiveAndBelow(index, files.size())) {
            currentFile = files[index];
            if (onFileChosen) onFileChosen(currentFile);
        }
    };
    refreshFiles();
}

void SoundFontsLibraryBox::refreshFiles() {
    files = libraryFolder.findChildFiles(juce::File::findFiles | juce::File::ignoreHiddenFiles, true, "*.sf2");
    std::sort(files.begin(), files.end(), [this](const juce::File& a, const juce::File& b) {
        return a.getRelativePathFrom(libraryFolder).compareNatural(b.getRelativePathFrom(libraryFolder)) < 0;
    });
    clear(juce::dontSendNotification);
    if (files.isEmpty()) {
        addItem("Put .sf2 files in Music\\soundfonts", 1);
        setItemEnabled(1, false);
    } else {
        for (int i = 0; i < files.size(); ++i)
            addItem(files[i].getRelativePathFrom(libraryFolder), i + 1);
    }
    selectFile(currentFile);
}

void SoundFontsLibraryBox::selectFile(const juce::File& file) {
    currentFile = file;
    const int index = files.indexOf(file);
    setSelectedId(index < 0 ? 0 : index + 1, juce::dontSendNotification);
}

void SoundFontsLibraryBox::showPopup() {
    refreshFiles();
    juce::ComboBox::showPopup();
}

SoundWizardEditor::SoundWizardEditor(SoundWizardProcessor& p)
    : AudioProcessorEditor(p), processor(p), keys(p.keyboard, juce::MidiKeyboardComponent::horizontalKeyboard) {
    setLookAndFeel(&look);
    for (auto* button : {&load, &starter, &preview, &stop}) addAndMakeVisible(button);
    load.onClick = [this] { openFile(); };
    starter.onClick = [this] { processor.loadStarterBank(); };
    preview.onClick = [this] { processor.preview(); };
    stop.onClick = [this] { processor.panic(); };
    load.setTooltip("Load a SoundFont2 (.sf2) file, or drop one onto Sound Wizard.");
    preview.setTooltip("Play middle C for one second. FL Studio's audio engine must be running.");
    stop.setTooltip("Immediately stop all voices and clear the reverb tail.");
    addAndMakeVisible(search);
    search.setFont(uiFont()); search.setTextToShowWhenEmpty("Search instruments...", pale.withAlpha(0.7f));
    search.onTextChange = [this] { filterLibrary(); };
    addAndMakeVisible(soundFonts);
    soundFonts.onFileChosen = [this](const juce::File& file) {
        processor.loadSoundFont(file);
    };
    addAndMakeVisible(library); library.setRowHeight(44);
    library.getVerticalScrollBar().setAutoHide(false);
    library.getViewport()->setScrollBarThickness(20);
    library.setOutlineThickness(1); library.setColour(juce::ListBox::outlineColourId, highlight);
    library.setMultipleSelectionEnabled(false); library.setWantsKeyboardFocus(false);
    addAndMakeVisible(custom);
    customAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "customEnvelope", custom);
    mono.setName("MONO"); mono.setTitle("Mono");
    mono.setTooltip("One note at a time, with last-note priority. Overlapping notes play legato.");
    addAndMakeVisible(mono);
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "mono", mono);
    porta.setName("PORTA"); porta.setTitle("Portamento");
    porta.setSliderStyle(juce::Slider::LinearHorizontal);
    porta.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 23);
    porta.setScrollWheelEnabled(false);
    porta.setTooltip("Glide time between overlapping notes in mono or poly mode. Off to 2 seconds. Double-click to reset.");
    addAndMakeVisible(porta);
    portaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, "portamento", porta);
    porta.textFromValueFunction = [](double value) {
        if (value == 0) return juce::String("Off");
        return value < 1 ? juce::String(juce::roundToInt(value * 1000)) + " ms" : juce::String(value, 2) + " s";
    };
    porta.valueFromTextFunction = [](const juce::String& text) {
        return text.containsIgnoreCase("ms") ? text.getDoubleValue() * 0.001 : text.getDoubleValue();
    };
    porta.updateText();
    portaLabel.setText("Porta", juce::dontSendNotification);
    portaLabel.setFont(uiFont(14)); portaLabel.setColour(juce::Label::textColourId, pale);
    addAndMakeVisible(portaLabel);
    const char* ids[] = {"attack", "decay", "sustain", "release", "reverb", "volume"};
    const char* names[] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE", "REVERB", "VOLUME"};
    for (size_t i = 0; i < sliders.size(); ++i) {
        auto& slider = sliders[i];
        slider.setName(names[i]); slider.setTitle(names[i]);
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 23);
        slider.setScrollWheelEnabled(false);
        slider.setTooltip(i < 4 ? "Custom ADSR affects newly played notes. Enable Use custom ADSR to edit."
            : i == 4 ? "Amount of stereo reverb." : "Master output level. Double-click to reset.");
        addAndMakeVisible(slider);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i], slider);
        slider.textFromValueFunction = [i](double value) {
            if (i == 2 || i == 4) return juce::String(juce::roundToInt(value * 100)) + "%";
            if (i == 5) return value <= -60 ? juce::String("Mute") : juce::String(value, 1) + " dB";
            return value < 1 ? juce::String(juce::roundToInt(value * 1000)) + " ms" : juce::String(value, 2) + " s";
        };
        slider.valueFromTextFunction = [i](const juce::String& text) {
            if (text.equalsIgnoreCase("Mute")) return -60.0;
            const double value = text.getDoubleValue();
            if (i == 2 || i == 4) return value * 0.01;
            if (i < 4 && text.containsIgnoreCase("ms")) return value * 0.001;
            return value;
        };
        slider.updateText();
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setFont(uiFont(12, true)); labels[i].setColour(juce::Label::textColourId, pale);
        labels[i].setJustificationType(juce::Justification::centred); addAndMakeVisible(labels[i]);
    }
    addAndMakeVisible(keys);
    keys.setOctaveForMiddleC(4);
    keys.setKeyWidth(23); keys.setAvailableRange(36, 96); keys.setLowestVisibleKey(48);
    keys.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xffff2838));
    keys.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xffff2838));
    keys.setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keys.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xffffafb5).withAlpha(0.65f));
    keys.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0xffff7c88).withAlpha(0.4f));
    keys.setWantsKeyboardFocus(false);
    setResizable(true, true); setResizeLimits(864, 648, 1440, 1080);
    getConstrainer()->setFixedAspectRatio(4.0 / 3.0); setSize(960, 720);
    refreshLibrary(); timerCallback(); startTimerHz(30);
}

SoundWizardEditor::~SoundWizardEditor() { stopTimer(); setLookAndFeel(nullptr); }

void SoundWizardEditor::resized() {
    const float scale = (float)getWidth() / 960;
    auto place = [scale](juce::Component& component, int x, int y, int w, int h) {
        component.setBounds(juce::Rectangle<float>(x * scale, y * scale, w * scale, h * scale).toNearestInt());
    };
    place(load, 20, 66, 140, 34); place(starter, 170, 66, 132, 34);
    place(preview, 670, 66, 150, 34); place(stop, 830, 66, 110, 34);
    place(search, 32, 160, 236, 28); place(soundFonts, 32, 198, 236, 28);
    place(library, 32, 238, 236, 254);
    place(mono, 32, 501, 100, 30);
    place(portaLabel, 32, 539, 48, 24);
    place(porta, 84, 535, 184, 30);
    library.setRowHeight(juce::roundToInt(44 * scale));
    place(custom, 514, 374, 190, 30);
    for (size_t i = 0; i < sliders.size(); ++i) {
        const int x = i < 4 ? 310 + (int)i * 102 : 742 + ((int)i - 4) * 98;
        place(labels[i], x, 415, 90, 22); place(sliders[i], x, 445, 90, 118);
    }
    place(keys, 22, 604, 916, 72);
}

void SoundWizardEditor::paint(juce::Graphics& g) {
    g.fillAll(cobalt);
    g.addTransform(juce::AffineTransform::scale((float)getWidth() / 960));
    CobaltLook::bevel(g, {0, 0, 960, 720}, cobalt);
    CobaltLook::bevel(g, {10, 10, 940, 38}, navy);
    // A small wizard hat, drawn with the same hard edges as the window controls.
    juce::Path hat; hat.startNewSubPath(27, 36); hat.lineTo(39, 17); hat.lineTo(49, 36); hat.closeSubPath();
    g.setColour(highlight); g.fillPath(hat); g.fillRect(24, 36, 30, 3);
    g.setColour(green); g.fillRect(38, 26, 3, 3);
    g.setFont(juce::Font(juce::FontOptions("Times New Roman", 32, juce::Font::bold | juce::Font::italic)));
    g.setColour(ink);
    g.drawText("Sound Wizard", 68, 14, 360, 34, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffffff00));
    g.drawText("Sound Wizard", 66, 12, 360, 34, juce::Justification::centredLeft);
    g.setFont(monoFont(12)); g.setColour(highlight);
    g.drawText("SOUNDFONT PLAYER  /  v0.2.0", 620, 15, 312, 27, juce::Justification::centredRight);
    g.setFont(uiFont(14)); g.setColour(pale);
    const auto status = info.loading || info.message != "Ready" ? info.message : info.name;
    g.drawFittedText(status, 320, 66, 334, 34, juce::Justification::centredLeft, 1);
    CobaltLook::bevel(g, {20, 118, 260, 466}, panel);
    g.setColour(juce::Colour(0xffffff00));
    g.setFont(juce::Font(juce::FontOptions("Times New Roman", 20, juce::Font::bold)));
    g.drawText("Soundfont Library", 32, 127, 236, 24, juce::Justification::centredLeft);
    g.drawHorizontalLine(151, 32, 190);
    CobaltLook::bevel(g, {300, 118, 640, 230}, panel);
    const int selected = processor.selectedInstrument();
    const juce::String instrument = juce::isPositiveAndBelow(selected, info.instruments.size())
        ? info.instruments[selected] : juce::String("Select an instrument");
    g.setFont(uiFont(20, true)); g.setColour(pale);
    g.drawFittedText(instrument.upToFirstOccurrenceOf("|", false, false), 318, 131, 600, 28, juce::Justification::centredLeft, 1);
    g.setFont(monoFont(12)); g.setColour(highlight);
    g.drawText("BANK : PROGRAM  " + instrument.fromFirstOccurrenceOf("|", false, false), 320, 164, 596, 18, juce::Justification::centredLeft);
    CobaltLook::bevel(g, {318, 192, 604, 123}, cobalt, true);
    const juce::Rectangle<float> screen(324, 198, 592, 111);
    g.setColour(juce::Colour(0xff3939ff));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine(324 + i * 74, 198, 309);
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine(198 + i * 28, 324, 916);
    juce::Path wave;
    float peak = 0;
    for (float sample : waveform) peak = juce::jmax(peak, std::abs(sample));
    const float displayGain = juce::jmin(30.0f, 0.85f / juce::jmax(0.001f, peak));
    for (size_t i = 0; i < waveform.size(); ++i) {
        const float x = screen.getX() + (float)i * screen.getWidth() / (float)(waveform.size() - 1);
        const float y = screen.getCentreY() - juce::jlimit(-1.0f, 1.0f, waveform[i] * displayGain) * screen.getHeight() * 0.44f;
        if (i == 0) wave.startNewSubPath(x, y); else wave.lineTo(x, y);
    }
    g.setColour(waveformRed); g.strokePath(wave, juce::PathStrokeType(4.0f));
    g.setFont(monoFont(11)); g.setColour(waveformRed);
    g.drawText("LIVE OUTPUT", 331, 201, 150, 16, juce::Justification::centredLeft);
    g.setColour(pale); g.setFont(monoFont(12));
    g.drawText(juce::String(processor.activeVoices.load()) + " / 128 voices", 320, 321, 270, 18, juce::Justification::centredLeft);
    g.drawText("MIDI  OMNI", 730, 321, 188, 18, juce::Justification::centredRight);
    CobaltLook::bevel(g, {300, 366, 640, 218}, panel);
    g.setColour(juce::Colour(0xffffff00));
    g.setFont(juce::Font(juce::FontOptions("Times New Roman", 20, juce::Font::bold)));
    g.drawText("Envelope", 318, 377, 185, 22, juce::Justification::centredLeft);
    g.drawText("Output", 754, 377, 164, 22, juce::Justification::centredLeft);
    g.drawHorizontalLine(401, 318, 400);
    g.drawHorizontalLine(401, 754, 818);
    g.setColour(shadow); g.fillRect(728, 379, 1, 188);
    CobaltLook::bevel(g, {20, 602, 920, 76}, navy, true);

}

int SoundWizardEditor::getNumRows() { return (int)filtered.size(); }
void SoundWizardEditor::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) {
    if (!juce::isPositiveAndBelow(row, (int)filtered.size())) return;
    const auto entry = info.instruments[filtered[(size_t)row]];
    if (selected) { g.setColour(cobalt); g.fillAll(); g.setColour(highlight); g.drawRect(0, 0, w, h); }
    g.setFont(uiFont(14, selected)); g.setColour(pale);
    g.drawFittedText(entry.upToFirstOccurrenceOf("|", false, false), 10, 3, w - 18, h / 2, juce::Justification::centredLeft, 1);
    g.setFont(monoFont(11)); g.setColour(selected ? pale : highlight);
    g.drawText("Bank " + entry.fromFirstOccurrenceOf("|", false, false).replace(":", "  /  Program "), 10, h / 2, w - 18, h / 2 - 2, juce::Justification::centredLeft);
}
void SoundWizardEditor::selectedRowsChanged(int row) {
    if (!updatingSelection && juce::isPositiveAndBelow(row, (int)filtered.size()))
        processor.selectInstrument(filtered[(size_t)row]);
}
void SoundWizardEditor::filterLibrary() {
    filtered.clear();
    const auto text = search.getText().trim();
    for (int i = 0; i < info.instruments.size(); ++i)
        if (text.isEmpty() || info.instruments[i].containsIgnoreCase(text)) filtered.push_back(i);
    updatingSelection = true;
    library.updateContent(); library.deselectAllRows();
    const int selected = processor.selectedInstrument();
    for (size_t i = 0; i < filtered.size(); ++i) if (filtered[i] == selected) library.selectRow((int)i);
    updatingSelection = false;
    repaint();
}
void SoundWizardEditor::refreshLibrary() {
    info = processor.getLibraryInfo(); lastRevision = info.revision;
    soundFonts.selectFile(info.path.isEmpty() ? juce::File() : juce::File(info.path));
    filterLibrary();
}
void SoundWizardEditor::timerCallback() {
    const auto latest = processor.getLibraryInfo();
    if (latest.revision != lastRevision) refreshLibrary();
    if (lastSelected != processor.selectedInstrument()) {
        lastSelected = processor.selectedInstrument(); filterLibrary();
    }
    const bool enabled = processor.state.getRawParameterValue("customEnvelope")->load() > 0.5f;
    for (size_t i = 0; i < 4; ++i) sliders[i].setEnabled(enabled);
    processor.readScope(waveform); repaint();
}
void SoundWizardEditor::openFile() {
    chooser = std::make_unique<juce::FileChooser>("Load a SoundFont", juce::File(info.path), "*.sf2");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<SoundWizardEditor>(this)](const juce::FileChooser& dialog) {
            if (safe && dialog.getResult().existsAsFile()) safe->processor.loadSoundFont(dialog.getResult());
        });
}
bool SoundWizardEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size() == 1 && juce::File(files[0]).hasFileExtension("sf2");
}
void SoundWizardEditor::filesDropped(const juce::StringArray& files, int, int) {
    if (isInterestedInFileDrag(files)) processor.loadSoundFont(juce::File(files[0]));
}
