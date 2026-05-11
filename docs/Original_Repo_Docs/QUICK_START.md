# Skald - Quick Start Guide

## Getting Started with Your Viking MIDI Warrior

### What is Skald?

Skald is a generative MIDI sequencer inspired by Quintron's Drum Buddy and Playtonica MIDI Color Sequencer Orbita - mechanical rhythm machines that merge analog charm with hands-on performance. Place notes on concentric rings, scratch like vinyl, and explore generative patterns with motor control and probability.

---

## Installation

Your plugin has been automatically installed to:

### macOS
- **VST3**: `~/Library/Audio/Plug-Ins/VST3/Skald.vst3`
- **AU**: `~/Library/Audio/Plug-Ins/Components/Skald.component`
- **Standalone App**: `build/Skald_artefacts/Release/Standalone/Skald.app`

---

## How to Use in Your DAW

**IMPORTANT**: Skald is a MIDI generator, not an instrument. Follow these steps:

### Setup Steps

1. **Insert Skald on its own MIDI track** (leave track empty, no instruments)
2. **Create a separate MIDI track** with your synth/instrument
3. **Route MIDI from Skald's track** to your synth track (check DAW routing settings)
4. **Add dots and generate patterns!**

### DAW-Specific Instructions

#### Logic Pro
1. Create an empty **Software Instrument** track
2. In the MIDI FX slot, add **Audio Units > BeowulfAudio > Skald**
3. Create another track with your synth (Alchemy, ES2, etc.)
4. Route MIDI from Skald track to synth track
5. Press Play and add dots!

#### Ableton Live
1. Create a new MIDI track
2. Add **Skald.vst3** to the track
3. Create another MIDI track with your instrument
4. In Skald's track, set "MIDI To" to your instrument track
5. Press Play and create patterns!

#### Reaper
1. Create a new track and add **Skald.vst3**
2. Create another track with a VSTi synth
3. In Skald's track routing, send MIDI to synth track
4. Press Play and start sequencing!

---

## Interface Overview

### LED Displays (Top Left)
- **SCALE**: Tap to cycle through musical scales (Major, Minor, Pentatonic, Blues, etc.)
- **KEY**: Tap to change root note (C, C#, D, etc.)
- **DIV**: Tap to change speed/division (0.25x, 0.5x, 1x, 2x, 4x)
- **OCT**: Tap to shift octave range (-2, -1, 0, +1, +2)

### Knobs (Top Right)
- **PROB**: Probability - chance of notes triggering (0-100%)
- **SWING**: Groove timing - add swing to patterns
- **VEL**: Global velocity - note dynamics
- **VVAR**: Velocity Variation - humanize with random velocity
- **GATE**: Gate time - note length in milliseconds

### Turntable Controls
- **Double-click** anywhere on turntable to add/remove dots
- **Click and drag** dots to move them (change timing/pitch)
- **Click outer ring** to scratch - drag to spin, release for momentum
- **Sensor arm** (top) shows playback position

### Toggles (Bottom Left)
- **REV**: Reverse playback direction
- **MOTOR**: Toggle between motorized playback and manual mode

### Action Buttons (Bottom Right)
- **ADD**: Add random dot
- **RAND**: Randomize all dots
- **CLR**: Clear all dots
- **SAVE**: Save current pattern
- **LOAD**: Load saved pattern
- **HELP**: View this help information

---

## Quick Tips

### Creating Patterns
1. **Start simple**: Add 3-4 dots to hear the basic pattern
2. **Inner rings = lower notes**, outer rings = higher notes
3. **Spread dots around** the circle for rhythmic variation
4. **Use different rings** to create melodies

### Adding Expression
- **Probability**: Set to 80% for occasional missing notes
- **Velocity Variation**: Add 20-30% for human feel
- **Swing**: Try 60-66% for triplet/shuffle grooves
- **Reverse**: Flip direction mid-pattern for fills

### Performance Techniques
- **Scratching**: Click outer ring and drag for vinyl-style scratching
- **Motor Off**: Turn motor off for manual control
- **Speed Changes**: Tap DIV button live to change tempo divisions
- **Randomize**: Hit RAND for instant creative variations

---

## Troubleshooting

### No Sound?
- ✓ Check that Skald is on its own MIDI track (no instruments on it)
- ✓ Make sure you have a synth on a separate track
- ✓ Verify MIDI routing from Skald to synth track
- ✓ Check that your DAW transport is playing
- ✓ Make sure MOTOR toggle is ON (lit up)

### Notes Not Triggering?
- ✓ Check PROB (Probability) knob - set to 100% for testing
- ✓ Make sure dots are visible on turntable
- ✓ Verify sensor arm is rotating (motor is on)

### Plugin Not Showing Up?
- Rescan plugins in your DAW preferences
- Check installation locations listed above
- Restart your DAW after installation

---

## Next Steps

Now that you're set up:

1. **Experiment with scales** - Try Pentatonic, Blues, or Dorian
2. **Layer multiple instances** - Run Skald on multiple tracks with different synths
3. **Record MIDI** - Record the output to edit in your DAW
4. **Save patterns** - Use SAVE/LOAD to build a library
5. **Get creative** - Try extreme settings and happy accidents!

---

**Built by Beowulf Audio | v1.0.0**

Inspired by mechanical rhythm machines, built for modern music makers.

🎵 **Have fun and make something unique!** 🎵
