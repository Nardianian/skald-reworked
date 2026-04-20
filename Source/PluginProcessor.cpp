#include <objbase.h>          // Required For CoInitializeEx
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_devices/juce_audio_devices.h>

#ifdef JUCE_WINDOWS
#include <wrl/client.h>     //For ComPtr (if used)
#include <roapi.h>          // For WinRT interfaces
#endif

//==============================================================================
SkaldProcessor::SkaldProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
    )
{
    isMidiLearning = false;
    lastParamToLearn = ""; // It must be empty at the beginning!

    // Force Multi-Threaded COM initialization for the entire process.
    // COINIT_MULTITHREADED is specifically required by WinRT MIDI.
    static HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // A little breather to allow drivers to respond
    juce::Thread::sleep(150);
    // ---------------------------------------------
    // Initialization of algorithmic parameters 
    lastPulses = 4;
    lastSteps = 16;
    lastDepth = 1;
    lastShift = 0;

    // Initialize scale
    updateScaleNotes();

    // --- Standalone: ​​Initializing Device Manager ---
    if (juce::JUCEApplication::isStandaloneApp())
    {
        // Force initialization for MIDI permissions on Windows
        deviceManager.initialise(0, 0, nullptr, true, {}, nullptr);
        deviceManager.addMidiInputDeviceCallback({}, nullptr);
    }

    // --- Standalone: ​​Sequential Initialization ---
    if (juce::JUCEApplication::isStandaloneApp())
    {
        // Gives a short break (50ms) to allow COM/WinRT threads 
        // initialized by deviceManager.initialise to settle down.
        juce::Thread::sleep(50);
    }

    // Perform the first scan
    refreshMidiOutputs();

    // If it doesn't find anything (only 0 to 1 devices), WinRT is probably still enumerating
    if (availableMidiOutputs.size() <= 1)
    {
        // Increase wait time to 500ms: WinRT on Windows 11 can be very slow to boot.
        juce::Thread::sleep(500);
        refreshMidiOutputs();
    }

    if (juce::JUCEApplication::isStandaloneApp() && !availableMidiOutputs.isEmpty())
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        // Note: In WinRT, the identifier is more reliable than the name for comparison.
        juce::String lastMidiPort = setup.outputDeviceName;

        int targetIndex = 0;
        for (int i = 0; i < availableMidiOutputs.size(); ++i)
        {
            if (availableMidiOutputs[i].name == lastMidiPort)
            {
                targetIndex = i;
                break;
            }
        }
        changeMidiPort(targetIndex);
    }

    // Start with a simple pentatonic melody pattern
    dots.clear();
    addDot(0.0f, 0, juce::Colour(0xffff6b35));      // Root
    addDot(90.0f, 2, juce::Colour(0xffff6b35));     // 3rd note
    addDot(180.0f, 4, juce::Colour(0xffff6b35));    // 5th note
    addDot(270.0f, 2, juce::Colour(0xffff6b35));    // 3rd note

    // Initialize RtMidi (Multi-client) for Standalone
    if (juce::JUCEApplication::isStandaloneApp())
    {
        setupRtMidi();
    }
}

SkaldProcessor::~SkaldProcessor()
{
    // 1. Reset RtMidi instances (stops callback threads before cleaning up the rest)
    rtMidiInUWP.reset();
    rtMidiInLegacy.reset();

    // 2. Stop standalone thread if active
    if (standaloneMidiOut.output != nullptr)
    {
        standaloneMidiOut.output->stopBackgroundThread();
        standaloneMidiOut.output.reset();
    }
    // 3. Clear the device list (force release of WinRT objects)
    availableMidiOutputs.clear();

    // 4. Optional: If you use JUCE's deviceManager
    deviceManager.removeMidiInputDeviceCallback({}, nullptr);
}

//==============================================================================
const juce::String SkaldProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SkaldProcessor::acceptsMidi() const
{
    return true;  // Must accept MIDI for Ableton to recognize as Instrument
}

bool SkaldProcessor::producesMidi() const
{
    return true;
}

bool SkaldProcessor::isMidiEffect() const
{
    return false;  // Must be false for Ableton Live compatibility
}

double SkaldProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SkaldProcessor::getNumPrograms()
{
    return 1;
}

int SkaldProcessor::getCurrentProgram()
{
    return 0;
}

void SkaldProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String SkaldProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void SkaldProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void SkaldProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    triggeredThisRotation.resize(dots.size(), false);

    // Refresh MIDI in Standalone mode
    if (juce::JUCEApplication::isStandaloneApp())
    {
        if (standaloneMidiOut.output == nullptr)
        {
            refreshMidiOutputs();
            if (!availableMidiOutputs.isEmpty())
            {
                int savedIndex = standaloneMidiOut.selectedPortIndex;
                changeMidiPort(savedIndex >= 0 ? savedIndex : 0);
            }
        }
    }
}

void SkaldProcessor::releaseResources()
{
}

