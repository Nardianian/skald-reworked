#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include "EuclideanSet.h"
#include "RtMidi.h"
#include <atomic>
#include <map> 

//==============================================================================
// Scale types
enum class ScaleType
{
    Major,
    Minor,
    HarmonicMinor,
    MelodicMinor,
    Pentatonic,
    PentatonicMinor,
    Blues,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    Chromatic
};

// Data structure for a single dot on a turntable
struct TurntableDot
{
    float angle;        // Position on the turntable (0-360 degrees)
    int ringIndex;      // Which ring (0-11) - determines pitch in scale
    juce::Colour color; // Visual color representation
    int midiChannel;    // MIDI Channel (1-16)
    bool active;        // Whether this dot is active
    int velocity;       // Individual velocity (1-127)
    float gateTime;     // Individual gate time (0.0 to 1.0 multiplier)

    TurntableDot() : angle(0.0f), ringIndex(0),
        color(juce::Colours::red), midiChannel(1), active(true),
        velocity(100), gateTime(0.5f) {}
};

//==============================================================================
class SkaldProcessor : public juce::AudioProcessor
{
public:
    // Track recently triggered dots for visual feedback
    struct TriggeredDotInfo
    {
        int dotIndex;
        juce::int64 timestamp;
        int velocity;           // Actual triggered velocity (after variation)
        float gateTimeMs;       // Gate time for this trigger
        bool wasTriggered;      // True if probability allowed trigger
        int beatCount;          // Beat counter state (for swing visualization)
    };

    // Structure to manage generation parameters for each ring
    struct RingSettings {
        bool isEuclidean = false; // Mode Selector (Standard vs. Euclidean)
        int pulses = 4;           // Number of pulses
        int steps = 16;           // Number of grid steps
        int depth = 1;            // Hyper-Euclidean Depth
        int shift = 0;            // Pattern Rotation/Shift
    };

    RingSettings ringSettings[12]; // Gestiamo fino a 12 anelli (corrispondenti alle note della scala)

    // --- MIDI LEARN SYSTEM ---
    struct MidiMapping {
        int ccNumber = -1;
        juce::String paramID;
    };

    // Map to associate ParamID -> CC Number
    std::map<juce::String, int> midiMappings;

    bool isMidiLearning = false;
    juce::String lastParamToLearn = "";

    // Helper to apply MIDI values ​​to GUI object type  
    void applyMidiControl(const juce::String& paramID, int value);

    SkaldProcessor();
    ~SkaldProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Turntable-specific methods
    void addDot(float angle, int ringIndex, juce::Colour color);
    void removeDot(int index);
    void clearAllDots();
    void exportMidiFile(const juce::File& targetFile);
    std::vector<TurntableDot>& getDots() { return dots; }

    // Scale and key management
    void setScale(ScaleType newScale);
    void setRootNote(int newRoot); // 0-11 (C-B)
    void setOctaveShift(int shift); // -2, -1, 0, +1, +2
    ScaleType getScale() const { return currentScale; }
    int getRootNote() const { return rootNote; }
    int getOctaveShift() const { return octaveShift; }
    // Force the number of rings to 9 (0=Scratch, 1-8=Note)
    // This ensures that the Editor graphics and the Processor logic are always aligned.
    int getNumRings() const { return 9; }

    // Convert ring index to MIDI note based on current scale/key
    int ringToMidiNote(int ringIndex) const;

    // Get scale intervals
    static std::vector<int> getScaleIntervals(ScaleType scale);

    // Speed control (1.0 = normal speed, 2.0 = double speed, etc.)
    void setSpeed(float newSpeed) { speed = newSpeed; }
    float getSpeed() const { return speed; }

    // Get current rotation angle (for GUI visualization)
    float getCurrentRotation() const { return currentRotation; }

    // Velocity control (1-127)
    void setGlobalVelocity(int vel) { globalVelocity = juce::jlimit(1, 127, vel); }
    int getGlobalVelocity() const { return globalVelocity; }

    // Gate time control (in milliseconds)
    void setGateTime(float timeMs) { gateTimeMs = juce::jmax(10.0f, timeMs); }
    float getGateTime() const { return gateTimeMs; }

    // Reverse rotation
    void setReverse(bool shouldReverse) { isReversed = shouldReverse; }
    bool getReverse() const { return isReversed; }

    // Motor control (record player style start/stop)
    void setMotorRunning(bool shouldRun) { motorRunning = shouldRun; }
    bool getMotorRunning() const { return motorRunning; }

    // Scratching control (manual turntable manipulation)
    void setScratchVelocity(float velocity) { scratchVelocity = velocity; }
    float getScratchVelocity() const { return scratchVelocity; }
    void setBeingScratched(bool scratching) { isBeingScratched = scratching; }
    void setRotationDirect(float angle) { currentRotation = angle; }
    float getCurrentSpeedMultiplier() const { return currentSpeedMultiplier; }
    void setCurrentSpeedMultiplier(float mult) { currentSpeedMultiplier = mult; }

    // Probability control (0-100%)
    void setProbability(float prob) { probability = juce::jlimit(0.0f, 100.0f, prob); }
    float getProbability() const { return probability; }

    // Velocity variation (0-100%)
    void setVelocityVariation(float var) { velocityVariation = juce::jlimit(0.0f, 100.0f, var); }
    float getVelocityVariation() const { return velocityVariation; }

    // Swing amount (0-100%)
    void setSwing(float sw) { swing = juce::jlimit(0.0f, 100.0f, sw); }
    float getSwing() const { return swing; }

    // Getter for algorithmic parameters
    int getLastPulses() const { return lastPulses; }
    int getLastSteps() const { return lastSteps; }
    int getLastDepth() const { return lastDepth; }
    int getLastShift() const { return lastShift; }

