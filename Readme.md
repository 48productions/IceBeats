# IceBeats Music Visualizer
*"it's like a firebeat, but **cooler**"*

## Introduction
IceBeats gives you real-time music visualization on an addressable LED strip.

The hardware setup is dirt simple - Just grab a Teensy 3.2, an LED strip, and maybe a few basic components. No bloated bluetooth/wifi control or complex setup, just pure RGB goodness (well, kinda).


Instead of bloated bluetooth/wifi control, you get a TON of 100% optional bloat instead! If you're planning on throwing this on a StepMania cabinet (like I am), this code can optionally drive your cabinet lights as well. Input is also "supported".


**Keep in mind this project is still a work in progress!**


## Lighting Hardware
Code support for these are built-in - all 99% configurable, use any you like!

 * Visualization strip - An addressable RGB LED strip for visualizations. Designed for 60-90 leds, more can be configured but may result in performance drops.
 * Bass light strand - Designed for low voltage two-wire LED string lights, blinks to the beat.
    * Designed for two-wire strands where alternate LEDs/colors are driven by reversing the strip polarity using a L293D H-Bridge, single-color strands can be used too!
 * Bass LED strip - Another addressable RGB LED strip, blinks to the beat (automagically switches to SM control, if available)

## Electronics Hardware
 * Accepts audio input via RCA/mono 3.5mm jack (or any other input Teensy's audio library supports, like USB, through easy mods)
 * Designed around Teensy 3.2, other Audio-capable Teensy boards likely compatible (The 4.0 doesn't look to be 100% compatible at time of writing)
 
## Visualization
 * FFT-based music visualization using the Teensy Audio Library
 * Multiple visualization effects and optional dynamic, music-based color palettes
 * Idle animations for when no music is playing
 
## StepMania IO
 * Drives StepMania cabinet/pad lighting (reads the standard SextetStream format via serial, configure either SextetStreamToFile (Linux) or Win32Serial (Windows, SM5.3). Other, non-Stepmania programs are untested but probably work.
 * Can send input via keystrokes (currently disabled but configurable, this is NOT your ideal input board though - you really should go buy a separate board instead :P )
 
## Todo:
 * Todo: A lot, this is a WIP :(

## License
I'd greatly apprecate it if you credit/link back to this repo if you use this code somewhere - this thing took a ton of time to make.

That being said,

¯\_(ツ)_/¯