bool SkaldProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SkaldProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    buffer.clear();

        // --- MIDI LEARN LISTENER ---
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isController())
        {
            int cc = msg.getControllerNumber();
            int val = msg.getControllerValue();

            if (isMidiLearning && lastParamToLearn.isNotEmpty())
            {
                // Assign the received CC to the selected parameter
                midiMappings[lastParamToLearn] = cc;

                // FUNDAMENTAL RESET: Stops learning and clears ID
                isMidiLearning = false;
                lastParamToLearn = "";
            }
            else
            {
                // Normal mode: Check if the CC is already mapped
                for (auto const& [paramID, mappedCC] : midiMappings)
                {
                    if (cc == mappedCC)
                        applyMidiControl(paramID, val);
                }
            }
        }
    }

    // Process active note-offs first (notes that should end in this buffer)
    std::vector<ActiveNote> notesToKeep;
    for (const auto& note : activeNotes)
    {
        juce::int64 noteOffInBuffer = note.noteOffSample - totalSamplesProcessed;

        if (noteOffInBuffer < buffer.getNumSamples() && noteOffInBuffer >= 0)
        {
            // Note-off occurs in this buffer
            juce::MidiMessage noteOff = juce::MidiMessage::noteOff(note.channel, note.midiNote, (juce::uint8)0);
            midiMessages.addEvent(noteOff, static_cast<int>(noteOffInBuffer));
            // Standalone: Manual Send for Note Off
            if (standaloneMidiOut.output != nullptr)
                standaloneMidiOut.output->sendMessageNow(noteOff);
        }
        else if (noteOffInBuffer >= buffer.getNumSamples())
        {
            // Note continues into future buffers
            notesToKeep.push_back(note);
        }
        // If noteOffInBuffer < 0, note already ended - discard it
    }
    activeNotes = notesToKeep;

    // Send any queued preview notes
    {
        juce::ScopedLock lock(previewNotesLock);
        for (const auto& previewNote : previewNotesToSend)
        {
            // Use the value transported in the timeStamp field as velocity
            int pVel = juce::jlimit(1, 127, previewNote.timeStamp);
            juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, previewNote.midiNote, (juce::uint8)pVel);
            midiMessages.addEvent(noteOn, 0);
            // Manual submission in standalone mode for Preview
            if (standaloneMidiOut.output != nullptr)
                standaloneMidiOut.output->sendMessageNow(noteOn);

            // Schedule note-off for preview (100ms)
            juce::int64 noteOffSample = totalSamplesProcessed + static_cast<juce::int64>(sampleRate * 0.1);
            activeNotes.push_back({ previewNote.midiNote, 1, noteOffSample });
        }
        previewNotesToSend.clear();
    }

    // Determine if we're playing and what BPM to use
    bool shouldPlay = isPlayingStandalone;  // Default to standalone state
    double currentBPM = standaloneBPM;

    // Get BPM and play state from host (overrides standalone if available)
    if (auto* playHead = getPlayHead())
    {
        if (auto positionInfo = playHead->getPosition())
        {
            if (positionInfo->getBpm().hasValue())
            {
                currentBPM = *positionInfo->getBpm();
            }

            // If host is providing play state, use it
            if (positionInfo->getIsPlaying())
            {
                shouldPlay = true;
            }
        }
    }

    // Record player-style motor control: ramp up/down speed
    // Motor only runs when BOTH DAW transport is playing AND motor toggle is ON
    const float rampUpRate = 3.0f;    // Fast start (reaches full speed in ~0.33 seconds)
    const float rampDownRate = 0.4f;  // Gradual stop (comes to halt in ~2.5 seconds)
    const float rampStep = 1.0f / sampleRate;  // Per-sample increment

    if (motorRunning && shouldPlay)  // BOTH conditions must be true
    {
        // Ramp up to full speed
        if (currentSpeedMultiplier < 1.0f)
            currentSpeedMultiplier = juce::jmin(1.0f, currentSpeedMultiplier + (rampUpRate * rampStep * buffer.getNumSamples()));
    }
    else
    {
        // Ramp down to stop (if either motor is OFF or DAW is stopped)
        if (currentSpeedMultiplier > 0.0f)
            currentSpeedMultiplier = juce::jmax(0.0f, currentSpeedMultiplier - (rampDownRate * rampStep * buffer.getNumSamples()));
    }

    // Scratching physics: apply friction to scratch velocity (like motor slowdown)
    if (!isBeingScratched && std::abs(scratchVelocity) > 0.01f)
    {
        // Apply friction to slow down the "thrown" turntable
        // Use same decay rate as motor ramp-down for consistent feel
        const float scratchDecayRate = 0.4f;  // Same as motor ramp down
        const float decayStep = scratchDecayRate / sampleRate;
        float velocityReduction = decayStep * buffer.getNumSamples();

        // Reduce velocity toward zero
        if (scratchVelocity > 0.0f)
            scratchVelocity = juce::jmax(0.0f, scratchVelocity - (std::abs(scratchVelocity) * velocityReduction));
        else
            scratchVelocity = juce::jmin(0.0f, scratchVelocity + (std::abs(scratchVelocity) * velocityReduction));

        // Stop completely if velocity is very small
        if (std::abs(scratchVelocity) < 0.1f)
            scratchVelocity = 0.0f;
    }

    // Apply scratch momentum (when not being actively scratched but has velocity)
    float previousRotation = currentRotation;
    if (!isBeingScratched && std::abs(scratchVelocity) > 0.01f)
    {
        // Apply scratch velocity
        float scratchIncrement = scratchVelocity * (buffer.getNumSamples() / sampleRate);
        currentRotation += scratchIncrement;

        // Wrap around and reset triggers when completing a rotation
        if (currentRotation < 0.0f)
        {
            currentRotation += 360.0f;
            // Reset trigger tracking when we complete a rotation (backward)
            std::fill(triggeredThisRotation.begin(), triggeredThisRotation.end(), false);
        }
        else if (currentRotation >= 360.0f)
        {
            currentRotation = std::fmod(currentRotation, 360.0f);
            // Reset trigger tracking when we complete a rotation (forward)
            std::fill(triggeredThisRotation.begin(), triggeredThisRotation.end(), false);
        }
    }
    // Only advance rotation if playing (or if motor is spinning down) and NOT being scratched or thrown
    // Only use motor rotation when scratch velocity is zero to avoid interference
    else if (!isBeingScratched && std::abs(scratchVelocity) < 0.01f && (shouldPlay || currentSpeedMultiplier > 0.0f))
    {
        // Calculate rotation increment based on BPM and speed
        // One full rotation per 2 bars (8 beats) at normal speed
        // Apply reverse if enabled
        double beatsPerSecond = currentBPM / 60.0;
        float currentMidiSpeed = midiSpeed.load(); // Reads the atomic value from RtMidi
        double effectiveSpeed = isReversed ? -currentMidiSpeed : currentMidiSpeed;
        effectiveSpeed *= currentSpeedMultiplier;  // Apply motor ramp
        double rotationsPerSecond = (beatsPerSecond / 8.0) * effectiveSpeed;
        double degreesPerSample = (rotationsPerSecond * 360.0) / sampleRate;
        float rotationIncrement = static_cast<float>(degreesPerSample * buffer.getNumSamples());

        currentRotation += rotationIncrement;

        // Wrap around and reset triggers when completing a rotation (both directions)
        if (currentRotation < 0.0f)
        {
            currentRotation += 360.0f;
            // Reset trigger tracking when we complete a rotation (backward/reverse)
            std::fill(triggeredThisRotation.begin(), triggeredThisRotation.end(), false);
        }
        else if (currentRotation >= 360.0f)
        {
            currentRotation = std::fmod(currentRotation, 360.0f);
            // Reset trigger tracking when we complete a rotation (forward)
            std::fill(triggeredThisRotation.begin(), triggeredThisRotation.end(), false);
        }
    }

    // Note triggering: Check each dot to see if we've crossed its angle
    // This works for normal playback, scratching, and scratch momentum
    if (previousRotation != currentRotation)
    {
        // Check each dot to see if we've crossed its angle
        for (size_t i = 0; i < dots.size(); ++i)
        {
            if (!dots[i].active)
                continue;

            // Calculate the trigger angle (when dot is at top/under sensor)
            // Visual angle 0° = top (sensor arm position)
            // Trigger when: (dot.angle - currentRotation) = 0°
            // Which means: currentRotation = dot.angle
            float triggerAngle = dots[i].angle;

            // Check if we've crossed the trigger angle (works for both directions)
            // Add small tolerance to handle floating-point precision and very small movements
            bool crossed = false;
            float rotationDelta = currentRotation - previousRotation;
            const float tolerance = 0.5f;  // 0.5 degree tolerance for edge cases

            // Handle wrap-around
            if (rotationDelta > 180.0f)
                rotationDelta -= 360.0f;
            else if (rotationDelta < -180.0f)
                rotationDelta += 360.0f;

            if (std::abs(rotationDelta) < 0.001f)
            {
                // No movement - skip crossing check
                continue;
            }
            else if (rotationDelta > 0.0f)
            {
                // Forward rotation (clockwise)
                float diff = triggerAngle - previousRotation;
                if (diff < 0.0f) diff += 360.0f;
                // Add tolerance: allow trigger if within range + tolerance
                crossed = (diff >= -tolerance && diff <= rotationDelta + tolerance);
            }
            else if (rotationDelta < 0.0f)
            {
                // Reverse rotation (counter-clockwise)
                float diff = previousRotation - triggerAngle;
                if (diff < 0.0f) diff += 360.0f;
                // Add tolerance: allow trigger if within range + tolerance
                crossed = (diff >= -tolerance && diff <= -rotationDelta + tolerance);
            }

            // Trigger MIDI note if we crossed this dot and haven't triggered it yet
            if (crossed && !triggeredThisRotation[i])
            {
                // Apply probability - check if this note should trigger
                float probRoll = random.nextFloat() * 100.0f;
                float currentMidiProb = midiProbability.load(); // Reads the atomic value from RtMidi
                bool passedProbability = probRoll <= currentMidiProb;

                if (!passedProbability)
                {
                    // Track the dot pass but mark as not triggered for visual feedback
                    {
                        juce::ScopedLock lock(triggeredDotsLock);
                        auto currentTime = juce::Time::currentTimeMillis();
                        recentlyTriggeredDots.push_back({
                            static_cast<int>(i),
                            currentTime,
                            0,              // velocity (not used when not triggered)
                            0.0f,           // gateTimeMs (not used when not triggered)
                            false,          // wasTriggered = false
                            swingBeatCounter
                            });

                        // Clean up old entries (older than 1000ms to accommodate gate times)
                        recentlyTriggeredDots.erase(
                            std::remove_if(recentlyTriggeredDots.begin(), recentlyTriggeredDots.end(),
                                [currentTime](const TriggeredDotInfo& entry) {
                                    return (currentTime - entry.timestamp) > 1000;
                                }),
                            recentlyTriggeredDots.end()
                        );
                    }

                    triggeredThisRotation[i] = true; // Mark as triggered even if skipped
                    continue; // Skip this note
                }

                // Calculate exactly when in the block the crossing occurred
                int triggerSample = 0;

                if (previousRotation < currentRotation)
                {
                    // Normal case: calculate how far through the block the crossing happened
                    float rotationRange = currentRotation - previousRotation;
                    float rotationToCrossing = triggerAngle - previousRotation;

                    if (rotationRange > 0.0f)
                    {
                        float fraction = rotationToCrossing / rotationRange;
                        triggerSample = static_cast<int>(fraction * buffer.getNumSamples());
                        triggerSample = juce::jlimit(0, buffer.getNumSamples() - 1, triggerSample);
                    }
                }
                // For wrap-around case, trigger at start of block for simplicity

                // Apply swing timing based on beat position in rotation
                // One full rotation = 8 beats, so calculate which beat this note falls on
                swingBeatCounter++;
                if (swing > 0.0f)
                {
                    // Calculate which 16th note subdivision this trigger falls on (0-31)
                    // One rotation = 8 beats = 32 sixteenth notes
                    float rotationProgress = currentRotation / 360.0f;  // 0.0 to 1.0
                    int sixteenthNote = static_cast<int>(rotationProgress * 32.0f) % 32;

                    // Apply swing to every other 16th note (odd numbered ones)
                    // This creates the classic "long-short" swing pattern
                    if (sixteenthNote % 2 == 1)
                    {
                        // Calculate tempo-relative swing delay
                        // At 100% swing: delay by full 16th note (dramatic swing)
                        // At 66% swing: classic jazz triplet feel
                        // At 50% swing: no swing (straight)
                        double secondsPerBeat = 60.0 / currentBPM;
                        double sixteenthNoteDuration = secondsPerBeat / 4.0;  // 16th note subdivision

                        // Swing percentage maps to delay amount:
                        // 50% = no delay (straight), 66% = triplet feel, 100% = full 16th delay
                        float swingRatio = (swing / 100.0f);  // 0.0 to 1.0
                        float delayRatio = (swingRatio - 0.5f) * 2.0f;  // -1.0 to 1.0, centered at 0.5 (50%)
                        delayRatio = juce::jlimit(0.0f, 1.0f, delayRatio);  // Clamp to 0.0-1.0

                        double swingDelaySec = sixteenthNoteDuration * delayRatio;
                        int swingOffset = static_cast<int>(swingDelaySec * sampleRate);
                        triggerSample = juce::jmin(buffer.getNumSamples() - 1, triggerSample + swingOffset);
                    }
                }

                // Get MIDI note from ring index based on current scale
                int midiNote = ringToMidiNote(dots[i].ringIndex);

                // Calculate velocity with individual dot base
                int finalVelocity = dots[i].velocity;
                if (velocityVariation > 0.0f)
                {
                    // Add random variation based on velocityVariation parameter
                    float variation = (random.nextFloat() * 2.0f - 1.0f) * (velocityVariation / 100.0f);
                    finalVelocity = static_cast<int>(dots[i].velocity * (1.0f + variation * 0.5f));
                    finalVelocity = juce::jlimit(1, 127, finalVelocity);
                }

                // --- Individual MIDI and Gate Management ---
                juce::MidiMessage noteOn = juce::MidiMessage::noteOn(dots[i].midiChannel, midiNote, (juce::uint8)finalVelocity);
                midiMessages.addEvent(noteOn, triggerSample);

                // Standalone: ​​Send Note ON immediately
                if (standaloneMidiOut.output != nullptr)
                    standaloneMidiOut.output->sendMessageNow(noteOn);

                // --- Correct Gate Time Conversion (ms to seconds) ---
                double individualGateSeconds = static_cast<double>(dots[i].gateTime) / 1000.0;

                juce::int64 durationSamples = static_cast<juce::int64>(sampleRate * individualGateSeconds);
                juce::int64 absoluteNoteOffSample = totalSamplesProcessed + triggerSample + durationSamples;

                // Adds to the list of active notes for future Note Off
                activeNotes.push_back({ midiNote, dots[i].midiChannel, absoluteNoteOffSample });

                triggeredThisRotation[i] = true;

                // Track this dot for visual feedback with full parameter info
                {
                    juce::ScopedLock lock(triggeredDotsLock);
                    auto currentTime = juce::Time::currentTimeMillis();
                    recentlyTriggeredDots.push_back({
                        static_cast<int>(i),
                        currentTime,
                        finalVelocity,      // Actual velocity after variation
                        dots[i].gateTime * 1000.0f,  // Individual gate time in Ms
                        true,               // wasTriggered = true
                        swingBeatCounter    // Beat counter for swing visualization
                        });

                    // Clean up old entries (older than 1000ms to accommodate gate times)
                    recentlyTriggeredDots.erase(
                        std::remove_if(recentlyTriggeredDots.begin(), recentlyTriggeredDots.end(),
                            [currentTime](const TriggeredDotInfo& entry) {
                                return (currentTime - entry.timestamp) > 1000;
                            }),
                        recentlyTriggeredDots.end()
                    );
                }
            }
        }
    }

    // Increment total samples processed for accurate note-off timing across buffers
    totalSamplesProcessed += buffer.getNumSamples();
}

