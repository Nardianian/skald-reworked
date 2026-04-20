
cmake_minimum_required(VERSION 3.15)
project(Skald-Reworked VERSION 1.2.2)

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
    VERSION ${PROJECT_VERSION}
    FORMATS VST3 VST AAX LV2 Standalone
    PRODUCT_NAME "Skald-Reworked"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Instrument" "Generator"
    VST2_CATEGORY kPlugCategSynth
    AAX_EPlugInCategory_SWGenerators 
    BUNDLE_ID "com.Nardianian.skald-reworked"
    LV2URI https://github.com/Nardianian/skald-reworked
    AAX_IDENTIFIER com.Nardianian.skald
    PLUGIN_MANUFACTURER_EMAIL "support@beowulfaudio.com"
)

# Add source files
target_sources(Skald PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/EuclideanSet.cpp
    Source/EuclideanSet.h
    Source/RtMidi.cpp
)

# Compiler definitions
target_compile_definitions(Skald PUBLIC
    _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS
    __WINDOWS_UWP__=1
    __WINDOWS_MM__=1
    JUCE_USE_WINRT_MIDI=1
    JUCE_FORCE_WINRT_MIDI=0
    JUCE_USE_WINDOWS_MIDI_SERVICES=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_ASIO=1
    JUCE_JACK=1
    JUCE_DIRECT2D=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_WIN_PER_MONITOR_DPI_AWARE=1
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Link required JUCE modules
target_link_libraries(Skald PRIVATE
    runtimeobject 
    WindowsApp
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
				
