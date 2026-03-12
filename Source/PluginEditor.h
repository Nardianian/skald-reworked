#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "EuclideanSet.h"

//==============================================================================
// Forward declaration
class SkaldEditor;

// Custom hardware-style button look
class HardwareButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HardwareButtonLookAndFeel(SkaldEditor* ed) : editor(ed) {}

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                            bool isMouseOverButton, bool isButtonDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                       bool isMouseOverButton, bool isButtonDown) override;

private:
    SkaldEditor* editor;
};

//==============================================================================
// Custom Music Knob (inspired by mx-knob from music-ux)
class MusicKnob : public juce::Slider
{
public:
    MusicKnob()
    {
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                           juce::MathConstants<float>::pi * 2.75f, true);
    }

    void paint(juce::Graphics& g) override;
    void setSpriteImage(const juce::Image& sprite, int numFrames)
    {
        knobSprite = sprite;
        spriteFrameCount = numFrames;
    }

private:
    juce::Image knobSprite;
    int spriteFrameCount = 101;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicKnob)
};

//==============================================================================
// Custom Toggle Switch (inspired by mx-switch from music-ux)
class MusicToggle : public juce::ToggleButton
{
public:
    MusicToggle() {}

    void paint(juce::Graphics& g) override;
    void setSpriteImage(const juce::Image& sprite) { toggleSprite = sprite; }

private:
    juce::Image toggleSprite;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicToggle)
};

//==============================================================================
class SkaldEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    SkaldEditor (SkaldProcessor&);
    int getSelectedDotIndex() const { return selectedDotIndex; }
    ~SkaldEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void timerCallback() override;

    // Images (public so LookAndFeel can access)
    juce::Image vikingFullImage;   // Full body for help/about screen
    juce::Image addIcon;
    juce::Image clearIcon;
    juce::Image wallpaperImage;

    // Custom fonts
    juce::Font csArthemisFont { juce::FontOptions() };      // For titles
    juce::Font distropiaxFont { juce::FontOptions() };      // For sub-headers
    juce::Font wonderworldFont { juce::FontOptions() };     // For regular text

private:
    SkaldProcessor& audioProcessor;
    HardwareButtonLookAndFeel hardwareLookAndFeel;

    // UI Components
    juce::Label speedDisplay;
    juce::Label speedLabel;
    juce::TextButton speedTapButton;

    juce::Label scaleDisplay;
    juce::Label scaleLabel;
    juce::TextButton scaleTapButton;

    juce::Label keyDisplay;
    juce::Label keyLabel;
    juce::TextButton keyTapButton;

    juce::Label octaveDisplay;
    juce::Label octaveLabel;
    juce::TextButton octaveTapButton;

    juce::TextButton clearButton;
    juce::Label clearLabel;
    juce::TextButton addDotButton;
    juce::Label addLabel;
    juce::TextButton randomizeButton;
    juce::Label randomizeLabel;
    juce::TextButton playStopButton;
    juce::Label bpmLabel;
    juce::Slider bpmSlider;

    juce::Label versionLabel; // Patch 47

    // New quick-win controls (row 2)
    MusicKnob velocityKnob;
    juce::Label velocityLabel;
    MusicKnob gateTimeKnob;
    juce::Label gateTimeLabel;
    MusicToggle reverseToggle;
    juce::Label reverseLabel;
    MusicToggle startStopToggle;
    juce::Label startStopLabel;

    // High-value controls (row 3)
    MusicKnob probabilityKnob;
    juce::Label probabilityLabel;
    MusicKnob velocityVariationKnob;
    juce::Label velocityVariationLabel;
    MusicKnob swingKnob;
    juce::Label swingLabel;
    juce::TextButton savePatternButton;
    juce::Label saveLabel;
    juce::TextButton loadPatternButton;
    juce::Label loadLabel;
    juce::TextButton aboutButton;
    juce::Label aboutLabel;

    // Help/About screen
    juce::TextButton backButton;
    juce::Label backLabel;
    bool showingHelpScreen = false;

    // --- Algorithmic Section ---
    juce::Slider pulsesSlider, stepsSlider, depthSlider, shiftSlider;
    juce::Label pulsesLabel, stepsLabel, depthLabel, shiftLabel;
    juce::ComboBox ringSelector;
    juce::Label ringSelectorLabel;
    juce::TextButton generateButton;
    juce::Label algoTitleLabel;
    int selectedRingForAlgo = 0; // Ring attualmente sotto editing (0-11)

    // Current selection indices
    int currentSpeedIndex = 2; // Default to 1x
    int currentScaleIndex = 4; // Default to Pentatonic
    int currentKeyIndex = 0;   // Default to C
    int currentOctaveIndex = 2; // Default to baseline (0 = -2, 1 = -1, 2 = 0, 3 = +1, 4 = +2)

    // Turntable visualization
    juce::Rectangle<float> turntableArea;
    float turntableRadius = 150.0f;
    juce::Point<float> turntableCenter;

    // Interaction state
    int selectedDotIndex = -1;
    bool isDraggingDot = false;
    int initialChannelOnDrag = 1;
    int initialVelocityOnDrag;
    int currentMidiChannel = 1;

    // Scratching state
    bool isScratching = false;
    float lastScratchAngle = 0.0f;
    juce::int64 lastScratchTime = 0;
    juce::Point<float> lastScratchPos;
    float scratchVelocity = 0.0f;

    // Available colors for different MIDI channels
    std::vector<juce::Colour> channelColors = {
        juce::Colours::red,
        juce::Colours::blue,
        juce::Colours::green,
        juce::Colours::yellow,
        juce::Colours::orange,
        juce::Colours::purple,
        juce::Colours::cyan,
        juce::Colours::magenta,
        juce::Colours::lime,
        juce::Colours::pink,
        juce::Colours::brown,
        juce::Colours::grey,
        juce::Colours::gold,
        juce::Colours::turquoise,
        juce::Colours::violet,
        juce::Colours::salmon
    };

    // Helper methods
    float angleFromPoint(juce::Point<float> point);
    juce::Point<float> pointFromAngle(float angle, float radius);
    int findDotAtPoint(juce::Point<float> point);
    float getRingSpacing() const;
    juce::String midiNoteToString(int midiNote) const;
    void paintHelpScreen(juce::Graphics& g);
    void setControlsVisible(bool visible);

    // Visual feedback helpers
    float calculateGlowBrightness(int velocity) const;
    float getSwingOffset(int beatCount, float swingAmount) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkaldEditor)
};


