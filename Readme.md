# IceBeats Music Visualizer
*"it's like a firebeat, but **cooler**"*

## Introduction
IceBeats is yet *another* Teensy sketch for real-time music visualization on an addressable LED strip.
It aims for a simple hardware setup that provides dynamic music visualizations.

This project is aimed as a (partial) IO replacement for dance game cabinets running Stepmania. Though tailored towards my cabinet specifically, it should be easy to adapt for other setups (or dancegame-less setups that only want music visualization).

It is designed around the Teensy Audio Library to run on the Teensy 3.2. Input is taken from an RCA audio jack, and visualized to a WS2812B LED strip.
Other Teensies that can run the audio library are likely compatible too (though the library is not fully working on the 4.0 at time of writing)

**Keep in mind this project is still a work in progress!**


## Hardware
 * Accepts audio input via RCA/mono 3.5mm jack
 * Designed around Teensy 3.2, other Audio-capable Teensy boards likely compatible
 * Output to WS2812B LED Strip via FastLED (other strip types likely compatible)
 * Blinks an external decorative light strand to the beat (designed to drive cheap two wire strands where alternate LEDs/colors are driven by reversing the strip polarity, using a L293D H-Bridge or similar)
 
## Visualization
 * FFT-based music visualization using the Teensy Audio Library
 * Multiple visualization effects and color palettes
 
## StepMania IO
 * Accepts input on digital IOs to simulate keystrokes
 * Drives cabinet/pad lighting via Serial (Reads data from Stepmania's SextetStream format, use either SextetStreamToFile on Linux or Win32Serial on Windows (if using SM5.3))
 
## Todo:
 * Planned: Multiple idle animations to play when there is no music
 * Planned: Drive two more LED strips for bass neon replacements
 * Planned: Cycle color palettes/visualizations
 * Todo: Everything else :(
