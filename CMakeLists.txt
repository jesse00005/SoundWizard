cmake_minimum_required(VERSION 3.22)
project(SoundWizard VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(JUCE_DIR "" CACHE PATH "Optional path to an existing JUCE 8.0.12 source checkout")
if(NOT JUCE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
endif()
if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE
        GIT_SUBMODULES "")
    FetchContent_MakeAvailable(juce)
endif()
juce_add_binary_data(WizardBank SOURCES Assets/WizardStarter.sf2)
juce_add_plugin(SoundWizard
    COMPANY_NAME "Night Haul Audio"
    BUNDLE_ID "audio.nighthaul.soundwizard"
    PLUGIN_MANUFACTURER_CODE Nhau PLUGIN_CODE Swzd
    PRODUCT_NAME "Sound Wizard"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Sampler
    FORMATS VST3 COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(SoundWizard PRIVATE Source/Engine.cpp Source/Processor.cpp Source/Editor.cpp)
target_include_directories(SoundWizard PUBLIC Source)
target_compile_definitions(SoundWizard PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(SoundWizard PUBLIC juce::juce_audio_utils juce::juce_dsp
    PRIVATE WizardBank juce::juce_recommended_config_flags)
juce_add_console_app(WizardCheck PRODUCT_NAME "WizardCheck")
target_sources(WizardCheck PRIVATE Tests/Check.cpp)
target_link_libraries(WizardCheck PRIVATE SoundWizard)
enable_testing()
add_test(NAME wizard_checks COMMAND WizardCheck)