//==============================================================================
bool SkaldProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SkaldProcessor::createEditor()
{
    return new SkaldEditor(*this);
}

//==============================================================================
void SkaldProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, false);

    stream.writeFloat(speed);
    stream.writeInt(static_cast<int>(currentScale));
    stream.writeInt(rootNote);
    stream.writeInt(static_cast<int>(dots.size()));

    for (const auto& dot : dots)
    {
        stream.writeFloat(dot.angle);
        stream.writeInt(dot.ringIndex);
        stream.writeInt(dot.color.getARGB());
        stream.writeBool(dot.active);
        stream.writeInt(dot.velocity);     
        stream.writeInt(dot.midiChannel);   
        stream.writeFloat(dot.gateTime);   // Individual Gate Rescue
    }

    stream.writeInt(globalVelocity);
    stream.writeFloat(gateTimeMs);
    stream.writeBool(isReversed);
    stream.writeFloat(probability);
    stream.writeFloat(velocityVariation);
    stream.writeFloat(swing);
    // --- Algorithmic Parameters ---
    stream.writeInt(lastPulses);
    stream.writeInt(lastSteps);
    stream.writeInt(lastDepth);
    stream.writeInt(lastShift);
    // Saving MIDI Maps
    stream.writeInt((int)midiMappings.size());
    for (auto const& [paramID, cc] : midiMappings)
    {
        stream.writeString(paramID);
        stream.writeInt(cc);
    }
}

void SkaldProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);

    speed = stream.readFloat();
    currentScale = static_cast<ScaleType>(stream.readInt());
    rootNote = stream.readInt();
    updateScaleNotes();

    int numDots = stream.readInt();

    dots.clear();
    for (int i = 0; i < numDots; ++i)
    {
        TurntableDot dot;
        dot.angle = stream.readFloat();
        dot.ringIndex = stream.readInt();
        dot.color = juce::Colour(stream.readInt());
        dot.active = stream.readBool();
        dot.velocity = stream.readInt();       
        dot.midiChannel = stream.readInt();    
        dot.gateTime = stream.readFloat();    // Individual Gate Reset
        dots.push_back(dot);
    }

    // Synchronize the trigger tracker with the number of dots loaded
    triggeredThisRotation.assign(dots.size(), false);

    // Safe Read: Make sure the stream is not exhausted before reading the global parameters.
    if (!stream.isExhausted()) globalVelocity = stream.readInt();
    if (!stream.isExhausted()) gateTimeMs = stream.readFloat();
    if (!stream.isExhausted()) isReversed = stream.readBool();
    if (!stream.isExhausted()) probability = stream.readFloat();
    if (!stream.isExhausted()) velocityVariation = stream.readFloat();
    if (!stream.isExhausted()) swing = stream.readFloat();

    // --- Resetting Algorithmic Parameters ---
    if (!stream.isExhausted()) lastPulses = stream.readInt();
    if (!stream.isExhausted()) lastSteps = stream.readInt();
    if (!stream.isExhausted()) lastDepth = stream.readInt();
    if (!stream.isExhausted()) lastShift = stream.readInt();

        // MIDI Map Recovery (Safe Read with Integrity Check)
    if (stream.getNumBytesRemaining() >= sizeof(int))
    {
        int numMappings = stream.readInt();

        // Safety limit: Do not load more than 100 mappings to avoid buffer overflows
        if (numMappings > 0 && numMappings < 100)
        {
            midiMappings.clear();
            for (int i = 0; i < numMappings; ++i)
            {
                if (stream.isExhausted()) break;
                juce::String pID = stream.readString();
                int cc = stream.readInt();
                midiMappings[pID] = cc;
            }
        }
    }
}

