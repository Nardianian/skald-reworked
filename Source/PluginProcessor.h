#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_graphics/juce_graphics.h>
#include "EuclideanSet.h"

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

    // Struttura per gestire i parametri di generazione per ogni anello (Punto 1-4)
    struct RingSettings {
        bool isEuclidean = false; // Selettore modalità (Standard vs Euclideo)
        int pulses = 4;           // Numero di impulsi
        int steps = 16;           // Numero di step della griglia
        int depth = 1;            // Profondità Hyper-Euclidean
        int shift = 0;            // Rotazione/Shift del pattern
    };

    RingSettings ringSettings[12]; // Gestiamo fino a 12 anelli (corrispondenti alle note della scala)

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
    // Patch 6.5: Forziamo il numero di anelli a 7 (0=Scratch, 1-6=Note)
    // Questo garantisce che la grafica del Editor e la logica del Processor siano sempre allineate.
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

    // Getter per parametri algoritmici
    int getLastPulses() const { return lastPulses; }
    int getLastSteps() const { return lastSteps; }
    int getLastDepth() const { return lastDepth; }
    int getLastShift() const { return lastShift; }

    // Standalone transport control
    void setPlaying(bool shouldPlay) { isPlayingStandalone = shouldPlay; }
    bool isPlaying() const { return isPlayingStandalone; }

    // BPM control for standalone
    void setStandaloneBPM(double bpm) { standaloneBPM = bpm; }
    double getStandaloneBPM() const { return standaloneBPM; }

    // Get effective BPM (standalone or host)
    double getBPM() const { return hostBPM; }

    // Preview note triggering (for UI feedback)
    void triggerPreviewNote(int ringIndex);

    // Get recently triggered dots for visual feedback
    std::vector<TriggeredDotInfo> getRecentlyTriggeredDots() const;

    void generateHyperPattern(int ringIndex, int pulses, int steps, int depth, int rotation);

    // Patch: Funzione refresh per accesso dall'Editor
    void refreshMidiOutputs();

    // Patch: Gestione MIDI Standalone
    void changeMidiPort(int index);
    juce::Array<juce::MidiDeviceInfo>& getAvailableMidiOutputs() { return availableMidiOutputs; }
    int getSelectedMidiPortIndex() const { return standaloneMidiOut.selectedPortIndex; }

    // Gestore dispositivi per forzare il MIDI in Standalone
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
    bool motorRunning = true;           // Motor on/off state
    float currentSpeedMultiplier = 1.0f; // Current speed (ramps up/down like record player)

    // Scratching/manual control
    float scratchVelocity = 0.0f;       // Angular velocity from scratching (degrees per second)
    bool isBeingScratched = false;      // True when user is actively scratching

    // High-value parameters
    float probability = 100.0f;     // Probability of note trigger (0-100%)
    float velocityVariation = 0.0f; // Velocity randomization amount (0-100%)
    float swing = 0.0f;             // Swing amount (0-100%)

    // --- Memoria parametri algoritmici (Patch 35) ---
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

    // Track swing state (which beat we're on for swing timing)
    int swingBeatCounter = 0;

    // --- Gestione MIDI Manuale ---
    struct CustomMidiOut
    {
        std::unique_ptr<juce::MidiOutput> output;
        int selectedPortIndex = -1;
    };
    CustomMidiOut standaloneMidiOut;
    juce::Array<juce::MidiDeviceInfo> availableMidiOutputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkaldProcessor)
};