    // Standalone transport control
    void setPlaying(bool shouldPlay) { isPlayingStandalone = shouldPlay; }
    bool isPlaying() const { return isPlayingStandalone; }

    // BPM control for standalone
    void setStandaloneBPM(double bpm) { standaloneBPM = bpm; }

    // Functions to activate Learn from outside
    void startMidiLearn(const juce::String& paramID) {
        lastParamToLearn = paramID;
        isMidiLearning = true;
    }

    void stopMidiLearn() { isMidiLearning = false; }
    void clearMidiMapping(const juce::String& paramID) { midiMappings.erase(paramID); }
    void clearAllMidiMappings() { midiMappings.clear(); isMidiLearning = false; }
    double getStandaloneBPM() const { return standaloneBPM; }

    // Get effective BPM (standalone or host)
    double getBPM() const { return hostBPM; }

    // Preview note triggering (for UI feedback)
    void triggerPreviewNote(int ringIndex);

    // Get recently triggered dots for visual feedback
    std::vector<TriggeredDotInfo> getRecentlyTriggeredDots() const;

    void generateHyperPattern(int ringIndex, int pulses, int steps, int depth, int rotation);

    // Refresh function for access from the Editor
    void refreshMidiOutputs();

    // MIDI Standalone Management
    void changeMidiPort(int index);
    juce::Array<juce::MidiDeviceInfo>& getAvailableMidiOutputs() { return availableMidiOutputs; }
    int getSelectedMidiPortIndex() const { return standaloneMidiOut.selectedPortIndex; }

    // Device Manager to force MIDI in Standalone
    juce::AudioDeviceManager deviceManager;


private:
    // Structure to track active MIDI notes for proper note-off timing
    struct ActiveNote
    {
        int midiNote;
        int channel;
        juce::int64 noteOffSample;  // Absolute sample position when note should turn off
    };
    std::vector<ActiveNote> activeNotes;
    juce::int64 totalSamplesProcessed = 0;  // Track absolute sample position
    //==============================================================================
    std::vector<TurntableDot> dots;
    float currentRotation = 0.0f;  // Current rotation angle (0-360)
    float speed = 1.0f;             // Rotation speed multiplier
    double hostBPM = 120.0;         // BPM from host DAW
    double sampleRate = 44100.0;

    // Quick-win parameters
    int globalVelocity = 100;       // MIDI velocity (1-127)
    float gateTimeMs = 100.0f;      // Note duration in milliseconds
    bool isReversed = false;        // Reverse rotation direction

    // Motor control (record player style)
    bool motorRunning = true;            // Motor on/off state
    float currentSpeedMultiplier = 1.0f; // Current speed (ramps up/down like record player)

    // Scratching/manual control
    float scratchVelocity = 0.0f;       // Angular velocity from scratching (degrees per second)
    bool isBeingScratched = false;      // True when user is actively scratching

    // High-value parameters
    float probability = 100.0f;     // Probability of note trigger (0-100%)
    float velocityVariation = 0.0f; // Velocity randomization amount (0-100%)
    float swing = 0.0f;             // Swing amount (0-100%)

    // --- Algorithmic parameter memory ---
    int lastPulses = 4;
    int lastSteps = 16;
    int lastDepth = 1;
    int lastShift = 0;

    // Standalone mode variables
    bool isPlayingStandalone = false;
    double standaloneBPM = 120.0;

    // Scale and key settings
    ScaleType currentScale = ScaleType::Pentatonic;
    int rootNote = 0; // C
    int baseOctave = 4; // C4 as base
    int octaveShift = 0; // -2, -1, 0, +1, or +2 octave shift
    std::vector<int> scaleNotes; // MIDI notes for current scale

    // Update scale notes based on current settings
    void updateScaleNotes();

    // Track which notes we've triggered to avoid re-triggering
    std::vector<bool> triggeredThisRotation;

    // Preview notes queue (for UI feedback)
    struct PreviewNote
    {
        int midiNote;
        int timeStamp;
    };
    std::vector<PreviewNote> previewNotesToSend;
    juce::CriticalSection previewNotesLock;

    // Track recently triggered dots for visual feedback
    std::vector<TriggeredDotInfo> recentlyTriggeredDots;
    juce::CriticalSection triggeredDotsLock;

    // Random number generator for probability and velocity variation
    juce::Random random;

    // Helper to save/load MIDI mappings in plugin state
    void saveMidiMappings(juce::XmlElement& xml);
    void loadMidiMappings(juce::XmlElement& xml);

    // Track swing state (which beat we're on for swing timing)
    int swingBeatCounter = 0;

    // --- Manual MIDI management ---
    struct CustomMidiOut
    {
        std::unique_ptr<juce::MidiOutput> output;
        int selectedPortIndex = -1;
    };
    CustomMidiOut standaloneMidiOut;
    juce::Array<juce::MidiDeviceInfo> availableMidiOutputs;

	// Forced Dual Stack for maximum compatibility (WMS/UWP and WinMM)
    std::unique_ptr<rt::midi::RtMidiIn> rtMidiInUWP;    // Per Multi-Client (WMS/UWP)
    std::unique_ptr<rt::midi::RtMidiIn> rtMidiInLegacy; // Per loopMIDI (WinMM)

    // Static callback for RtMidi
    static void midiCallback(double deltatime, std::vector<unsigned char>* message, void* userData);

    // Port initialization method
    void setupRtMidi();

    std::atomic<float> midiSpeed{ 1.0f };       // Memorizza la velocità in modo sicuro
    std::atomic<float> midiProbability{ 0.5f }; // Memorizza la probabilità in modo sicuro
    juce::CriticalSection midiCallbackLock;     // Protegge dati non atomici se necessario

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkaldProcessor)
};
