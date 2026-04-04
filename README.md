README

<img src="./jbg2fcs2.png"
style="width:7.14722in;height:4.75278in" />

The idea of this fork was born from the desire to fix some aspects of
the standalone GUI (buttons, boxes and labels overlapping at the top
left) and therefore from the attempt to implement some development ideas
suggested by the author of the original repository (see the bottom of
the readme). 

New features:

> Added velocity selection for dot.
>
> Added gate-time selection for dot.
>
> Added channel midi output selection for dot
>
> Added midi port selection (on standalone).
>
> Added Hypereuclidean option: you can choose between the starting
> standard mode (the normal Skald MIDI Generative) or the Hypereuclidean
> pattern generation (pattern GENERATE button)
>
> Added Pulses and Steps to define how many strokes to distribute evenly
> in a number of subdivisions (e.g. 3 strokes over 8 steps)
>
> Fixed GUI bugs on standalone and made GUI resizable.
>
> ASIO & Jack support on Windows.

Here only for Windows x64, using CMake v4.2.0, Juce v8.0.12 and Visual
Studio 2026 Insiders on Windows 11 (but being a Juce project it can be
easily adapted for Mac and Linux).

MIDI Channel, Velocity, Gate Time and in-ring position change modes:

--> Click on a single dot

--> SHIFT + Vertical Drag: Changes the individual point's velocity.

--> CTRL/CMD + Drag: Adjusts Gate Time (how long that specific note
stays "pressed)

--> ALT + Vertical Drag: Changes the individual point's MIDI channel.

--> Drag Only: Changes the point's position on the disk.

<img src="./kbjctot3.png"
style="width:6.07361in;height:5.61806in" />

The "Help" button has been radically changed: previously, it opened a fixed screen with a list of
features and instructions for use. Since more functions and buttons have
been added, clicking "Help" now opens a PDF manual. However, it is
important that the manual be located in the same folder as the plugin or
executable (for standalone versions) and that the file name
"Skald_Manual.pdf" is not changed.

Original “TODO” ideas List (marked with an x what has been done here):

X   Multiple MIDI channels per dot (color-coded) 

X   Per-dot velocity

X   Per-dot gate time

X   Hypereuclidean pattern generator (advanced customized Euclidean) 
 
X   MIDI file export

> MIDI CC modulation per dot
>
> Pattern preset browser
>
> Multiple concurrent turntables
>
> Tempo-independent mode
> 

