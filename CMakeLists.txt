cmake_minimum_required(VERSION 3.15)
project(Skald VERSION 1.0.4)

# Add JUCE as a subdirectory
add_subdirectory(../JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

# Add other plugins subdirectories
juce_set_vst2_sdk_path("C:/SDKs/vstsdk2.4")
juce_set_aax_sdk_path("C:/SDKs/aax-sdk")

include_directories(C:/SDKs/asiosdk/common)
include_directories("C:/Program Files/JACK2/include")

# Create binary data from images
juce_add_binary_data(BinaryData
    SOURCES
        images/viking_full.png
        images/viking_head.png
        images/switch_toggle.png
        images/knob_simplegray.png
        images/wallpaper.jpg
        fonts/header.ttf
        fonts/subheader.ttf
        fonts/paragraph.ttf
)

# Create the plugin target
juce_add_plugin(Skald
    COMPANY_NAME "BeowulfAudio"
    PLUGIN_MANUFACTURER_CODE Bwlf
    PLUGIN_CODE Skld
    FORMATS VST3 VST AAX LV2 Standalone
    PRODUCT_NAME "Skald"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Instrument" "Generator"
    VST2_CATEGORY kPlugCategSynth
    AAX_EPlugInCategory_SWGenerators 
    BUNDLE_ID "https://github.com/josephvolmer/skald"
    LV2URI https://github.com/josephvolmer/skald
    AAX_IDENTIFIER com.josephvolmer.skald
    PLUGIN_MANUFACTURER_EMAIL "support@beowulfaudio.com"
)

# Add source files
target_sources(Skald PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

# Compiler definitions
target_compile_definitions(Skald PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1
    JUCE_JACK=1
    JUCE_USE_WINRT_MIDI=1
    JUCE_FORCE_WINRT_MIDI=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link required JUCE modules
target_link_libraries(Skald PRIVATE
    BinaryData
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# C++ standard
target_compile_features(Skald PRIVATE cxx_std_17)