//==============================================================================
void SkaldProcessor::addDot(float angle, int ringIndex, juce::Colour color)
{
    TurntableDot dot;
    dot.angle = angle;
    dot.ringIndex = ringIndex;
    dot.color = color;
    dot.active = true;
    dots.push_back(dot);
    triggeredThisRotation.resize(dots.size(), false);
}

void SkaldProcessor::removeDot(int index)
{
    if (index >= 0 && index < static_cast<int>(dots.size()))
    {
        dots.erase(dots.begin() + index);
        triggeredThisRotation.resize(dots.size(), false);
    }
}

void SkaldProcessor::clearAllDots()
{
    dots.clear();
    triggeredThisRotation.clear();
}

//==============================================================================
// Scale system implementation

std::vector<int> SkaldProcessor::getScaleIntervals(ScaleType scale)
{
    switch (scale)
    {
    case ScaleType::Major:           return { 0, 2, 4, 5, 7, 9, 11, 12 };
    case ScaleType::Minor:           return { 0, 2, 3, 5, 7, 8, 10, 12 };
    case ScaleType::HarmonicMinor:   return { 0, 2, 3, 5, 7, 8, 11, 12 };
    case ScaleType::MelodicMinor:    return { 0, 2, 3, 5, 7, 9, 11, 12 };
    case ScaleType::Pentatonic:      return { 0, 2, 4, 7, 9, 12 };
    case ScaleType::PentatonicMinor: return { 0, 3, 5, 7, 10, 12 };
    case ScaleType::Blues:           return { 0, 3, 5, 6, 7, 10, 12 };
    case ScaleType::Dorian:          return { 0, 2, 3, 5, 7, 9, 10, 12 };
    case ScaleType::Phrygian:        return { 0, 1, 3, 5, 7, 8, 10, 12 };
    case ScaleType::Lydian:          return { 0, 2, 4, 6, 7, 9, 11, 12 };
    case ScaleType::Mixolydian:      return { 0, 2, 4, 5, 7, 9, 10, 12 };
    case ScaleType::Locrian:         return { 0, 1, 3, 5, 6, 8, 10, 12 };
    case ScaleType::Chromatic:       return { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
    default:                         return { 0, 2, 4, 7, 9, 12 };
    }
}

void SkaldProcessor::updateScaleNotes()
{
    scaleNotes.clear();
    auto intervals = getScaleIntervals(currentScale);

    // Create single octave of the scale
    int baseMIDI = rootNote + (baseOctave * 12);
    for (int interval : intervals)
    {
        // Skip the octave repeat (12) to keep it to one octave
        if (interval == 12) continue;
        scaleNotes.push_back(baseMIDI + interval);
    }
}

void SkaldProcessor::setScale(ScaleType newScale)
{
    currentScale = newScale;
    updateScaleNotes();
}

void SkaldProcessor::setRootNote(int newRoot)
{
    rootNote = juce::jlimit(0, 11, newRoot);
    updateScaleNotes();
}

void SkaldProcessor::setOctaveShift(int shift)
{
    octaveShift = juce::jlimit(-2, 2, shift);
}

int SkaldProcessor::ringToMidiNote(int ringIndex) const
{
    if (ringIndex >= 0 && ringIndex < static_cast<int>(scaleNotes.size()))
    {
        int baseNote = scaleNotes[ringIndex];
        return baseNote + (octaveShift * 12); // Apply octave shift
    }
    return 60; // Default to middle C
}

//==============================================================================
// Preview note triggering
void SkaldProcessor::triggerPreviewNote(int ringIndex)
{
    int midiNote = ringToMidiNote(ringIndex);
    int previewVel = globalVelocity;

    if (auto* editor = dynamic_cast<SkaldEditor*>(getActiveEditor()))
    {
        int selectedIdx = editor->getSelectedDotIndex();
        if (selectedIdx >= 0 && selectedIdx < static_cast<int>(dots.size()))
            previewVel = dots[selectedIdx].velocity;
    }

    juce::ScopedLock lock(previewNotesLock);
    PreviewNote preview;
    preview.midiNote = midiNote;
    preview.timeStamp = previewVel; // Trasportiamo la velocity qui
    previewNotesToSend.push_back(preview);
}

//==============================================================================
// Get recently triggered dots for visual feedback
std::vector<SkaldProcessor::TriggeredDotInfo> SkaldProcessor::getRecentlyTriggeredDots() const
{
    juce::ScopedLock lock(triggeredDotsLock);

    // Return all recently triggered dots (cleanup happens in processBlock)
    return recentlyTriggeredDots;
}

// --- HyperEuclidean Generator ---
void SkaldProcessor::generateHyperPattern(int ringIndex, int pulses, int steps, int depth, int rotation)
{
    // Limit 7 rings (0 = Scratch, 1-6 = Dots)
    if (ringIndex < 1 || ringIndex > 6)
        return;

    // Update memory for saving
    lastPulses = pulses;
    lastSteps = steps;
    lastDepth = depth;
    lastShift = rotation;

    // 1. Instance with HyperEuclidean logic (depth > 1 creates jazz-rock or broken rhythmic substructures)
    HyperEuclidean generator(pulses, steps, depth);
    auto pattern = generator.generateSequence();
    auto vels = generator.velocities;

    // 2. Cleaning of the specific ring
    dots.erase(std::remove_if(dots.begin(), dots.end(),
        [ringIndex](const TurntableDot& d) { return d.ringIndex == ringIndex; }),
        dots.end());

    if (steps <= 0) return;
    float angleStep = 360.0f / static_cast<float>(steps);

    // 3. Generation with IOI Velocity (distant strokes = strong accents, close strokes = ghost notes)
    for (int i = 0; i < (int)pattern.size(); ++i)
    {
        if (pattern[i] == 1)
        {
            TurntableDot newDot;
            newDot.ringIndex = ringIndex;

            // Rotation for syncopations and rhythmic shifts
            float finalAngle = std::fmod((i * angleStep) + (rotation * angleStep), 360.0f);
            newDot.angle = finalAngle;

            // IOI Velocity: Logic in your .cpp assigns velocity based on space.
            // If vels[i] is 0, we assign a safety value
            newDot.velocity = (vels[i] > 0) ? vels[i] : 80;

            newDot.active = true;
            newDot.midiChannel = ringIndex + 1;
            newDot.color = juce::Colour::fromHSV(ringIndex * 0.15f, 0.8f, 0.9f, 1.0f);

            dots.push_back(newDot);
        }
    }
    triggeredThisRotation.resize(dots.size(), false);
} 
//=================================================
void SkaldProcessor::refreshMidiOutputs()
{
    availableMidiOutputs.clear();

    // With WinMM (WinRT=0 in CMake), this call is immediate and sees everything.
    auto devices = juce::MidiOutput::getAvailableDevices();

    if (devices.isEmpty()) {
        DBG("No devices found. Checking for 250ms delay...");
        juce::Thread::sleep(250);
        devices = juce::MidiOutput::getAvailableDevices();
    }

    for (auto& d : devices)
    {
        // This DBG will tell you the name and ID 
        // If the ID starts with {....} or has a strange format, it's WinRT/WMS
        // If the ID is a number or a simple name, it's WinMM.
        DBG("Device: " << d.name << " | ID: " << d.identifier);
        availableMidiOutputs.add(d);
    }

    DBG("Total devices found: " << availableMidiOutputs.size());

    // Standalone persistence management
    if (juce::JUCEApplication::isStandaloneApp() && standaloneMidiOut.output != nullptr)
    {
        auto currentInfo = standaloneMidiOut.output->getDeviceInfo();
        bool stillExists = false;
        for (const auto& d : availableMidiOutputs) {
            if (d.identifier == currentInfo.identifier) {
                stillExists = true;
                break;
            }
        }
        if (!stillExists) {
            standaloneMidiOut.output->stopBackgroundThread();
            standaloneMidiOut.output = nullptr;
        }
    }
}

void SkaldProcessor::changeMidiPort(int index)
{
    if (!juce::JUCEApplication::isStandaloneApp()) return;

    if (index >= 0 && index < availableMidiOutputs.size())
    {
        // Closes the previous door securely
        if (standaloneMidiOut.output != nullptr)
        {
            standaloneMidiOut.output->stopBackgroundThread();
            standaloneMidiOut.output = nullptr;
        }

        // Attempt to open the new device using a unique identifier
        standaloneMidiOut.output = juce::MidiOutput::openDevice(availableMidiOutputs[index].identifier);

        if (standaloneMidiOut.output != nullptr)
        {
            standaloneMidiOut.output->startBackgroundThread();
            standaloneMidiOut.selectedPortIndex = index;

            // Update DeviceManager to maintain persistence
            deviceManager.setDefaultMidiOutputDevice(availableMidiOutputs[index].identifier);
            DBG("Successfully opened MIDI port: " << availableMidiOutputs[index].name);
        }
    }
}

void SkaldProcessor::exportMidiFile(const juce::File& targetFile)
{
    juce::MidiFile midiFile;
    juce::MidiMessageSequence sequence;
    double ticksPerQuarter = 960.0;

    for (const auto& dot : dots)
    {
        if (!dot.active) continue;

        int midiNote = ringToMidiNote(dot.ringIndex);
        // Angle-based tempo calculation (0-360 mapped to 0-8 beats)
        double beatPosition = (dot.angle / 360.0) * 8.0;
        double timeInTicks = beatPosition * ticksPerQuarter;

        auto noteOn = juce::MidiMessage::noteOn(dot.midiChannel, midiNote, (juce::uint8)dot.velocity);
        noteOn.setTimeStamp(timeInTicks);

        auto noteOff = juce::MidiMessage::noteOff(dot.midiChannel, midiNote);
        noteOff.setTimeStamp(timeInTicks + (dot.gateTime * (ticksPerQuarter / 100.0)));

        sequence.addEvent(noteOn);
        sequence.addEvent(noteOff);
    }

    sequence.updateMatchedPairs();
    midiFile.setTicksPerQuarterNote(static_cast<int>(ticksPerQuarter));
    midiFile.addTrack(sequence);

    if (auto outStream = targetFile.createOutputStream())
        midiFile.writeTo(*outStream);
}

void SkaldProcessor::applyMidiControl(const juce::String& paramID, int value)
{
    // A) Continuous Controls (Linear Mapping 0-127)
    if (paramID == "speed") setSpeed(juce::jmap((float)value, 0.0f, 127.0f, 0.1f, 4.0f));
    else if (paramID == "tempo") setStandaloneBPM(juce::jmap((double)value, 0.0, 127.0, 40.0, 240.0));
    else if (paramID == "prob") setProbability((float)value / 127.0f * 100.0f);
    else if (paramID == "swing") setSwing((float)value / 127.0f * 100.0f);
    else if (paramID == "vel") setGlobalVelocity(value == 0 ? 1 : value);
    else if (paramID == "vvar") setVelocityVariation((float)value / 127.0f * 100.0f);
    else if (paramID == "gate") setGateTime(juce::jmap((float)value, 0.0f, 127.0f, 10.0f, 2000.0f));

    // Scratch via MIDI (Absolute position mapping 0-127 -> 0-360 degrees)
    else if (paramID == "scratch") {
        float newAngle = (float)value / 127.0f * 360.0f;
        currentRotation = newAngle; // Set the rotation directly
        // Reset triggers to prevent "playing" old notes while moving
        std::fill(triggeredThisRotation.begin(), triggeredThisRotation.end(), false);
    }

    // --- Algorithmic Controls via MIDI (Using Existing Variables) ---
    else if (paramID == "pulses") {
        lastPulses = juce::jlimit(0, 32, (int)juce::jmap((float)value, 0.0f, 127.0f, 0.0f, 32.0f));
        // With a ring selected, it updates and generates
        generateHyperPattern(0, lastPulses, lastSteps, lastDepth, lastShift);
    }
    else if (paramID == "steps") {
        lastSteps = juce::jlimit(1, 32, (int)juce::jmap((float)value, 0.0f, 127.0f, 1.0f, 32.0f));
        generateHyperPattern(0, lastPulses, lastSteps, lastDepth, lastShift);
    }
    else if (paramID == "shift") {
        lastShift = juce::jlimit(0, 32, (int)juce::jmap((float)value, 0.0f, 127.0f, 0.0f, 32.0f));
        generateHyperPattern(0, lastPulses, lastSteps, lastDepth, lastShift);
    }

    // B) Cyclic Selectors (Zoning)
    else if (paramID == "div") {
        int speedIdx = juce::jlimit(0, 4, (int)(value / 25.6f)); // 5 zone from 0.25x to 4x
        float speeds[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f };
        setSpeed(speeds[speedIdx]);
    }
    else if (paramID == "oct") {
        setOctaveShift(juce::jlimit(-2, 2, (int)(value / 25.6f) - 2));
    }

    // C) List Selectors (Slot zone)
    else if (paramID == "key") setRootNote(juce::jlimit(0, 11, (int)(value / 10.66f)));
    else if (paramID == "scale") setScale(static_cast<ScaleType>(juce::jlimit(0, 12, (int)(value / 9.84f))));
    else if (paramID == "h_depth") {
        // Mapping: CC 0-127 -> Values 1, 2, 3, 4
        lastDepth = juce::jlimit(1, 4, (int)(value / 32.0f) + 1);

        // Apply the change and regenerate the Euclidean pattern (default ring is 0)
        generateHyperPattern(0, lastPulses, lastSteps, lastDepth, lastShift);
    }

    // D) Switches (Toggle with threshold 64)
    else if (paramID == "rev") { if (value > 64) setReverse(!getReverse()); }
    else if (paramID == "motor") { if (value > 64) setMotorRunning(!getMotorRunning()); }
    else if (paramID == "play") { if (value > 64) setPlaying(!isPlaying()); }
}

void SkaldProcessor::setupRtMidi() {
    try {
        using namespace rt::midi;

        // 1. INSTANCE for MODERN standards (WMS and Bluetooth)
        rtMidiInUWP = std::make_unique<RtMidiIn>(RtMidi::WINDOWS_UWP);
        unsigned int uwpPorts = rtMidiInUWP->getPortCount();
        for (unsigned int i = 0; i < uwpPorts; ++i) {
            try {
                rtMidiInUWP->openPort(i);
                DBG("RtMidi UWP Opened: " << rtMidiInUWP->getPortName(i));
            }
            catch (...) { DBG("RtMidi UWP: Could not open port " << i); }
        }
        rtMidiInUWP->setCallback(&SkaldProcessor::midiCallback, this);
        rtMidiInUWP->ignoreTypes(false, false, false);

        // 2. INSTANCE for LEGACY standards (loopMIDI and 5-pin DIN Hardware)
        rtMidiInLegacy = std::make_unique<RtMidiIn>(RtMidi::WINDOWS_MM);
        unsigned int legacyPorts = rtMidiInLegacy->getPortCount();
        for (unsigned int i = 0; i < legacyPorts; ++i) {
            try {
                // Avoid opening Microsoft GS Wavetable virtual ports that create feedback
                juce::String pName = rtMidiInLegacy->getPortName(i);
                if (!pName.containsIgnoreCase("Microsoft GS")) {
                    rtMidiInLegacy->openPort(i);
                    DBG("RtMidi Legacy Opened: " << pName);
                }
            }
            catch (...) { DBG("RtMidi Legacy: Could not open port " << i); }
        }
        rtMidiInLegacy->setCallback(&SkaldProcessor::midiCallback, this);
        rtMidiInLegacy->ignoreTypes(false, false, false);

    }
    catch (rt::midi::RtMidiError& error) {
        DBG("RtMidi Critical Error: " + juce::String(error.getMessage()));
    }
}

// The callback is called in a separate high-priority thread
void SkaldProcessor::midiCallback(double deltatime, std::vector<unsigned char>* message, void* userData)
{
    auto* processor = static_cast<SkaldProcessor*>(userData);
    if (message->size() < 3) return;

    // Protects processor data access from the two RtMidi threads
    juce::ScopedLock sl(processor->midiCallbackLock);

    int status = message->at(0);
    int data1 = message->at(1);
    int data2 = message->at(2);

    if ((status & 0xF0) == 0xB0) { // MIDI CC
        int ccNumber = data1;
        int value = data2;

        if (processor->isMidiLearning && processor->lastParamToLearn.isNotEmpty()) {
            processor->midiMappings[processor->lastParamToLearn] = ccNumber;
            processor->isMidiLearning = false;
            processor->lastParamToLearn = "";
            return;
        }

        for (auto const& [paramID, mappedCC] : processor->midiMappings) {
            if (ccNumber == mappedCC) {
                processor->applyMidiControl(paramID, value);

                // Atomic updates for critical parameters in real time
                if (paramID == "speed")
                    processor->midiSpeed.store(juce::jmap((float)value, 0.0f, 127.0f, 0.1f, 4.0f));
                else if (paramID == "prob")
                    processor->midiProbability.store((float)value / 127.0f * 100.0f);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkaldProcessor();
}
